/**
  包含audio 解码器,
**/
#ifndef AUDIODECODELOOP_H
#define AUDIODECODELOOP_H
#include <deque>
#include <mutex>
#include <functional>
#include "aacdecoder.h"
#include "commonlooper.h"
#include "mediabase.h"
#include "packetqueue.h"
#include "semaphore.h"
#include "framequeue.h"
namespace LQF {
class AudioDecodeLoop : public CommonLooper
{
public:
    AudioDecodeLoop(PacketQueue *packet_queue = NULL,
                    FrameQueue *frame_queue = NULL);
    virtual ~AudioDecodeLoop();
    RET_CODE Init(const Properties& properties);


    void AddEventCallback(std::function<int(void*)> callableObject) {
    }
     virtual void Loop();
    int Post(void *);
    void setCache_enough(bool cache_enough);

    void setMax_cache_duration(int max_cache_duration);

private:
    AACDecoder *aac_decoder_ = NULL;
    //    uint8_t *pcm_buf_;
//    int32_t pcm_buf_size_;
//    const int PCM_BUF_MAX_SIZE = 32768; //

    uint32_t PRINT_MAX_FRAME_DECODE_TIME = 5;
    uint32_t decode_frames_ = 0;
    uint32_t deque_max_size_ = 20;

    // 队列相关
    AVPacket pkt_;
    PacketQueue	*packet_queue_;         // 数据包队列
    int		pkt_serial_;         // 包序列
    int		finished_;           // =0，解码器处于工作状态；=非0，解码器处于空闲状态
    int		packet_pending_;     // =0，解码器处于异常状态，需要考虑重置解码器；=1，解码器处于正常状态
    int64_t		start_pts_;          // 初始化时是stream的start time
    AVRational	start_pts_tb_;       // 初始化时是stream的time_base
    int64_t		next_pts_;           // 记录最近一次解码后的frame的pts，当解出来的部分帧没有有效的pts时则使用next_pts进行推算
    AVRational	next_pts_tb_;        // next_pts的单位

    FrameQueue *frame_queue_;

    int packet_cache_delay_ = 0;
    int max_cache_duration_ = 1000; // 默认是1秒
    bool cache_enough_ = false;
    Semaphore semaphore_;

};

}

#endif // AUDIODECODELOOP_H
