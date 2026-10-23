#include "muxer.h"

Muxer::Muxer()
{
}

Muxer::~Muxer()
{
    this->DeInit();
}

int Muxer::Init(char const* url)
{
    this->DeInit();
    int ret = avformat_alloc_output_context2(&this->fmt_ctx_, NULL, NULL, url);
    if (ret != 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("avformat_alloc_output_context2 failed:%s\n", errbuf);
        return -1;
    }
    this->url_ = url;
    return 0;
}

void Muxer::DeInit()
{
    if (this->fmt_ctx_)
    {
        if (!(this->fmt_ctx_->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&this->fmt_ctx_->pb);
        }
        avformat_free_context(this->fmt_ctx_);
        this->fmt_ctx_ = NULL;
    }
    this->url_ = "";
    this->aud_codec_ctx_ = NULL;
    this->aud_stream_ = NULL;
    this->audio_index_ = -1;

    this->vid_codec_ctx_ = NULL;
    this->vid_stream_ = NULL;
    this->video_index_ = -1;
}

int Muxer::AddStream(AVCodecContext* codec_ctx)
{
    if (!this->fmt_ctx_)
    {
        printf("fmt ctx is NULL\n");
        return -1;
    }
    if (!codec_ctx)
    {
        printf("codec ctx is NULL\n");
        return -1;
    }
    AVStream* st = avformat_new_stream(this->fmt_ctx_, NULL);
    if (!st)
    {
        printf("avformat_new_stream failed\n");
        return -1;
    }
    //    st->codecpar->codec_tag = 0;
    // 从编码器上下文复制
    int const ret = avcodec_parameters_from_context(st->codecpar, codec_ctx);
    if (ret < 0)
    {
        return ret;
    }
    st->time_base = codec_ctx->time_base;
    av_dump_format(this->fmt_ctx_, 0, this->url_.c_str(), 1);

    // 判断当前的是视频流还是音频流
    if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        this->aud_codec_ctx_ = codec_ctx;
        this->aud_stream_ = st;
        this->audio_index_ = st->index;
    }
    else if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        this->vid_codec_ctx_ = codec_ctx;
        this->vid_stream_ = st;
        this->video_index_ = st->index;
    }
    return 0;
}

int Muxer::SendHeader()
{
    if (!this->fmt_ctx_)
    {
        printf("fmt ctx is NULL\n");
        return -1;
    }
    int ret = avformat_write_header(this->fmt_ctx_, NULL);
    if (ret != 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("avformat_write_header failed:%s\n", errbuf);
        return -1;
    }
    return 0;
}

int Muxer::SendPacket(AVPacket* packet)
{
    if (!this->fmt_ctx_ || !packet || packet->size <= 0 || !packet->data)
    {
        printf("packet is null\n");
        if (packet)
            av_packet_free(&packet);

        return -1;
    }

    int const stream_index = packet->stream_index;
    AVRational src_time_base{};  // 编码后的包
    AVRational dst_time_base{};  // mp4输出文件对应流的time_base
    if (this->vid_stream_ && this->vid_codec_ctx_ && stream_index == this->video_index_)
    {
        src_time_base = this->vid_codec_ctx_->time_base;
        dst_time_base = this->vid_stream_->time_base;
    }
    else if (this->aud_stream_ && this->aud_codec_ctx_ && stream_index == this->audio_index_)
    {
        src_time_base = this->aud_codec_ctx_->time_base;
        dst_time_base = this->aud_stream_->time_base;
    }
    else
    {
        av_packet_free(&packet);
        return AVERROR(EINVAL);
    }
    // 时间基转换
    av_packet_rescale_ts(packet, src_time_base, dst_time_base);

    int ret = 0;
    ret = av_interleaved_write_frame(this->fmt_ctx_,
                                     packet);  // 不是立即写入文件，内部缓存，主要是对pts进行排序
    //    ret = av_write_frame(fmt_ctx_, packet);
    av_packet_free(&packet);
    if (ret == 0)
    {
        return 0;
    }
    else
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("avformat_write_header failed:%s\n", errbuf);
        return -1;
    }
}

int Muxer::SendTrailer()
{
    if (!this->fmt_ctx_)
    {
        printf("fmt ctx is NULL\n");
        return -1;
    }
    int ret = av_write_trailer(this->fmt_ctx_);
    if (ret != 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("av_write_trailer failed:%s\n", errbuf);
        return -1;
    }
    return 0;
}

int Muxer::Open()
{
    if (!this->fmt_ctx_)
    {
        return AVERROR(EINVAL);
    }
    if (this->fmt_ctx_->oformat->flags & AVFMT_NOFILE)
    {
        return 0;
    }
    return avio_open(&this->fmt_ctx_->pb, this->url_.c_str(), AVIO_FLAG_WRITE);
}
