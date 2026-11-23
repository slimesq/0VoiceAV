#ifndef VIDEODECODELOOP_H
#define VIDEODECODELOOP_H
#include "commonlooper.h"
#include "mediabase.h"
#include "h264decoder.h"
#include "packetqueue.h"
#include "framequeue.h"
namespace LQF {
class VideoDecodeLoop: public CommonLooper
{
public:
    VideoDecodeLoop(PacketQueue *packetq = NULL, FrameQueue *frameq = NULL);
    virtual ~VideoDecodeLoop();
    virtual RET_CODE Init(const Properties& properties);

    int queue_picture(AVFrame *src_frame, double pts,
                      double duration, int64_t pos, int serial);
    int GetWdith(){
        return h264_decoder_->GetWidth();
    }
    int GetHeight(){
        return h264_decoder_->GetHeight();
    }

    virtual void Loop();
    int Post(void *);
private:


    H264Decoder *h264_decoder_ = NULL;
    uint8_t *yuv_buf_;
    int32_t yuv_buf_size_;
    const int YUV_BUF_MAX_SIZE = int(1920*1080*1.5); //先写死最大支持, fixme

    // 延迟指标监测
    uint32_t PRINT_MAX_FRAME_DECODE_TIME = 5;
    uint32_t decode_frames_ = 0;
    uint32_t deque_max_size_ = 20;
    PacketQueue *pkt_queue_;
    FrameQueue *frame_queue_;

    int		pkt_serial;         // 包序列
    int packet_cache_delay_;

    int		pkt_serial_;         // 包序列
    int		finished_;           // =0，解码器处于工作状态；=非0，解码器处于空闲状态
    int		packet_pending_;     // =0，解码器处于异常状态，需要考虑重置解码器；=1，解码器处于正常状态
    int64_t		start_pts_;          // 初始化时是stream的start time
    AVRational	start_pts_tb_;       // 初始化时是stream的time_base
    int64_t		next_pts_;           // 记录最近一次解码后的frame的pts，当解出来的部分帧没有有效的pts时则使用next_pts进行推算
    AVRational	next_pts_tb_;        // next_pts的单位
};
}



#endif // VideoDecodeLoop_H
