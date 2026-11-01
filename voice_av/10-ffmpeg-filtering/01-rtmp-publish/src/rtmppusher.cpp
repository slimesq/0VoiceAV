#include "rtmppusher.h"
#include "dlog.h"
#include "timesutil.h"
RtmpPusher::RtmpPusher(MessageQueue* msg_queue) : msg_queue_(msg_queue)
{
    LogInfo("RtmpPusher create");
}

RtmpPusher::~RtmpPusher()
{
    DeInit();  // 释放资源
}

/**
 * @brief decode_interrupt_cb
 * @param ctx
 * @return false:继续阻塞;  true:退出阻塞
 */
static int decode_interrupt_cb(void* ctx)
{
    RtmpPusher* rtmp_puser = (RtmpPusher*)ctx;
    if (rtmp_puser->IsTimeout())
    {
        LogWarn("timeout:%dms", rtmp_puser->GetTimeout());
        return 1;
    }
    LogInfo("block time:%lld", rtmp_puser->GetBlockTime());
    return 0;
}

RET_CODE RtmpPusher::Init(Properties const& properties)
{
    url_ = properties.GetProperty("url", "");
    audio_frame_duration_ = properties.GetProperty("audio_frame_duration", 0);
    video_frame_duration_ = properties.GetProperty("video_frame_duration", 0);
    timeout_ = properties.GetProperty("timeout", 5000);  // 默认为5秒
    max_queue_duration_ = properties.GetProperty("max_queue_duration", 500);
    if (url_ == "")
    {
        LogError("url is null");
        return RET_FAIL;
    }
    int ret = 0;
    char str_error[512] = {0};
    // 初始化网络库
    ret = avformat_network_init();
    if (ret < 0)
    {
        av_strerror(ret, str_error, sizeof(str_error) - 1);
        LogError("avformat_network_init failed:%s", str_error);
        return RET_FAIL;
    }
    // 分配AVFormatContext
    ret = avformat_alloc_output_context2(&fmt_ctx_, NULL, "flv", url_.c_str());
    if (ret < 0)
    {
        av_strerror(ret, str_error, sizeof(str_error) - 1);
        LogError("avformat_alloc_output_context2 failed:%s", str_error);
        return RET_FAIL;
    }
    fmt_ctx_->interrupt_callback.callback = decode_interrupt_cb;  // 设置超时回调
    fmt_ctx_->interrupt_callback.opaque = this;
    // 创建队列
    queue_ = new PacketQueue(audio_frame_duration_, video_frame_duration_);
    if (!queue_)
    {
        LogError("new PacketQueue failed");
        return RET_ERR_OUTOFMEMORY;
    }
    return RET_OK;
}

void RtmpPusher::DeInit()  // 这个函数重复调用没有问题
{
    if (queue_)
    {
        queue_->Abort();
    }
    Stop();
    if (fmt_ctx_)
    {
        if (fmt_ctx_->pb && !(fmt_ctx_->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&fmt_ctx_->pb);
        }
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = NULL;
    }
    if (queue_)
    {
        delete queue_;
        queue_ = NULL;
    }
}

RET_CODE RtmpPusher::Push(AVPacket* pkt, MediaType media_type)
{
    if (!queue_)
    {
        return RET_FAIL;
    }
    int ret = queue_->Push(pkt, media_type);
    if (ret < 0)
    {
        return RET_FAIL;
    }
    else
    {
        return RET_OK;
    }
}

RET_CODE RtmpPusher::Connect()
{
    int ret = 0;
    if (!audio_stream_ && !video_stream_)
    {
        return RET_FAIL;
    }
    av_dump_format(fmt_ctx_, 0, url_.c_str(), 1);

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE))
    {
        RestTiemout();
        ret = avio_open2(&fmt_ctx_->pb,
                         url_.c_str(),
                         AVIO_FLAG_WRITE,
                         &fmt_ctx_->interrupt_callback,
                         nullptr);  // rtmp需要avio open
        if (ret < 0)
        {
            char str_error[512] = {0};
            av_strerror(ret, str_error, sizeof(str_error) - 1);
            LogError("avio_open failed:%s", str_error);
            return RET_FAIL;
        }
    }

    LogInfo("connect to:%s", url_.c_str());
    RestTiemout();
    // 连接服务器
    ret = avformat_write_header(fmt_ctx_, NULL);
    if (ret < 0)
    {
        char str_error[512] = {0};
        av_strerror(ret, str_error, sizeof(str_error) - 1);
        LogError("av_opt_set failed:%s", str_error);
        return RET_FAIL;
    }
    LogInfo("avformat_write_header ok");
    return Start();  // 启动线程
}

void RtmpPusher::Loop()
{
    LogInfo("Loop into");
    int ret = 0;
    AVPacket* pkt = NULL;
    MediaType media_type;
    while (true)
    {
        if (request_abort_)
        {
            LogInfo("abort request");
            break;
        }
        debugQueue(debug_interval_);
        checkPacketQueueDuration();  // 可以每隔一秒check一次
        ret = queue_->PopWithTimeout(&pkt, media_type, 1000);
        if (1 == ret)
        {  // 1代表 读取到消息
            if (request_abort_)
            {
                LogInfo("abort request");
                av_packet_free(&pkt);
                break;
            }
            switch (media_type)
            {
                case E_VIDEO_TYPE:
                    ret = sendPacket(pkt, media_type);
                    if (ret < 0)
                    {
                        LogError("send video Packet failed");
                    }
                    av_packet_free(&pkt);
                    break;
                case E_AUDIO_TYPE:
                    ret = sendPacket(pkt, media_type);
                    if (ret < 0)
                    {
                        LogError("send audio Packet failed");
                    }
                    av_packet_free(&pkt);
                    break;
                default:
                    av_packet_free(&pkt);
                    break;
            }
        }
    }
    RestTiemout();
    ret = av_write_trailer(fmt_ctx_);
    if (ret < 0)
    {
        char str_error[512] = {0};
        av_strerror(ret, str_error, sizeof(str_error) - 1);
        LogError("av_write_trailer failed:%s", str_error);
        return;
    }
    LogInfo("av_write_trailer ok");
}

