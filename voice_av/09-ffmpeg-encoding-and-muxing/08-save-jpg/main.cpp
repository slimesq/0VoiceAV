// 本程序主要解决一位学员将视频转换成jpeg和ppm格式。
// 腾讯课堂 零声教育 Darren 音视频高级开发课程
// 使用说明，输入要转成图片的视频路径， 默认只能转5张图片
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avio.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef __cplusplus
}
#endif

AVFrame* allocate_sws_frame(AVCodecContext* video_enc_ctx)
{
    int ret = 0;
    AVFrame* sws_frame = av_frame_alloc();
    if (sws_frame)
    {
        sws_frame->format = video_enc_ctx->pix_fmt;
        sws_frame->width = video_enc_ctx->width;
        sws_frame->height = video_enc_ctx->height;
        sws_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = av_frame_get_buffer(sws_frame, 32);  // 分配buffer
        if (ret < 0)
        {
            av_frame_free(&sws_frame);
            return NULL;
        }
    }
    return sws_frame;
}

// Encode one decoded frame with the course's JPEG/PPM conversion pipeline.
static int encodeAndSaveImage(char const* out_file_name,
                              AVFrame* src_frame,
                              AVCodecID codecId,
                              AVPixelFormat pixelFormat,
                              int jpeg_quality)
{
    AVFormatContext* ofmt_ctx{nullptr};
    AVCodecContext* enc_ctx{nullptr};
    AVStream* video_st{nullptr};
    AVFrame* picture{nullptr};
    AVPacket* pkt{nullptr};
    SwsContext* img_convert_ctx{nullptr};
    AVDictionary* options{nullptr};
    AVCodec const* encoder = avcodec_find_encoder(codecId);
    char const* muxerName = codecId == AV_CODEC_ID_MJPEG ? "mjpeg" : "image2";
    bool headerWritten{false};
    bool draining{false};
    bool wroteImage{false};
    int ret{0};
    if (!encoder)
        return AVERROR_ENCODER_NOT_FOUND;
    ret = avformat_alloc_output_context2(&ofmt_ctx, nullptr, muxerName, out_file_name);
    if (ret < 0)
        goto fail;
    video_st = avformat_new_stream(ofmt_ctx, nullptr);
    enc_ctx = avcodec_alloc_context3(encoder);
    pkt = av_packet_alloc();
    picture = av_frame_alloc();
    if (!video_st || !enc_ctx || !pkt || !picture)
    {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    enc_ctx->pix_fmt = pixelFormat;
    enc_ctx->width = src_frame->width;
    enc_ctx->height = src_frame->height;
    enc_ctx->time_base = AVRational{1, 25};
    if (jpeg_quality > 0)
    {
        enc_ctx->qcompress = static_cast<float>(jpeg_quality > 100 ? 100 : jpeg_quality) / 100.f;
        enc_ctx->qmin = 2;
        enc_ctx->qmax = 31;
        enc_ctx->max_qdiff = 3;
    }
    ret = avcodec_open2(enc_ctx, encoder, nullptr);
    if (ret < 0)
        goto fail;
    ret = avcodec_parameters_from_context(video_st->codecpar, enc_ctx);
    if (ret < 0)
        goto fail;
    video_st->time_base = enc_ctx->time_base;
    if (src_frame->format == pixelFormat)
    {
        ret = av_frame_ref(picture, src_frame);
        if (ret < 0)
            goto fail;
    }
    else
    {
        picture->format = pixelFormat;
        picture->width = enc_ctx->width;
        picture->height = enc_ctx->height;
        ret = av_frame_get_buffer(picture, 32);
        if (ret < 0)
            goto fail;
        img_convert_ctx = sws_getContext(src_frame->width,
                                         src_frame->height,
                                         static_cast<AVPixelFormat>(src_frame->format),
                                         enc_ctx->width,
                                         enc_ctx->height,
                                         pixelFormat,
                                         SWS_BICUBIC,
                                         nullptr,
                                         nullptr,
                                         nullptr);
        if (!img_convert_ctx)
        {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        ret = sws_scale(img_convert_ctx,
                        src_frame->data,
                        src_frame->linesize,
                        0,
                        src_frame->height,
                        picture->data,
                        picture->linesize);
        if (ret <= 0)
        {
            ret = AVERROR(EINVAL);
            goto fail;
        }
    }
    picture->pts = 0;
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, out_file_name, AVIO_FLAG_WRITE);
        if (ret < 0)
            goto fail;
    }
    else
    {
        av_dict_set(&options, "update", "1", 0);
    }
    ret = avformat_write_header(ofmt_ctx, &options);
    av_dict_free(&options);
    if (ret < 0)
        goto fail;
    headerWritten = true;
    ret = avcodec_send_frame(enc_ctx, picture);
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) && !draining)
        {
            draining = true;
            ret = avcodec_send_frame(enc_ctx, nullptr);
            continue;
        }
        if (ret == AVERROR_EOF)
        {
            ret = wroteImage ? 0 : AVERROR_INVALIDDATA;
            break;
        }
        if (ret < 0)
            break;
        pkt->stream_index = video_st->index;
        av_packet_rescale_ts(pkt, enc_ctx->time_base, video_st->time_base);
        ret = av_interleaved_write_frame(ofmt_ctx, pkt);
        av_packet_unref(pkt);
        wroteImage = ret >= 0;
    }
