#include "util.h"

#include <errno.h>
#include <stdio.h>

// Read one tightly packed image and copy it into frame-owned buffers.
AVFrame *get_frame_from_yuvile(const char *filename, int width, int height, int format)
{
    AVFrame *frame = av_frame_malloc(width, height, format);
    FILE *infile = NULL;
    uint8_t *data = NULL;
    uint8_t *source_data[4] = {0};
    int source_linesize[4] = {0};
    const int image_size = av_image_get_buffer_size(format, width, height, 1);
    if (!frame || image_size < 0)
        goto fail;
    infile = fopen(filename, "rb");
    data = av_malloc(image_size);
    if (!infile || !data || fread(data, 1, image_size, infile) != (size_t)image_size)
        goto fail;
    if (av_image_fill_arrays(source_data, source_linesize, data, format, width, height, 1) < 0)
        goto fail;
    av_image_copy(frame->data, frame->linesize, (const uint8_t **)source_data,
                  source_linesize, format, width, height);
    fclose(infile);
    av_free(data);
    return frame;

fail:
    if (infile)
        fclose(infile);
    av_free(data);
    av_frame_free(&frame);
    return NULL;
}

int save_frame_to_i420p(const char *filename, const AVFrame *frame)
{
    if (!frame || frame->format != AV_PIX_FMT_YUV420P)
        return AVERROR(EINVAL);
    const int image_size = av_image_get_buffer_size(frame->format, frame->width, frame->height, 1);
    if (image_size < 0)
        return image_size;
    uint8_t *data = av_malloc(image_size);
    if (!data)
        return AVERROR(ENOMEM);
    int ret = av_image_copy_to_buffer(data, image_size, (const uint8_t * const *)frame->data,
                                      frame->linesize, frame->format, frame->width, frame->height, 1);
    FILE *outfile = NULL;
    if (ret < 0)
        goto end;
    outfile = fopen(filename, "wb");
    if (!outfile || fwrite(data, 1, image_size, outfile) != (size_t)image_size)
        ret = AVERROR(EIO);
    else
        ret = 0;

end:
    if (outfile && fclose(outfile) != 0)
        ret = AVERROR(EIO);
    av_free(data);
    return ret;
}

int save_frame_to_jpeg(const char *filename, const AVFrame *frame)
{
    int ret = AVERROR(EINVAL);
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    AVCodecContext *codec_ctx = NULL;
    AVPacket *packet = NULL;
    AVFrame *jpeg_frame = NULL;
    AVDictionary *options = NULL;
    FILE *outfile = NULL;
    if (!frame || (frame->format != AV_PIX_FMT_YUV420P && frame->format != AV_PIX_FMT_YUVJ420P))
        goto end;
    if (!codec) {
        ret = AVERROR_ENCODER_NOT_FOUND;
        goto end;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    packet = av_packet_alloc();
    jpeg_frame = av_frame_clone(frame);
    if (!codec_ctx || !packet || !jpeg_frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    codec_ctx->width = frame->width;
    codec_ctx->height = frame->height;
    codec_ctx->time_base = (AVRational){1, 25};
    codec_ctx->framerate = (AVRational){25, 1};
    jpeg_frame->format = codec_ctx->pix_fmt;
    av_dict_set(&options, "flags", "+qscale", 0);
    av_dict_set(&options, "qmax", "2", 0);
    av_dict_set(&options, "qmin", "2", 0);
    ret = avcodec_open2(codec_ctx, codec, &options);
    if (ret < 0)
        goto end;
    ret = avcodec_send_frame(codec_ctx, jpeg_frame);
    if (ret < 0)
        goto end;
    ret = avcodec_receive_packet(codec_ctx, packet);
    if (ret == AVERROR(EAGAIN)) {
        ret = avcodec_send_frame(codec_ctx, NULL);
        if (ret >= 0)
            ret = avcodec_receive_packet(codec_ctx, packet);
    }
    if (ret < 0)
        goto end;
    outfile = fopen(filename, "wb");
    if (!outfile || fwrite(packet->data, 1, packet->size, outfile) != (size_t)packet->size)
        ret = AVERROR(EIO);

end:
    if (outfile && fclose(outfile) != 0)
        ret = AVERROR(EIO);
    av_dict_free(&options);
    av_packet_free(&packet);
    av_frame_free(&jpeg_frame);
    avcodec_free_context(&codec_ctx);
    return ret;
}

AVFrame *get_frame_from_jpegfile(const char *filename)
{
    int ret = 0;
    int stream_index = -1;
    AVFormatContext *format_ctx = NULL;
    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    ret = avformat_open_input(&format_ctx, filename, NULL, NULL);
    if (ret < 0)
        goto end;
    ret = avformat_find_stream_info(format_ctx, NULL);
    if (ret < 0)
        goto end;
    stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (stream_index < 0) {
        ret = stream_index;
        goto end;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!codec_ctx || !packet || !frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    ret = avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar);
    if (ret < 0 || (ret = avcodec_open2(codec_ctx, codec, NULL)) < 0)
        goto end;
    for (;;) {
        ret = av_read_frame(format_ctx, packet);
        if (ret == AVERROR_EOF) {
            ret = avcodec_send_packet(codec_ctx, NULL);
        } else if (ret >= 0) {
            if (packet->stream_index != stream_index) {
                av_packet_unref(packet);
                continue;
            }
            ret = avcodec_send_packet(codec_ctx, packet);
        }
        av_packet_unref(packet);
        if (ret < 0)
            break;
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret != AVERROR(EAGAIN))
            break;
    }

end:
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    if (ret < 0)
        av_frame_free(&frame);
    return frame;
}

AVFrame *av_frame_malloc(int width, int height, int format)
{
    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return NULL;
    frame->width = width;
    frame->height = height;
    frame->format = format;
    if (av_frame_get_buffer(frame, 1) < 0)
        av_frame_free(&frame);
    return frame;
}