bool RtmpPusher::IsTimeout()
{
    if (TimesUtil::GetTimeMillisecond() - pre_time_ > timeout_)
    {
        return true;  // 超时
    }
    return false;
}

void RtmpPusher::RestTiemout()
{
    pre_time_ = TimesUtil::GetTimeMillisecond();  // 重置为当前时间
}

int RtmpPusher::GetTimeout()
{
    return timeout_;
}

int64_t RtmpPusher::GetBlockTime()
{
    return TimesUtil::GetTimeMillisecond() - pre_time_;
}

void RtmpPusher::debugQueue(int64_t interval)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond();
    if (cur_time - pre_debug_time_ > interval)
    {
        // 打印信息
        PacketQueueStats stats;
        queue_->GetStats(&stats);
        LogInfo("duration:a-%lldms, v-%lldms", stats.audio_duration, stats.video_duration);
        pre_debug_time_ = cur_time;
    }
}

void RtmpPusher::checkPacketQueueDuration()
{
    PacketQueueStats stats;
    queue_->GetStats(&stats);
    if (stats.audio_duration > max_queue_duration_ || stats.video_duration > max_queue_duration_)
    {
        msg_queue_->notify_msg3(
            MSG_RTSP_QUEUE_DURATION, stats.audio_duration, stats.video_duration);
        LogWarn("drop packet -> a:%lld, v:%lld, th:%d",
                stats.audio_duration,
                stats.video_duration,
                max_queue_duration_);
        queue_->Drop(false, max_queue_duration_);
    }
}

int RtmpPusher::sendPacket(AVPacket* pkt, MediaType media_type)
{
    AVRational dst_time_base;
    AVRational src_time_base = {1, 1000};  // 我们采集、编码 时间戳单位都是ms
    if (E_VIDEO_TYPE == media_type)
    {
        pkt->stream_index = video_index_;
        dst_time_base = video_stream_->time_base;
    }
    else if (E_AUDIO_TYPE == media_type)
    {
        pkt->stream_index = audio_index_;
        dst_time_base = audio_stream_->time_base;
    }
    else
    {
        LogError("unknown mediatype:%d", media_type);
        return -1;
    }
    av_packet_rescale_ts(pkt, src_time_base, dst_time_base);
    RestTiemout();
    int ret = av_write_frame(fmt_ctx_, pkt);
    if (ret < 0)
    {
        msg_queue_->notify_msg2(MSG_RTSP_ERROR, ret);
        char str_error[512] = {0};
        av_strerror(ret, str_error, sizeof(str_error) - 1);
        LogError("av_write_frame failed:%s", str_error);  // 出错没有回调给PushWork
        return -1;
    }
    return 0;
}

RET_CODE RtmpPusher::ConfigVideoStream(AVCodecContext const* ctx)
{
    if (!fmt_ctx_)
    {
        LogError("fmt_ctx is null");
        return RET_FAIL;
    }
    if (!ctx)
    {
        LogError("ctx is null");
        return RET_FAIL;
    }
    // 添加视频流
    AVStream* vs = avformat_new_stream(fmt_ctx_, NULL);
    if (!vs)
    {
        LogError("avformat_new_stream failed");
        return RET_FAIL;
    }
    vs->codecpar->codec_tag = 0;
    // 从编码器拷贝信息
    if (avcodec_parameters_from_context(vs->codecpar, ctx) < 0)
    {
        return RET_FAIL;
    }
    vs->time_base = ctx->time_base;
    video_ctx_ = (AVCodecContext*)ctx;
    video_stream_ = vs;
    video_index_ = vs->index;  // 整个索引非常重要 fmt_ctx_根据index判别 音视频包
    return RET_OK;
}

RET_CODE RtmpPusher::ConfigAudioStream(AVCodecContext const* ctx)
{
    if (!fmt_ctx_)
    {
        LogError("fmt_ctx is null");
        return RET_FAIL;
    }
    if (!ctx)
    {
        LogError("ctx is null");
        return RET_FAIL;
    }
    // 添加视频流
    AVStream* as = avformat_new_stream(fmt_ctx_, NULL);
    if (!as)
    {
        LogError("avformat_new_stream failed");
        return RET_FAIL;
    }
    as->codecpar->codec_tag = 0;
    // 从编码器拷贝信息
    if (avcodec_parameters_from_context(as->codecpar, ctx) < 0)
    {
        return RET_FAIL;
    }
    as->time_base = ctx->time_base;
    audio_ctx_ = (AVCodecContext*)ctx;
    audio_stream_ = as;
    audio_index_ = as->index;  // 整个索引非常重要 fmt_ctx_根据index判别 音视频包
    return RET_OK;
}
