#include "audioencoder.h"

AudioEncoder::AudioEncoder()
{
}

AudioEncoder::~AudioEncoder()
{
    this->DeInit();
}

int AudioEncoder::InitAAC(int channels, int sample_rate, int bit_rate)
{
    this->DeInit();
    this->channels_ = channels;
    this->sample_rate_ = sample_rate;
    this->bit_rate_ = bit_rate;

    AVCodec const* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec)
    {
        printf("avcodec_find_encoder AV_CODEC_ID_AAC failed\n");
        return -1;
    }
    this->codec_ctx_ = avcodec_alloc_context3(codec);
    if (!this->codec_ctx_)
    {
        printf("avcodec_alloc_context3 AV_CODEC_ID_AAC failed\n");
        return -1;
    }

    this->codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    this->codec_ctx_->bit_rate = this->bit_rate_;
    this->codec_ctx_->sample_rate = this->sample_rate_;
    this->codec_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    av_channel_layout_default(&this->codec_ctx_->ch_layout, this->channels_);
    this->codec_ctx_->time_base = AVRational{1, this->sample_rate_};

    int ret = avcodec_open2(this->codec_ctx_, NULL, NULL);
    if (ret != 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("avcodec_open2 failed:%s\n", errbuf);
        return -1;
    }
    printf("InitAAC success\n");
    return 0;
}

void AudioEncoder::DeInit()
{
    if (this->codec_ctx_)
    {
        avcodec_free_context(&this->codec_ctx_);  // codec_ctx_被设置为NULL
                                                  //        codec_ctx_ = NULL;  // 不需要再写
    }
}

AVPacket* AudioEncoder::Encode(AVFrame* frame, int stream_index, int64_t pts, int64_t time_base)
{
    if (!this->codec_ctx_)
    {
        printf("codec_ctx_ null\n");
        return NULL;
    }
    pts = av_rescale_q(pts, AVRational{1, (int)time_base}, this->codec_ctx_->time_base);
    if (frame)
    {
        frame->pts = pts;
    }
    int ret = avcodec_send_frame(this->codec_ctx_, frame);
    if (ret != 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("avcodec_send_frame failed:%s\n", errbuf);
        return NULL;
    }
    AVPacket* packet = av_packet_alloc();
    if (!packet)
    {
        return NULL;
    }
    ret = avcodec_receive_packet(this->codec_ctx_, packet);
    if (ret != 0)
    {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_packet_free(&packet);
            return NULL;
        }
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("aac avcodec_receive_packet failed:%s\n", errbuf);
        av_packet_free(&packet);
        return NULL;
    }
    packet->stream_index = stream_index;
    return packet;
}

int AudioEncoder::GetFrameSize()
{
    if (this->codec_ctx_)
        return this->codec_ctx_->frame_size;
    return 0;
}

int AudioEncoder::GetSampleFormat()
{
    if (this->codec_ctx_)
        return this->codec_ctx_->sample_fmt;

    return -1;  // AV_SAMPLE_FMT_NONE
}

AVCodecContext* AudioEncoder::GetCodecContext()
{
    return this->codec_ctx_;
}

int AudioEncoder::GetChannels()
{
    if (this->codec_ctx_)
        return this->codec_ctx_->ch_layout.nb_channels;

    return -1;
}

int AudioEncoder::GetSampleRate()
{
    if (this->codec_ctx_)
        return this->codec_ctx_->sample_rate;

    return -1;
}
