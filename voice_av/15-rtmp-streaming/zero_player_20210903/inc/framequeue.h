#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H
#include <iostream>
#include <stdint.h>
#include <string.h>
#include <mutex>
#include <condition_variable>
#include "dlog.h"
#include "mediabase.h"
#include "packetqueue.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace LQF {
#define VIDEO_PICTURE_QUEUE_SIZE	3       // 图像帧缓存数量
#define SUBPICTURE_QUEUE_SIZE		16      // 字幕帧缓存数量
#define SAMPLE_QUEUE_SIZE           9       // 采样帧缓存数量
#define FRAME_QUEUE_SIZE		SUBPICTURE_QUEUE_SIZE
using std::mutex;
using std::conditional;

// 用于缓存解码后的数据
typedef struct Frame {
    AVFrame		*frame;         // 指向数据帧
    AVSubtitle	sub;            // 用于字幕
    int		serial;             // 帧序列，在seek的操作时serial会变化
    double		pts;            // 时间戳，单位为秒
    double		duration;       // 该帧持续时间，单位为秒
    int         int_duration;   // 单位为ms，方便统计避免浮点数的问题
    int64_t		pos;            // 该帧在输入文件中的字节位置
    int		width;              // 图像宽度
    int		height;             // 图像高读
    int		format;             // 对于图像为(enum AVPixelFormat)，
    // 对于声音则为(enum AVSampleFormat)
    AVRational	sar;            // 图像的宽高比（16:9，4:3...），如果未知或未指定则为0/1
    int		uploaded;           // 用来记录该帧是否已经显示过？
    int		flip_v;             // =1则旋转180， = 0则正常播放
} Frame;

class FrameQueue
{
public:
    FrameQueue();

    int frame_queue_init(PacketQueue *pktq, int max_size, int keep_last)
    {
        int i;
        if (!(mutex_ = SDL_CreateMutex())) {
            av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
            return AVERROR(ENOMEM);
        }
        if (!(cond_ = SDL_CreateCond())) {
            av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
            return AVERROR(ENOMEM);
        }
        pktq_ = pktq;
        max_size_ = FFMIN(max_size, FRAME_QUEUE_SIZE);
        keep_last_ = !!keep_last;
        for (i = 0; i < max_size_; i++)
            if (!(queue_[i].frame = av_frame_alloc())) // 分配AVFrame结构体
                return AVERROR(ENOMEM);
        return 0;
    }
    void frame_queue_destory()
    {
        int i;
        for (i = 0; i < max_size_; i++) {
            Frame *vp = &queue_[i];
            // 释放对vp->frame中的数据缓冲区的引用，注意不是释放frame对象本身
            frame_queue_unref_item(vp);
            // 释放vp->frame对象
            av_frame_free(&vp->frame);
        }
        SDL_DestroyMutex(mutex_);
        SDL_DestroyCond(cond_);
    }
    /* 获取队列当前Frame, 在调用该函数前先调用frame_queue_nb_remaining确保有frame可读 */
    Frame *frame_queue_peek()
    {
        return &queue_[(rindex_ + rindex_shown_) % max_size_];
    }

    /* 获取当前Frame的下一Frame, 此时要确保queue里面至少有2个Frame */
    // 不管你什么时候调用，返回来肯定不是 NULL
    Frame *frame_queue_peek_next()
    {
        return &queue_[(rindex_ + rindex_shown_ + 1) % max_size_];
    }

    /* 获取last Frame：
     * 当rindex_shown=0时，和frame_queue_peek效果一样
     * 当rindex_shown=1时，读取的是已经显示过的frame
     */
    Frame *frame_queue_peek_last()
    {
        return &queue_[rindex_];    // 这时候才有意义
    }
    // 获取可写指针
    Frame *frame_queue_peek_writable()
    {
        /* wait until we have space to put a new frame */
        SDL_LockMutex(mutex_);
        while (size_ >= max_size_ &&
               !pktq_->abort_request()) {	/* 检查是否需要退出 */
            SDL_CondWait(cond_, mutex_);
        }
        SDL_UnlockMutex(mutex_);

        if (pktq_->abort_request())			 /* 检查是不是要退出 */
            return NULL;

        return &queue_[windex_];
    }

