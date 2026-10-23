#include "audioresampler.h"

AudioResampler::AudioResampler()
{
}

AudioResampler::~AudioResampler()
{
    this->DeInit();
}

int AudioResampler::InitFromS16ToFLTP(int in_channels,
                                      int in_sample_rate,
                                      int out_channels,
                                      int out_sample_rate)
{
    this->DeInit();
    this->in_channels_ = in_channels;
    this->in_sample_rate_ = in_sample_rate;
    this->out_channels_ = out_channels;
    this->out_sample_rate_ = out_sample_rate;

    AVChannelLayout inputLayout{};
    AVChannelLayout outputLayout{};
    av_channel_layout_default(&inputLayout, this->in_channels_);
    av_channel_layout_default(&outputLayout, this->out_channels_);
    int ret = swr_alloc_set_opts2(&this->ctx_,
                                  &outputLayout,
                                  AV_SAMPLE_FMT_FLTP,
                                  this->out_sample_rate_,
                                  &inputLayout,
                                  AV_SAMPLE_FMT_S16,
                                  this->in_sample_rate_,
                                  0,
                                  NULL);
    av_channel_layout_uninit(&inputLayout);
    av_channel_layout_uninit(&outputLayout);
    if (ret < 0)
    {
        printf("swr_alloc_set_opts2 failed\n");
        return ret;
    }
    ret = swr_init(this->ctx_);
    if (ret < 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("swr_init  failed:%s\n", errbuf);
        return -1;
    }
    return 0;
}

int AudioResampler::ResampleFromS16ToFLTP(uint8_t* in_data, AVFrame* out_frame)
{
    if (!this->ctx_ || !in_data || !out_frame)
    {
        return AVERROR(EINVAL);
    }
    out_frame->sample_rate = this->out_sample_rate_;
    uint8_t const* indata[AV_NUM_DATA_POINTERS] = {0};
    indata[0] = in_data;
    int samples = swr_convert(
        this->ctx_, out_frame->data, out_frame->nb_samples, indata, out_frame->nb_samples);
    if (samples <= 0)
    {
        return -1;
    }
    out_frame->nb_samples = samples;
    return samples;
}

void AudioResampler::DeInit()
{
    if (this->ctx_)
    {
        swr_free(&this->ctx_);
    }
}

AVFrame* AllocFltpPcmFrame(int channels, int nb_samples)
{
    AVFrame* pcm = NULL;
    pcm = av_frame_alloc();
    if (!pcm)
    {
        return NULL;
    }
    pcm->format = AV_SAMPLE_FMT_FLTP;
    av_channel_layout_default(&pcm->ch_layout, channels);
    pcm->nb_samples = nb_samples;

    int ret = av_frame_get_buffer(pcm, 0);
    if (ret != 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("av_frame_get_buffer failed:%s\n", errbuf);
        av_frame_free(&pcm);
        return NULL;
    }
    return pcm;
}

void FreePcmFrame(AVFrame* frame)
{
    if (frame)
        av_frame_free(&frame);
}
