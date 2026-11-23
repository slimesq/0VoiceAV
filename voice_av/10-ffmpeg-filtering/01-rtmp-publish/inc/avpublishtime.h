#ifndef AVTIMEBASE_H
#define AVTIMEBASE_H
#include <stdint.h>

#include <cstdlib>
#include "timesutil.h"
#include "dlog.h"
class AVPublishTime
{
public:
    typedef enum PTS_STRATEGY {
        PTS_RECTIFY = 0,        // 缺省类型，pts的间隔尽量保持帧间隔
        PTS_REAL_TIME           // 实时pts
    }PTS_STRATEGY;
public:
    static AVPublishTime* GetInstance() {
        static AVPublishTime instance{};
        return &instance;
    }

    AVPublishTime() {
        start_time_ = getCurrentTimeMsec();
    }

    void Rest() {
        start_time_ = getCurrentTimeMsec();
        audio_pre_pts_ = 0;
        video_pre_pts_ = 0;
    }

    void set_audio_frame_duration(const double frame_duration) {
        audio_frame_duration_ = frame_duration;
        audio_frame_threshold_ = (uint32_t)(frame_duration/2);
    }

    void set_video_frame_duration(const double frame_duration) {
        video_frame_duration_ = frame_duration;
        video_frame_threshold_ = (uint32_t)(frame_duration/2);
    }

    uint32_t get_audio_pts() {
        int64_t pts = getCurrentTimeMsec() - start_time_;
        if(PTS_RECTIFY == audio_pts_strategy_) {
            uint32_t diff = (uint32_t)abs(pts - (long long)(audio_pre_pts_ + audio_frame_duration_));
            if(diff < audio_frame_threshold_) {
                // 误差在阈值范围内, 保持帧间隔
                audio_pre_pts_ += audio_frame_duration_; //帧间隔累加，浮点数
                LogDebug("get_audio_pts1:%u RECTIFY:%0.0lf", diff, audio_pre_pts_);
                return (uint32_t)(((int64_t)audio_pre_pts_)%0xffffffff);
            }
            audio_pre_pts_ = (double)pts; // 误差超过半帧，重新调整pts
            LogDebug("get_audio_pts2:%u, RECTIFY:%0.0lf", diff, audio_pre_pts_);
            return (uint32_t)(pts%0xffffffff);
        }else {
            audio_pre_pts_ = (double)pts; // 误差超过半帧，重新调整pts
            LogDebug("get_audio_pts REAL_TIME:%0.0lf", audio_pre_pts_);
            return (uint32_t)(pts%0xffffffff);
        }
    }

    uint32_t get_video_pts() {
        int64_t pts = getCurrentTimeMsec() - start_time_;
        if(PTS_RECTIFY == video_pts_strategy_) {
            uint32_t diff =(uint32_t)abs(pts - (long long)(video_pre_pts_ + video_frame_duration_));
            if(diff < video_frame_threshold_) {
                // 误差在阈值范围内, 保持帧间隔
                video_pre_pts_ += video_frame_duration_;
                LogDebug("get_video_pts1:%u RECTIFY:%0.0lf", diff, video_pre_pts_);
                return (uint32_t)(((int64_t)video_pre_pts_)%0xffffffff);
            }
            video_pre_pts_ = (double)pts; // 误差超过半帧，重新调整pts
            LogDebug("get_video_pts2:%u RECTIFY:%0.0lf", diff, video_pre_pts_);
            return (uint32_t)(pts%0xffffffff);
        }else {
            video_pre_pts_ = (double)pts; // 误差超过半帧，重新调整pts
            LogDebug("get_video_pts REAL_TIME:%0.0lf", video_pre_pts_);
            return (uint32_t)(pts%0xffffffff);
        }
    }


    void set_audio_pts_strategy(PTS_STRATEGY pts_strategy){
        audio_pts_strategy_ = pts_strategy;
    }
    void set_video_pts_strategy(PTS_STRATEGY pts_strategy){
        video_pts_strategy_ = pts_strategy;
    }

