#ifndef RTSPPUSHER_H
#define RTSPPUSHER_H

#include "mediabase.h"
#include "commonlooper.h"
#include "packetqueue.h"
extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
}

class RtspPusher: public CommonLooper
{
public:
    RtspPusher();
    virtual ~RtspPusher();
    RET_CODE Init(const Properties& properties);
    void DeInit();
    RET_CODE Push(AVPacket *pkt, MediaType media_type);
    // 连接服务器，如果连接成功则启动线程
    RET_CODE Connect();

    // 如果有视频成分
    RET_CODE ConfigVideoStream(const AVCodecContext *ctx);
    // 如果有音频成分
    RET_CODE ConfigAudioStream(const AVCodecContext *ctx);
    virtual void Loop();
private:
    int sendPacket(AVPacket *pkt, MediaType media_type);
    // 整个输出流的上下文
    AVFormatContext *fmt_ctx_ = NULL;
    // 视频编码器上下文
    AVCodecContext *video_ctx_ = NULL;
    // 音频频编码器上下文
    AVCodecContext *audio_ctx_ = NULL;

    // 流成分
    AVStream *video_stream_ = NULL;
    int video_index_ = -1;
    AVStream *audio_stream_ = NULL;
    int audio_index_ = -1;

    std::string url_ = "";
    std::string rtsp_transport_ = "";

    double audio_frame_duration_ = 23.21995649; // 默认23.2ms 44.1khz  1024*1000ms/44100=23.21995649ms
    double video_frame_duration_ = 40;  // 40ms 视频帧率为25的  ， 1000ms/25=40ms
    PacketQueue *queue_ = NULL;
};

#endif // RTSPPUSHER_H