    Frame *frame_queue_peek_readable()
    {
        /* wait until we have a readable a new frame */
        SDL_LockMutex(mutex_);
        while (size_ - rindex_shown_ <= 0 &&
               !pktq_->abort_request()) {
            SDL_CondWait(cond_, mutex_);
        }
        SDL_UnlockMutex(mutex_);

        if (pktq_->abort_request())
            return NULL;

        return &queue_[(rindex_ + rindex_shown_) % max_size_];
    }
    // 更新写指针
    void frame_queue_push(int duration_ms)
    {
        if (++windex_ == max_size_)
            windex_ = 0;
        SDL_LockMutex(mutex_);
        size_++;
        total_duration_ += duration_ms;
        SDL_CondSignal(cond_);    // 当_readable在等待时则可以唤醒
        SDL_UnlockMutex(mutex_);
    }

    /* 释放当前frame，并更新读索引rindex，
     * 当keep_last为1, rindex_show为0时不去更新rindex,也不释放当前frame */
    void frame_queue_next()
    {
        if (keep_last_ && !rindex_shown_) {
            rindex_shown_ = 1; // 第一次进来没有更新，对应的frame就没有释放
            return;
        }
        int duration_ms = queue_[rindex_].int_duration;
        frame_queue_unref_item(&queue_[rindex_]);
        if (++rindex_ == max_size_)
            rindex_ = 0;
        SDL_LockMutex(mutex_);
        size_--;
        total_duration_ -= duration_ms;
        if(total_duration_ < 0) {
            total_duration_ = 0;
            LogError("total_duration_ < 0 is error");
        }
        SDL_CondSignal(cond_);
        SDL_UnlockMutex(mutex_);
    }

    /* return the number of undisplayed frames in the queue */
    int frame_queue_nb_remaining()
    {
        return size_ - rindex_shown_;	// 注意这里为什么要减去rindex_shown_
    }

    /* return last shown position */
    int64_t frame_queue_last_pos()
    {
        Frame *fp = &queue_[rindex_];
        if (rindex_shown_ && fp->serial == pktq_->serial())
            return fp->pos;
        else
            return -1;
    }
    // 剩余帧数
    int size() {
        return size_ - rindex_shown_; // 注意这里为什么要减去rindex_shown_
    }
    int maxSize() {
        return max_size_; // 注意这里为什么要减去rindex_shown_
    }
    void frame_queue_unref_item(Frame *vp)
    {
        av_frame_unref(vp->frame);	/* 释放数据 */
//        avsubtitle_free(&vp->sub);
    }

    void frame_queue_abort()
    {
        SDL_LockMutex(mutex_);

        abort_request_ = 1;

        SDL_CondBroadcast(cond_);

        SDL_UnlockMutex(mutex_);
    }
    int duration()
    {
        return total_duration_;
    }
public:
    int abort_request_ = 0;
    Frame	queue_[FRAME_QUEUE_SIZE];        // FRAME_QUEUE_SIZE  最大size, 数字太大时会占用大量的内存，需要注意该值的设置
    int		rindex_ = 0;                         // 表示循环队列的结尾处
    int		windex_ = 0;                         // 表示循环队列的开始处
    int		size_ = 0;                           // 当前队列的有效帧数
    int		max_size_ = FRAME_QUEUE_SIZE;                       // 当前队列最大的帧数容量
    int		keep_last_ = 0;                      // = 1说明要在队列里面保持最后一帧的数据不释放，只在销毁队列的时候才将其真正释放
    int		rindex_shown_ = 0;                   // 初始化为0，配合keep_last=1使用
    SDL_mutex	*mutex_;                     // 互斥量
    SDL_cond	*cond_;                      // 条件变量
    PacketQueue	*pktq_;                      // 数据包缓冲队列
    int total_duration_ = 0;
};
}
#endif // FRAMEQUEUE_H
