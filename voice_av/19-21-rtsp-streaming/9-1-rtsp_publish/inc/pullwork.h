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
#include "audioresample.h"
#include "ImageScale.h"
namespace LQF
{
class PullWork
{
public:
    PullWork();
    ~PullWork();
    RET_CODE Init(const Properties& properties);

    // Audio编码后的数据回调
    void audioInfoCallback(int what, MsgBaseObj *data, bool flush = false);
    // Video编码后的数据回调
    void videoInfoCallback(int what, MsgBaseObj *data, bool flush = false);

    void audioDataCallback(void *);
    // Video编码后的数据回调
    void videoDataCallback(void *);

    // pcm数据的数据回调
    void pcmCallback(uint8_t* pcm, uint32_t size, int64_t pts);
    void pcmFrameCallback(void *);
    void displayVideo(uint8_t* yuv, uint32_t size, int32_t format);
    // 数据回调
    void yuvCallback(uint8_t* yuv, uint32_t size, int64_t pts);
    void yuvFrameCallback(void *frame);
    int avSyncCallback(int64_t pts, int32_t duration, int64_t &get_diff);

private:
    std::string rtmp_url_;

    RTMPPlayer *rtmp_player_ = NULL;
    AudioDecodeLoop *audio_decode_loop_ = NULL; //音频解码线程
    AudioOutSDL *audio_out_sdl_ = NULL;

    VideoDecodeLoop *video_decode_loop_; //视频解码线程
    VideoOutputLoop *video_output_loop_ = NULL;
    VideoOutSDL *video_out_sdl_ = NULL;
    AVSync *avsync_ = NULL;

    int audio_out_sample_rate_ = 44100;
    int audio_out_sample_channels_ = 2;
    int audio_out_sample_format_ = AV_SAMPLE_FMT_S16;
    // 重采样
    AudioResampler *audio_resampler_ = NULL; // 将解码后的数据设置成输出需要音频
    // 视频尺寸变换
    ImageScaler *img_scale_ = NULL;
    int video_out_width_ = 480;
    int video_out_height_ = 270;
    AVPixelFormat video_out_format_ = AV_PIX_FMT_YUV420P;

    // 缓存控制
    int cache_duration_ = 500;
};
}
#endif // PULLWORK_H