    uint32_t getCurrenTime() {
        int64_t t = getCurrentTimeMsec() - start_time_;

        return (uint32_t)(t%0xffffffff);

    }
    // 各个关键点的时间戳
    inline const char *getKeyTimeTag() {
        return "keytime";
    }
    // rtmp位置关键点
    inline const char *getRtmpTag() {
        return "keytime:rtmp_publish";
    }

    // 发送metadata
    inline const char *getMetadataTag() {
        return "keytime:metadata";
    }
    // aac sequence header
    inline const char *getAacHeaderTag() {
        return "keytime:aacheader";
    }
    // aac raw data
    inline const char *getAacDataTag() {
        return "keytime:aacdata";
    }
    // avc sequence header
    inline const char *getAvcHeaderTag() {
        return "keytime:avcheader";
    }

    // 第一个i帧
    inline const char *getAvcIFrameTag() {
        return "keytime:avciframe";
    }
    // 第一个非i帧
    inline const char *getAvcFrameTag() {
        return "keytime:avcframe";
    }
    // 音视频解码
    inline const char *getAcodecTag() {
        return "keytime:acodec";
    }
    inline const char *getVcodecTag() {
        return "keytime:vcodec";
    }
    // 音视频捕获
    inline const char *getAInTag() {
        return "keytime:ain";
    }
    inline const char *getVInTag() {
        return "keytime:vin";
    }
private:
    int64_t getCurrentTimeMsec() const noexcept {
        return TimesUtil::GetTimeMillisecond();
    }

    int64_t start_time_ = 0;

    PTS_STRATEGY audio_pts_strategy_ = PTS_RECTIFY;
    double audio_frame_duration_ = 21.3333;  // 默认按aac 1024 个采样点, 48khz计算
    uint32_t audio_frame_threshold_ = (uint32_t)(audio_frame_duration_ /2);
    double audio_pre_pts_ = 0;

    PTS_STRATEGY video_pts_strategy_ = PTS_RECTIFY;
    double video_frame_duration_ = 40;  // 默认是25帧计算
    uint32_t video_frame_threshold_ = (uint32_t)(video_frame_duration_ /2);
    double video_pre_pts_ = 0;

};


// 用来debug rtmp拉流的关键时间点
class AVPlayTime
{
public:
    static AVPlayTime* GetInstance() {
        static AVPlayTime instance{};
        return &instance;
    }

    AVPlayTime() {
        start_time_ = getCurrentTimeMsec();
    }

    void Rest() {
        start_time_ = getCurrentTimeMsec();
    }
    // 各个关键点的时间戳
    inline const char *getKeyTimeTag() {
        return "keytime";
    }
    // rtmp位置关键点
    inline const char *getRtmpTag() {
        return "keytime:rtmp_pull";
    }
    // 获取到metadata
    inline const char *getMetadataTag() {
        return "metadata";
    }
    // aac sequence header
    inline const char *getAacHeaderTag() {
        return "aacheader";
    }
    // aac raw data
    inline const char *getAacDataTag() {
        return "aacdata";
    }
    // avc sequence header
    inline const char *getAvcHeaderTag() {
        return "avcheader";
    }

    // 第一个i帧
    inline const char *getAvcIFrameTag() {
        return "avciframe";
    }
    // 第一个非i帧
    inline const char *getAvcFrameTag() {
        return "avcframe";
    }
    // 音视频解码
    inline const char *getAcodecTag() {
        return "keytime:acodec";
    }
    inline const char *getVcodecTag() {
        return "keytime:vcodec";
    }
    // 音视频输出
    inline const char *getAoutTag() {
        return "keytime:aout";
    }
    inline const char *getVoutTag() {
        return "keytime:vout";
    }

    // 返回毫秒
    uint32_t getCurrenTime() {
        int64_t t = getCurrentTimeMsec() - start_time_;

        return (uint32_t)(t%0xffffffff);

    }

private:
    int64_t getCurrentTimeMsec() const noexcept {
        return TimesUtil::GetTimeMillisecond();
    }

    int64_t start_time_ = 0;

};


#endif // AVTIMEBASE_H
