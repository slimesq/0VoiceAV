#include "rtsppusher.h"
#include "timeutil.h"
#include "avtimebase.h"
namespace LQF
{
RTSPPusher::~RTSPPusher()
{
    if(fmt_ctx_) {
        Close();
    }
}

bool RTSPPusher::Init(std::string url) {
    int ret = 0;
    url_ = url;

    //初始化网络库
    ret = avformat_network_init();
    if(ret < 0) {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        LogError("avformat_network_init failed:%s", buf);
        return false;
    }
    //分配一个AVFormatContext，FFMPEG所有的操作都要通过这个AVFormatContext来进行
    ret = avformat_alloc_output_context2(&fmt_ctx_, 0, "rtsp", url_.c_str());//rtsp
    if(ret <0) {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        LogError("avformat_alloc_output_context2 failed:%s", buf);
        return false;
    }
    av_opt_set(fmt_ctx_->priv_data, "rtsp_transport", "udp", 0);
    //    //获得输出格式，这里是RTSP网络流
//    fmt_ctx_->oformat = av_guess_format("rtsp", NULL, NULL);

    return true;
}

bool RTSPPusher::Connect(int timeout)
{
    int ret = 0;
    //写入封装头
    ret = avformat_write_header(fmt_ctx_, NULL);    // 卡着不动
    if (ret < 0) {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        LogError("avformat_write_header failed:%s", buf);
        return false;
    }
    return true;
}

bool RTSPPusher::ConfigVideoSteam(const AVCodecContext *ctx)
{
    if (!fmt_ctx_)
    {
        return false;
    }
    //添加视频流
    AVStream *vs = avformat_new_stream(fmt_ctx_, NULL);
    if (!vs)
    {
        LogError("avformat_new_stream failed");
        return false;
    }
    vs->codecpar->codec_tag = 0;

    //从编码器复制参数
    avcodec_parameters_from_context(vs->codecpar, ctx);
    //    av_dump_format(fmt_ctx_, 0, url_.c_str(), 1);

    if (ctx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        video_ctx_ = (AVCodecContext *)ctx;
        video_stream_ = vs;
    }
    else
    {
        return false;
    }
    video_index_ = video_stream_->index;
    return true;
}

bool RTSPPusher::ConfigAudioSteam(const AVCodecContext *ctx)
{
    if (!fmt_ctx_)
    {
        return false;
    }
    //添加音频流
    AVStream *as = avformat_new_stream(fmt_ctx_, NULL);
    if (!as)
    {
        LogError("avformat_new_stream failed");
        return false;
    }
    as->codecpar->codec_tag = 0;

    //从编码器复制参数
    avcodec_parameters_from_context(as->codecpar, ctx);
    //    av_dump_format(fmt_ctx_, 0, url_.c_str(), 1);

    if (ctx->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        audio_ctx_ = (AVCodecContext *)ctx;
        audio_stream_ = as;
    }
    else
    {
        return false;
    }
    audio_index_ = audio_stream_->index;
    return true;
}
void RTSPPusher::handle(int what, void *data)
{
    LogDebug("into");
    //要加是否断开连接逻辑


    switch(what)
    {
    case RTSP_BODY_VID_RAW:
    {
        if(!is_first_video_frame_) {
            is_first_video_frame_ = true;
            LogInfo("Video-> %s:t%u", AVPublishTime::GetInstance()->getAvcFrameTag(),
                    AVPublishTime::GetInstance()->getCurrenTime());
        }

        AVPacket* pkt = (AVPacket*)data;
        if(sendPacket(pkt, RTSP_VIDEO_DATA))
        {
            //LogInfo("send pack ok");
        }
        else
        {
            LogInfo("at handle send h264 pack fail");
        }
        av_packet_unref(pkt);       // 释放数据
        break;
    }
    case RTSP_BODY_AUD_RAW:
    {
        if(!is_first_audio_frame_) {
            is_first_audio_frame_ = true;
            LogInfo("Audio-> %s:t%u", AVPublishTime::GetInstance()->getAvcFrameTag(),
                    AVPublishTime::GetInstance()->getCurrenTime());
        }

        AVPacket* pkt = (AVPacket*)data;
        if(sendPacket(pkt, RTSP_AUDIO_DATA))
        {
            //LogInfo("send pack ok");
        }
        else
        {
            LogInfo("at handle send aac pack fail");
        }
        av_packet_free(&pkt);       // 释放数据
        break;
        break;
    }
    default:
        break;
    }
    LogDebug("leave");
}

void RTSPPusher::Close()
{
    Stop();     // 停止线程
    if (fmt_ctx_) {
        av_write_trailer(fmt_ctx_);
        avio_closep(&fmt_ctx_->pb);
    }
}

bool RTSPPusher::sendPacket(AVPacket *packet, RTSPMediaType_T mediatype)
{
    if (!packet || packet->size <= 0 || !packet->data) {
        LogError("packet error");
        return false;
    }
    AVRational srcTimeBase = {1, 1000};
    AVRational desTimeBase;

    if(RTSP_VIDEO_DATA == mediatype) {
        packet->stream_index = video_index_;
    }else if(RTSP_AUDIO_DATA == mediatype) {
        packet->stream_index = audio_index_;
    } else {
        LogError("unknown mediatype:%d", mediatype);
        return false;
    }
    //判断音视频
    if (video_stream_ && video_ctx_ && packet->stream_index == video_stream_->index)
    {
//        srcTimeBase = video_ctx_->time_base;
        desTimeBase = video_stream_->time_base;
    }
    else if (audio_stream_ && audio_ctx_ && packet->stream_index == audio_stream_->index)
    {
//        srcTimeBase = audio_ctx_->time_base;
        desTimeBase = audio_stream_->time_base;
    }
    else
    {
        return false;
    }
    packet->pts = av_rescale_q(packet->pts, srcTimeBase, desTimeBase);
    packet->dts = av_rescale_q(packet->dts, srcTimeBase, desTimeBase);
    packet->duration = av_rescale_q(packet->duration, srcTimeBase, desTimeBase);
    LogInfo("index:%d, pts:%lld, #size:%d",packet->stream_index,packet->pts, packet->size);
//    int ret = av_interleaved_write_frame(fmt_ctx_, packet);
    //av_write_frame 直接将包写进Mux，没有缓存和重新排序，一切都需要用户自己设置。
    int ret = av_write_frame(fmt_ctx_, packet);
    if (ret < 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        LogError("av_write_frame failed:%s", buf);
        return false;
    }
    return true;
}

}   // end of LQF
