#include "ffmpeg_channel_mask.h"
#include "aacencoder.h"
#include "dlog.h"

/**
 * @brief AACEncoder::AACEncoder
 * @param properties
 *      key value
 *      sample_fmt 默认AV_SAMPLE_FMT_FLTP
 *      samplerate 默认 48000
 *      ch_layout  默认AV_CH_LAYOUT_STEREO
 *      bitrate    默认out_samplerate*3
 */
AACEncoder::AACEncoder()
{

}

AACEncoder::~AACEncoder()
{
    if (ctx_)
        avcodec_free_context(&ctx_);

    if (frame_)
        av_frame_free(&frame_);
}

RET_CODE AACEncoder::Init(const Properties &properties)
{
    // 获取参数
    sample_rate_ = properties.GetProperty("sample_rate", 48000);
    bitrate_ = properties.GetProperty("bitrate", 128*1024);
    channels_ = properties.GetProperty("channels", 2);
    channel_layout_ = properties.GetProperty("channel_layout",
                                             (int)default_channel_mask(channels_));

    int ret;

    type_ = AudioCodec::AAC;
    // 读取默认的aac encoder
    codec_ = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if(!codec_)
    {
        LogError("AAC: No encoder found\n");
        return RET_ERR_MISMATCH_CODE;
    }

    ctx_ = avcodec_alloc_context3(codec_);
    if (ctx_ == NULL)
    {
        LogError("AAC: could not allocate context.\n");
        return RET_ERR_OUTOFMEMORY;
    }

    //Set params
    if (av_channel_layout_from_mask(&ctx_->ch_layout, channel_layout_) < 0)
        return RET_FAIL;
    ctx_->sample_fmt		= AV_SAMPLE_FMT_FLTP;
    ctx_->sample_rate	= sample_rate_;
    ctx_->bit_rate		= bitrate_;
    ctx_->thread_count	= 1;

    //Allow experimental codecs
    ctx_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    if (avcodec_open2(ctx_, codec_, NULL) < 0)
    {
        LogError("AAC: could not open codec\n");
        return RET_FAIL;
    }


    frame_byte_size_ = av_get_bytes_per_sample(ctx_->sample_fmt)
            * ctx_->ch_layout.nb_channels * ctx_->frame_size;


    //Create frame
    frame_ = av_frame_alloc();
    //Set defaults
    frame_->nb_samples     = ctx_->frame_size;
    frame_->format         = ctx_->sample_fmt;
    av_channel_layout_copy(&frame_->ch_layout, &ctx_->ch_layout);
    frame_->sample_rate    = ctx_->sample_rate;
    //分配data buf
    ret = av_frame_get_buffer(frame_, 0);

    //Log
    LogInfo("AAC: Encoder open with frame sample size %d.\n",  ctx_->frame_size);

    return RET_OK;
}



int AACEncoder::Encode(AVFrame *frame, uint8_t *out, int out_len)
{
    if (!frame)
        return 0;
    if (!ctx_ || !out || out_len < 0)
        return -1;

    AVPacket packet = {};
    int ret = avcodec_send_frame(ctx_, frame);
    if (ret >= 0)
        ret = avcodec_receive_packet(ctx_, &packet);
    int size = -1;
    if (ret >= 0 && packet.size <= out_len) {
        memcpy(out, packet.data, packet.size);
        size = packet.size;
    }
    av_packet_unref(&packet);
    return size;
}

AVPacket * AACEncoder::Encode(AVFrame *frame, int64_t pts, const int flush)
{
    AVPacket *packet = NULL;
    int ret = 0;
    AVRational src_time_base = AVRational{1, 1000};
    frame->pts = pts;

    if (ctx_ == NULL)
    {
        LogError("AAC: no context.\n");
        return NULL;
    }
    if(frame){
        int ret = avcodec_send_frame(ctx_, frame);
        if (ret != 0) {
            LogError("avcodec_send_frame failed");
            return NULL;
        }
    }

    packet = av_packet_alloc();
    ret = avcodec_receive_packet(ctx_, packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return NULL;
    }
    else if (ret < 0) {
        LogError("avcodec_receive_packet() failed.");
        return NULL;
    }
    //    LogInfo("apts1:%ld", frame_->pts);
    //    LogInfo("apts2:%ld", packet->pts);
    return packet;
}

/**
 * @brief 每次需要输入足够进行一次编码的数据
 * @param in
 * @param size
 * @return
 */
RET_CODE AACEncoder::EncodeInput(const uint8_t *in, const uint32_t size)
{
    RET_CODE ret_code = RET_OK;
    if(in)
    {
        uint32_t need_size = av_get_bytes_per_sample(ctx_->sample_fmt) * ctx_->ch_layout.nb_channels *
                ctx_->frame_size;
        if(size != need_size)
        {
            LogError("need size:%u, but the size:%u", need_size, size);
            return RET_ERR_PARAMISMATCH;
        }
        AVFrame *frame = av_frame_alloc();
        if (!frame)
            return RET_ERR_OUTOFMEMORY;
        frame->nb_samples = ctx_->frame_size;
        frame->format = ctx_->sample_fmt;
        frame->sample_rate = ctx_->sample_rate;
        int ret = av_channel_layout_copy(&frame->ch_layout, &ctx_->ch_layout);
        if (ret >= 0)
            ret = avcodec_fill_audio_frame(frame, ctx_->ch_layout.nb_channels,
                                          ctx_->sample_fmt, in, size, 0);
        ret_code = ret < 0 ? RET_FAIL : EncodeInput(frame);
        av_frame_free(&frame);  // 释放自己申请的数据
        return ret_code;
    }
    else // 为null时flush编码器
    {
        return EncodeInput(NULL);
    }
}

RET_CODE AACEncoder::EncodeInput(const AVFrame *frame)
{
    int ret = avcodec_send_frame(ctx_, frame);
    if(ret != 0)
    {
        if(AVERROR(EAGAIN) == ret)
        {
            LogWarn("please receive packet then send frame");
            return RET_ERR_EAGAIN;
        }
        else if(AVERROR_EOF == ret)
        {
            LogWarn("if you wan continue use it, please new one decoder again");
        }
        return RET_FAIL;
    }
    return RET_OK;
}

RET_CODE AACEncoder::EncodeOutput(AVPacket *pkt)
{
    int ret = avcodec_receive_packet(ctx_, pkt);
    if(ret != 0)
    {
        if(AVERROR(EAGAIN) == ret)
        {
            LogWarn("output is not available in the current state - user must try to send input");
            return RET_ERR_EAGAIN;
        }
        else if(AVERROR_EOF == ret)
        {
            LogWarn("the encoder has been fully flushed, and there will be no more output packets");
            return RET_ERR_EOF;
        }
        return RET_FAIL;
    }
    return RET_OK;
}

RET_CODE AACEncoder::EncodeOutput(uint8_t *out, uint32_t &size)
{
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = out;
    pkt.size = size;

    RET_CODE ret = EncodeOutput(&pkt);
    size = pkt.size;        // 实际编码后的数据长度
    return ret;
}
