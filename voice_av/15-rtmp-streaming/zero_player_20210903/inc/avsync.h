#ifndef AVSYNC_H
#define AVSYNC_H
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#include "libavutil/time.h"
}
#endif
#include "mediabase.h"

/**
* 第一版本：仅支持audio master的方式
* 和ffplay不一样，我们统一使用毫秒的方式
*/
/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0
#define REFRESH_RATE 0.01
namespace LQF {
enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

enum AV_FRAME_SYNC_RESULT {
    AV_FRAME_FREE_RUN, // 有就播放，不做同步
    AV_FRAME_HOLD,     // 等待到时间再播放
    AV_FRAME_DROP,     // 丢掉当前帧
    AV_FRAME_PLAY      // 正常播放
};

const int MAX_NAME_SIZE = 20;

class Clock
{

public:
    Clock()
    {
    }

    void init_clock(int *queue_serial, const char *name)
        {
            speed_ = 1.0;
            paused_ = 0;
            queue_serial_ = queue_serial;
            if (name) {
                strncpy(name_, name, MAX_NAME_SIZE);
                name_[MAX_NAME_SIZE] = '\0';
            } else {
                name_[0] = '\0';
            }
            set_clock(NAN, -1);
        }
    double get_clock()
    {
        if (*queue_serial_ != serial_)
            return NAN;
        if (paused_) {
            return pts_;
        } else {
            double time = av_gettime_relative() / 1000000.0;
            return pts_drift_ + time - (time - last_updated_) * (1.0 - speed_);
        }
    }
    void set_clock_at(double pts, int serial, double time)
    {
        pts_ = pts;
        last_updated_ = time;
        pts_drift_ = pts_ - time;
        serial_ = serial;
        LogDebug("%s %lf, get %lf", name_, pts, get_clock());
    }
    void set_clock(double pts, int serial)
    {
        double time = av_gettime_relative() / 1000000.0;
        set_clock_at(pts, serial, time);
    }
    void init_clock(int *queue_serial)
    {
        speed_ = 1.0;
        paused_ = 0;
        queue_serial_ = queue_serial;
        set_clock(NAN, -1);
    }
    void sync_clock_to_slave(Clock *slave)
    {
        double clock = get_clock();
        double slave_clock = slave->get_clock();
        if (!isInvalid(slave_clock) && (isInvalid(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
            slave->set_clock(slave_clock, slave->serial_);
    }
    bool isInvalid(const double pts) {
        return isnan(pts);
    }

public:
    double pts_;           /* clock base */
    double pts_drift_;     /* clock base minus time at which we updated the clock */
    double last_updated_;
    double speed_;
    int serial_;           /* clock is based on a packet with this serial */
    int paused_;
    int *queue_serial_;    /* pointer to the current packet queue serial, used for obsolete clock*/
    char name_[MAX_NAME_SIZE+1];

};

class AVSync
{
public:

    AVSync()
    {

    }
    virtual ~AVSync()
    {

    }
    bool IsInit() {
        return is_init_;
    }
    // 设置音视频同步类型
    RET_CODE Init(const int av_sync_type,
                  int *aud_queue_serial,
                  int *vid_queue_serial) {
        av_sync_type_ = av_sync_type;
        // 初始化时钟
        audclk.init_clock(aud_queue_serial, "audclk");
        vidclk.init_clock(vid_queue_serial, "vidclk");
        extclk.init_clock(&extclk.serial_, "extclk");

        update_audio_pts(0, 0);
        is_init_ = true;
        return RET_OK;
    }
    int get_master_sync_type()
    {
        if (av_sync_type_ == AV_SYNC_VIDEO_MASTER) {
            if (has_video_)
                return AV_SYNC_VIDEO_MASTER;
            else
                return AV_SYNC_AUDIO_MASTER;
        } else if (av_sync_type_ == AV_SYNC_AUDIO_MASTER) {
            if (has_audio_)
                return AV_SYNC_AUDIO_MASTER;
            else
                return AV_SYNC_EXTERNAL_CLOCK;
        } else {
            return AV_SYNC_EXTERNAL_CLOCK;
        }
    }
    /* get the current master clock value */
    double get_master_clock()
    {
        double val;

        switch (get_master_sync_type()) {
        case AV_SYNC_VIDEO_MASTER:
            val = vidclk.get_clock();
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = audclk.get_clock();
            break;
        default:
            val = extclk.get_clock();
            break;
        }
        return val;
    }

    void update_video_pts(const double pts, const int serial) {
        LogDebug("video pts:%lld", pts);
        vidclk.set_clock(pts, serial);
    }

    void update_audio_pts(const double pts, const int serial) {
        LogDebug("audio pts:%lld", pts);
        if(abs(pts - audclk.get_clock()) > audio_frame_druation_/2)  {
            audclk.set_clock(pts, serial);
        }
    }

    AV_FRAME_SYNC_RESULT GetVideoSyncResult(const double pts, const  double duration, double &get_diff)
    {
        video_frame_druation_ = duration;
        double diff = pts - get_master_clock();
        get_diff = diff;
        LogDebug("vpts:%lld, duration:%d, diff:%lld",  pts, duration, diff);
        if(diff > 0 && diff < video_frame_druation_*20) {
            return AV_FRAME_HOLD;
        } else if(diff<=0 && diff >-video_frame_druation_/2){
            return AV_FRAME_PLAY;
        }
        else if(diff <= -video_frame_druation_/2){
            return AV_FRAME_DROP;
        } else {
            // 其他情况自由奔放地播放
            LogWarn("video free run, diff:%lld", diff);
            return AV_FRAME_FREE_RUN;
        }
    }

    int             av_sync_type_ = AV_SYNC_AUDIO_MASTER;
    int             has_video_ = 1;
    int             has_audio_ = 1;
    bool is_init_ = false;
    Clock audclk;
    Clock vidclk;
    Clock extclk;

    double audio_frame_druation_ = 22;
    double video_frame_druation_ = 40;

    double          audio_clock_;
    double          frame_timer;
    double          frame_last_pts;
    double          frame_last_delay;

    double          video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame
    double          video_current_pts; ///<current displayed pts (different from video_clock if frame fifos are used)
    double         video_current_pts_time;  ///<time (av_gettime) at which we updated video_current_pts - used to have running video pts
    static AVSync  s_avsync_;
};
//AVSync *AVSync::s_avsync_ = NULL;
}

#endif // AVSYNC_H
