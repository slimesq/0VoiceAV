#ifndef RTMPPUSHER_H
#define RTMPPUSHER_H
#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include "mediabase.h"
#include "naluloop.h"
#include "dlog.h"
namespace LQF
{
typedef enum RTSPMediaType
{
    RTSP_VIDEO_DATA = 1,
    RTSP_AUDIO_DATA = 2
}RTSPMediaType_T;


// 实现异步发送
class RTSPPusher : public NaluLoop
{
public:
    RTSPPusher():NaluLoop(30){LogInfo("RTSPPusher create!");}
    ~RTSPPusher();
    bool Init(std::string url);

    bool ConfigVideoSteam(const AVCodecContext *ctx);
    bool ConfigAudioSteam(const AVCodecContext *ctx);
    bool Connect(int timeout);

    virtual void handle(int what, void *data);
    void Close();

private:
    bool sendPacket(AVPacket *packet,RTSPMediaType_T mediatype);
    int64_t time_ = 0; //在SendMetadata获取时间比较准确


    bool is_first_metadata_ = false;    // 发送metadata
    bool is_first_video_sequence_ = false;
    bool is_first_video_frame_ = false;
    bool is_first_audio_sequence_ = false;
    bool is_first_audio_frame_ = false;

    //封装器
    AVFormatContext *fmt_ctx_ = NULL;
    // 视频编解码器上下文
    //视频编码器上下文
    AVCodecContext *video_ctx_ = NULL;

    //音频编码器上下文
    AVCodecContext *audio_ctx_ = NULL;

    //视频流
    AVStream *video_stream_ = NULL;
    int video_index_ = -1;

    //音频流
    AVStream *audio_stream_ = NULL;
    int audio_index_ = -1;

    string url_ ="";
};
}
#endif // RTMPPUSHER_H
