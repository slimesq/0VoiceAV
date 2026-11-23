#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "SDL.h"
#include "SDL_thread.h"
#ifdef __cplusplus
}
#endif

#include "mediabase.h"
namespace LQF {
//先全面借鉴ffplay的机制

typedef struct MyAVPacketList {
    AVPacket		pkt;
    struct MyAVPacketList	*next;          // 指向下一个元素
    int			serial;
} MyAVPacketList;

/**
 * @brief The PacketQueue class
 * 设计原则:
 *  1.有边界：可以设置最大缓存数量
 *  2.时长计算：可以结算队列里的数据维持播放的时间长度
 *  3.可阻塞：push和pop的时候可以使用阻塞的方式
 *  4.可非阻塞: push和pop的时候可以使用非阻塞的方式
 *  5.可清空队列
 *  6.可以唤醒队列进行退出
 */
 extern AVPacket flush_pkt;
class PacketQueue
{
public:
    static void InitFlushPacket() {
        av_init_packet(&flush_pkt);				// 初始化flush_packet
        flush_pkt.data = (uint8_t *)&flush_pkt; // 初始化为数据指向自己本身
    }
    PacketQueue() {

    }
    ~PacketQueue() {

    }
    int packet_queue_init() {
        mutex_ = SDL_CreateMutex();
        if (!mutex_) {
            LogError("SDL_CreateMutex(): %s\n", SDL_GetError());
            return AVERROR(ENOMEM);
        }
        cond_ = SDL_CreateCond();
        if (!cond_) {
            LogError("SDL_CreateCond(): %s\n", SDL_GetError());
            return AVERROR(ENOMEM);
        }
        abort_request_ = 1;
        is_init_ = 1;
        return 0;
    }
    int is_init() {
        return is_init_;
    }

    int packet_queue_put(AVPacket *pkt)
    {
        int ret;

        SDL_LockMutex(mutex_);
        ret = packet_queue_put_private(pkt);
        SDL_UnlockMutex(mutex_);

        if (pkt != &flush_pkt && ret < 0)
            av_packet_unref(pkt);

        return ret;
    }

    int packet_queue_put_nullpacket(int stream_index)
    {
        AVPacket pkt1, *pkt = &pkt1;
        av_init_packet(pkt);
        pkt->data = NULL;
        pkt->size = 0;
        pkt->stream_index = stream_index;
        return packet_queue_put(pkt);
    }

    void packet_queue_flush()
    {
        MyAVPacketList *pkt, *pkt1;

        SDL_LockMutex(mutex_);
        for (pkt = first_pkt_; pkt; pkt = pkt1) {
            pkt1 = pkt->next;
            av_packet_unref(&pkt->pkt);
            av_freep(&pkt);
        }
        last_pkt_ = NULL;
        first_pkt_ = NULL;
        nb_packets_ = 0;
        size_ = 0;
        duration_ = 0;
        SDL_UnlockMutex(mutex_);
    }

    void packet_queue_destroy()
    {
        packet_queue_flush();
        SDL_DestroyMutex(mutex_);
        SDL_DestroyCond(cond_);
    }

    void packet_queue_abort()
    {
        SDL_LockMutex(mutex_);

        abort_request_ = 1;

        SDL_CondBroadcast(cond_);

        SDL_UnlockMutex(mutex_);
    }

    void packet_queue_start()
    {
        SDL_LockMutex(mutex_);
        abort_request_ = 0;
        packet_queue_put_private(&flush_pkt);
        SDL_UnlockMutex(mutex_);
    }

    /* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
    int packet_queue_get(AVPacket *pkt, int block, int *serial)
    {
        MyAVPacketList *pkt1;
        int ret;

        SDL_LockMutex(mutex_);

        for (;;) {
            if (abort_request_) {
                ret = -1;
                break;
            }

            pkt1 = first_pkt_;
            if (pkt1) {
                first_pkt_ = pkt1->next;
                if (!first_pkt_)
                    last_pkt_ = NULL;
                nb_packets_--;
                size_ -= pkt1->pkt.size + sizeof(*pkt1);
                duration_ -= pkt1->pkt.duration;
                *pkt = pkt1->pkt;
                if (serial)
                    *serial = pkt1->serial;
                av_free(pkt1);
                ret = 1;
                break;
            } else if (!block) {
                ret = 0;
                break;
            } else {
                SDL_CondWait(cond_, mutex_);
            }
        }
        SDL_UnlockMutex(mutex_);
        return ret;
    }

    int packet_wait_cache(int block, int duration)
    {
        int ret;

        SDL_LockMutex(mutex_);

        for (;;) {
            if (abort_request_) {
                ret = -1;
                break;
            }
            if (duration_ > duration) {// 有足够的cache
                ret = 1;
                break;
            } else if (!block) {
                ret = 0;
                break;
            } else {
                SDL_CondWait(cond_, mutex_);
            }
        }
        SDL_UnlockMutex(mutex_);
        return ret;
    }

    inline int get_nb_packets() {
        return nb_packets_;
    }
    inline int duration() {
//        int64_t temp_duration = 0;
//        SDL_LockMutex(mutex_);
//        temp_duration = duration_;
//        SDL_UnlockMutex(mutex_);
        return  duration_ ;
    }

    inline int abort_request() {
        return abort_request_;
    }
    inline int serial() {
        return serial_;
    }

public:

    int packet_queue_put_private(AVPacket *pkt)
    {
        MyAVPacketList *pkt1;

        if (abort_request_)
            return -1;

        pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList));
        if (!pkt1)
            return -1;
        pkt1->pkt = *pkt;
        pkt1->next = NULL;
        if (pkt == &flush_pkt)
        {
            serial_++;
            printf("serial = %d\n", serial_++);
        }
        pkt1->serial = serial_;

        if (!last_pkt_)
            first_pkt_ = pkt1;
        else
            last_pkt_->next = pkt1;
        last_pkt_ = pkt1;
        nb_packets_++;
        size_ += pkt1->pkt.size + sizeof(*pkt1);
        duration_ += pkt1->pkt.duration;
        /* XXX: should duplicate packet data in DV case */
        SDL_CondSignal(cond_);
        return 0;
    }

    MyAVPacketList	*first_pkt_ = NULL;
    MyAVPacketList *last_pkt_ = NULL;  // 队首，队尾指针
    int		nb_packets_ = 0;                 // 包数量，也就是队列元素数量
    int		size_ = 0;                       // 队列所有元素的数据大小总和
    int		duration_ = 0;               // 队列所有元素的数据播放持续时间
    int		abort_request_ = 0;              // 用户退出请求标志
    int		serial_ = 0;                     //
    SDL_mutex	*mutex_ = NULL;                 // 互斥量
    SDL_cond	*cond_ = NULL;                  // 条件变量
    int is_init_ = 0;
};

}


#endif // PACKETQUEUE_H
