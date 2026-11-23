#ifndef PULLWORK_H
#define PULLWORK_H
#include "mediabase.h"
#include "audiodecodeloop.h"
#include "videodecodeloop.h"
#include "audiooutsdl.h"
#include "videooutputloop.h"
#include "videooutsdl.h"
#include "rtmpplayer.h"
#include "avsync.h"
#include "imagescale.h"
#include "framequeue.h"
namespace LQF
{

#define PULL_WORK_EVENT  0x1000
#define EVENT_UPDATE_AUDIO_CACHE_DURATION (PULL_WORK_EVENT+1)
#define EVENT_UPDATE_VIDEO_CACHE_DURATION (PULL_WORK_EVENT+2)

class PullWork
{
public:
    PullWork();
    ~PullWork();
    RET_CODE Init(const Properties& properties);

    // Audio info回调
    int AudioInfoCallback(int what, MsgBaseObj *data, bool flush = false);
    // Video info回调
    int VideoInfoCallback(int what, MsgBaseObj *data, bool flush = false);

    float AudioCacheInfoCallback(float speed_factor, int packet_cache, int frame_cache);


    // audio packet回调
    int audioPacketCallback(void *);
    // video packet回调
    int videoPacketCallback(void *);

    int displayVideo(uint8_t* yuv, uint32_t size, int32_t format);

    // 数据回调
    int outVideoPicture(const Frame *frame);

    int avSyncCallback(double pts, double duration, double &get_diff);
    void AddVideoRefreshCallback(function<int(const Frame *)> callback)
    {
         video_refresh_callback_ = callback;
    }
    void AddEventCallback(function<int(int what, int arg1, int arg2, int arg3, void *data)> callback)
    {
         event_callback_ = callback;
    }
    void setMax_cache_duration(int max_cache_duration);

    void setNetwork_jitter_duration(int network_jitter_duration);

    void setAccelerate_speed_factor(float accelerate_speed_factor);
    void cacheEnoughCallback(bool cache_enough);


private:
    function<int(const Frame *)> video_refresh_callback_ = NULL;
    function<int(int what, int arg1, int arg2, int arg3, void *data)> event_callback_ = NULL;
    std::string rtmp_url_;

    RTMPPlayer *rtmp_player_ = NULL;
    AudioDecodeLoop *audio_decode_loop_ = NULL; //音频解码线程
    AudioOutSDL *audio_out_sdl_ = NULL;

    VideoDecodeLoop *video_decode_loop_; //视频解码线程
    VideoOutputLoop *video_output_loop_ = NULL;
    AVSync *avsync_ = NULL;

    int audio_out_sample_rate_ = 48000;
    int audio_out_sample_channels_ = 2;
    int audio_out_sample_format_ = AV_SAMPLE_FMT_S16;
    // 视频尺寸变换
    ImageScaler *img_scale_ = NULL;
    int video_out_width_ = 480;
    int video_out_height_ = 270;
    AVPixelFormat video_out_format_ = AV_PIX_FMT_YUV420P;

    VideoFrame src_video_frame_;

    // packet队列
    PacketQueue audioq; // 音频队列
    PacketQueue videoq; // 视频队列

    // frame队列
    FrameQueue	pictq;          // 视频Frame队列
    FrameQueue	sampq;          // 采样Frame队列
    // 缓存控制
    int max_cache_duration_ = 500;  // 默认500ms
    int network_jitter_duration_ = 100; // 默认100ms
    float accelerate_speed_factor_ = 1.2; //默认加速是1.2

    int packet_cache_delay_ = 0;
};
}
#endif // PULLWORK_H
