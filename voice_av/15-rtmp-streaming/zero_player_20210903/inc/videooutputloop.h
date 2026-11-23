#ifndef VIDEOOUTPUTLOOP_H
#define VIDEOOUTPUTLOOP_H
#include <stdint.h>
#include <functional>
#include "commonlooper.h"
#include "framequeue.h"
#include "avsync.h"

namespace LQF {
using std::function;
class VideoOutputLoop : public CommonLooper
{
public:
    VideoOutputLoop(AVSync *avsync, PacketQueue *packet_queue, FrameQueue *frame_queue);
    virtual ~VideoOutputLoop() {

    }
    RET_CODE Init(const Properties& properties);
    // 负责输出
    virtual void Loop();
    void SetPlaybackRate(float playback_rate)
    {
        playback_rate_ = playback_rate;
    }


    void AddVideoRefreshCallback(function<int(const Frame *)> callback)
    {
         video_refresh_callback_ = callback;
    }
    // 用于判断是否输出显示的回调函数
    void AddAVSyncCallback(function<int(int64_t, int32_t, int64_t &)> callback)
    {
        callback_avsync_ = callback;
    }

    int		paused = 0;             // =1时暂停，=0时播放
    int		force_refresh = 0;      // =1时需要刷新画面，请求立即刷新画面的意思
    double frame_timer = 0;             // 记录最后一帧播放的时刻
    function<int(const Frame *)> video_refresh_callback_ = NULL;
    AVSync *avsync_ = NULL;
    FrameQueue *frame_queue_ = NULL;
    PacketQueue *packet_queue_ = NULL;
    int frame_drops_late = 0;
    bool b_first_frame = true;
private:
    void update_video_pts(double pts, int64_t pos, int serial);
    double compute_target_delay(double delay);
    void video_refresh(double *remaining_time);
    // pts和帧间隔
    std::function<int(int64_t, int32_t, int64_t &)> callback_avsync_ = NULL;
    int64_t pre_pts_ = 0;

    bool is_show_first_frame_ = false;  // 第一帧不做音视频同步
    uint32_t PRINT_MAX_FRAME_OUT_TIME = 5;
    uint32_t out_frames_ = 0;       // 统计输出的帧数

    float playback_rate_ = 1.0;

};

}

#endif // VIDEOOUTPUTLOOP_H
