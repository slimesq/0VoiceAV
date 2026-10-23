#include "videoencoder.h"
extern "C"
{
#include "libavutil/imgutils.h"
}
VideoEncoder::VideoEncoder()
{
}

VideoEncoder::~VideoEncoder()
{
    this->DeInit();
}

int VideoEncoder::InitH264(int width, int height, int fps, int bit_rate)
{
    this->DeInit();
    this->width_ = width;
    this->height_ = height;
    this->fps_ = fps;
    this->bit_rate_ = bit_rate;

    AVCodec const* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        printf("avcodec_find_encoder AV_CODEC_ID_H264 failed\n");
        return -1;
    }
    this->codec_ctx_ = avcodec_alloc_context3(codec);
    if (!this->codec_ctx_)
    {
        printf("avcodec_alloc_context3 AV_CODEC_ID_H264 failed\n");
        return -1;
    }

    this->codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    this->codec_ctx_->bit_rate = this->bit_rate_;
    this->codec_ctx_->width = this->width_;
    this->codec_ctx_->height = this->height_;
    this->codec_ctx_->framerate = {this->fps_, 1};
    // 视频的 如果不主动设置：The encoder timebase is not set
    this->codec_ctx_->time_base = {1, 1000000};  // 单位为微妙

    this->codec_ctx_->gop_size = this->fps_;
    this->codec_ctx_->max_b_frames = 0;
    this->codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    //    av_dict_set(&dict_, "tune", "zerolatency", 0);

    int ret = avcodec_open2(this->codec_ctx_, NULL, &this->dict_);
    if (ret != 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("avcodec_open2 failed:%s\n", errbuf);
        return -1;
    }

    this->frame_ = av_frame_alloc();
    if (!this->frame_)
    {
        printf("av_frame_alloc failed\n");
        return -1;
    }
    this->frame_->width = this->width_;
    this->frame_->height = this->height_;
    this->frame_->format = this->codec_ctx_->pix_fmt;

    printf("Inith264 success\n");
    return 0;
}

void VideoEncoder::DeInit()
{
    if (this->codec_ctx_)
    {
        avcodec_free_context(&this->codec_ctx_);
    }
    if (this->frame_)
    {
        av_frame_free(&this->frame_);
    }
    if (this->dict_)
    {
        av_dict_free(&this->dict_);
    }
}

AVPacket* VideoEncoder::Encode(
    uint8_t* yuv_data, int yuv_size, int stream_index, int64_t pts, int64_t time_base)
{
    if (!this->codec_ctx_)
    {
        printf("codec_ctx_ null\n");
        return NULL;
    }
    int ret = 0;

    pts = av_rescale_q(pts, AVRational{1, (int)time_base}, this->codec_ctx_->time_base);
    this->frame_->pts = pts;
    if (yuv_data)
    {
        int ret_size = av_image_fill_arrays(this->frame_->data,
                                            this->frame_->linesize,
                                            yuv_data,
                                            (AVPixelFormat)this->frame_->format,
                                            this->frame_->width,
                                            this->frame_->height,
                                            1);
        if (ret_size != yuv_size)
        {
            printf("ret_size:%d != yuv_size:%d -> failed\n", ret_size, yuv_size);
            return NULL;
        }
        ret = avcodec_send_frame(this->codec_ctx_, this->frame_);
    }
    else
    {
        ret = avcodec_send_frame(this->codec_ctx_, NULL);
    }

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
        printf("h264 avcodec_receive_packet failed:%s\n", errbuf);
        av_packet_free(&packet);
        return NULL;
    }
    packet->stream_index = stream_index;
    if (packet->duration == 0)
    {
        packet->duration = av_rescale_q(1, AVRational{1, this->fps_}, this->codec_ctx_->time_base);
    }
    return packet;
}

int VideoEncoder::Encode(uint8_t* yuv_data,
                         int yuv_size,
                         int stream_index,
                         int64_t pts,
                         int64_t time_base,
                         std::vector<AVPacket*>& packets)
{
    if (!this->codec_ctx_)
    {
        printf("codec_ctx_ null\n");
        return -1;
    }
    int ret = 0;

    pts = av_rescale_q(pts, AVRational{1, (int)time_base}, this->codec_ctx_->time_base);
    this->frame_->pts = pts;
    if (yuv_data)
    {
        int ret_size = av_image_fill_arrays(this->frame_->data,
                                            this->frame_->linesize,
                                            yuv_data,
                                            (AVPixelFormat)this->frame_->format,
                                            this->frame_->width,
                                            this->frame_->height,
                                            1);
        if (ret_size != yuv_size)
        {
            printf("ret_size:%d != yuv_size:%d -> failed\n", ret_size, yuv_size);
            return -1;
        }
        ret = avcodec_send_frame(this->codec_ctx_, this->frame_);
    }
    else
    {
        ret = avcodec_send_frame(this->codec_ctx_, NULL);
    }

    if (ret != 0)
    {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        printf("avcodec_send_frame failed:%s\n", errbuf);
        return -1;
    }
    while (1)
    {
        AVPacket* packet = av_packet_alloc();
        if (!packet)
        {
            return AVERROR(ENOMEM);
        }
        ret = avcodec_receive_packet(this->codec_ctx_, packet);
        packet->stream_index = stream_index;
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            ret = 0;
            av_packet_free(&packet);
            break;
        }
        else if (ret < 0)
        {
            char errbuf[1024] = {0};
            av_strerror(ret, errbuf, sizeof(errbuf) - 1);
            printf("h264 avcodec_receive_packet failed:%s\n", errbuf);
            av_packet_free(&packet);
            return ret;
        }
        if (packet->duration == 0)
        {
            packet->duration =
                av_rescale_q(1, AVRational{1, this->fps_}, this->codec_ctx_->time_base);
        }
        printf("h264 pts:%lld\n", packet->pts);
        packets.push_back(packet);
    }
    return ret;
}

AVCodecContext* VideoEncoder::GetCodecContext()
{
    return this->codec_ctx_;
}