fail:
    if (headerWritten)
    {
        int const trailerResult = av_write_trailer(ofmt_ctx);
        if (ret >= 0)
            ret = trailerResult;
    }
    av_dict_free(&options);
    av_packet_free(&pkt);
    av_frame_free(&picture);
    avcodec_free_context(&enc_ctx);
    sws_freeContext(img_convert_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    return ret;
}

int encode_and_save_jpeg(char const* out_file_name, AVFrame* src_frame, int jpeg_quality)
{
    return encodeAndSaveImage(
        out_file_name, src_frame, AV_CODEC_ID_MJPEG, AV_PIX_FMT_YUVJ420P, jpeg_quality);
}

int encode_and_save_ppm(char const* out_file_name, AVFrame* src_frame)
{
    return encodeAndSaveImage(out_file_name, src_frame, AV_CODEC_ID_PPM, AV_PIX_FMT_RGB24, 0);
}

///现在我们需要做的是让SaveFrame函数能把RGB信息定稿到一个PPM格式的文件中。
///我们将生成一个简单的PPM格式文件，请相信，它是可以工作的。
void SaveFrame(AVFrame* dec_frame, int width, int height, int index)
{
    FILE* pFile;
    char jpg_file_name[32];
    int y;

    // Open file
    sprintf(jpg_file_name, "frame%d.ppm", index);
    pFile = fopen(jpg_file_name, "wb");

    if (pFile == NULL)
        return;

    // Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);

    // Write pixel data
    for (y = 0; y < height; y++)
    {
        fwrite(dec_frame->data[0] + y * dec_frame->linesize[0], 1, width * 3, pFile);
    }

    // Close file
    fclose(pFile);
}

static int saveDecodedFrames(
    AVCodecContext* decoder, AVFrame* frame, int* index, int pictureCount, int quality)
{
    while (*index < pictureCount)
    {
        int ret = avcodec_receive_frame(decoder, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;
        char fileName[64]{};
        snprintf(fileName, sizeof(fileName), "frame%d.jpg", *index);
        ret = encode_and_save_jpeg(fileName, frame, quality);
        if (ret >= 0)
        {
            snprintf(fileName, sizeof(fileName), "frame%d.ppm", *index);
            ret = encode_and_save_ppm(fileName, frame);
        }
        av_frame_unref(frame);
        if (ret < 0)
            return ret;
        ++*index;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage: %s input_video [picture_count]\n", argv[0]);
        return 1;
    }
    int const pictureCount = argc == 3 ? atoi(argv[2]) : 5;
    if (pictureCount <= 0)
    {
        fprintf(stderr, "picture_count must be positive\n");
        return 1;
    }
    AVFormatContext* ifmt_ctx{nullptr};
    AVCodecContext* video_dec_ctx{nullptr};
    AVCodec const* video_dec{nullptr};
    AVFrame* dec_frame{nullptr};
    AVPacket* packet{nullptr};
    int index{0};
    int video_index{-1};
    int ret = avformat_open_input(&ifmt_ctx, argv[1], nullptr, nullptr);
    if (ret < 0)
        goto fail;
    ret = avformat_find_stream_info(ifmt_ctx, nullptr);
    if (ret < 0)
        goto fail;
    video_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &video_dec, 0);
    if (video_index < 0)
    {
        ret = video_index;
        goto fail;
    }
    video_dec_ctx = avcodec_alloc_context3(video_dec);
    packet = av_packet_alloc();
    dec_frame = av_frame_alloc();
    if (!video_dec_ctx || !packet || !dec_frame)
    {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ret = avcodec_parameters_to_context(video_dec_ctx, ifmt_ctx->streams[video_index]->codecpar);
    if (ret < 0)
        goto fail;
    ret = avcodec_open2(video_dec_ctx, video_dec, nullptr);
    if (ret < 0)
        goto fail;
    while (index < pictureCount && (ret = av_read_frame(ifmt_ctx, packet)) >= 0)
    {
        if (packet->stream_index == video_index)
        {
            ret = avcodec_send_packet(video_dec_ctx, packet);
            if (ret >= 0)
                ret = saveDecodedFrames(video_dec_ctx, dec_frame, &index, pictureCount, 80);
        }
        av_packet_unref(packet);
        if (ret < 0)
            goto fail;
    }
    if (ret == AVERROR_EOF && index < pictureCount)
    {
        ret = avcodec_send_packet(video_dec_ctx, nullptr);
        if (ret >= 0)
            ret = saveDecodedFrames(video_dec_ctx, dec_frame, &index, pictureCount, 80);
    }
    if (ret >= 0)
        printf("Saved %d JPEG/PPM frame pairs\n", index);
fail:
    av_packet_free(&packet);
    av_frame_free(&dec_frame);
    avcodec_free_context(&video_dec_ctx);
    avformat_close_input(&ifmt_ctx);
    if (ret < 0)
    {
        char error[128]{};
        av_strerror(ret, error, sizeof(error));
        fprintf(stderr, "Image extraction failed: %s\n", error);
    }
    return ret < 0 ? 1 : 0;
}
