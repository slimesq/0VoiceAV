/*
 * Copyright (c) 2017 Jun Zhao
 * Copyright (c) 2017 Kaixuan Liu
 *
 * Adapted from the 0Voice 07-11-hw-decode example.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>

typedef struct DecodeOutput
{
    FILE* file;
    AVFrame* frame;
    AVFrame* downloaded;
    uint8_t* buffer;
    unsigned int buffer_capacity;
    unsigned int frame_count;
    enum AVPixelFormat pixel_format;
    enum AVPixelFormat hw_pixel_format;
    int width;
    int height;
} DecodeOutput;

static enum AVPixelFormat select_hw_format(AVCodecContext* decoder, const enum AVPixelFormat* formats)
{
    const DecodeOutput* output = decoder->opaque;
    for (const enum AVPixelFormat* format = formats; *format != AV_PIX_FMT_NONE; ++format)
    {
        if (*format == output->hw_pixel_format)
            return *format;
    }
    return AV_PIX_FMT_NONE;
}

static int write_frame(DecodeOutput* output)
{
    AVFrame* frame = output->frame;
    if (frame->format == output->hw_pixel_format)
    {
        // Transfer the hardware surface to CPU memory before writing it.
        const int ret = av_hwframe_transfer_data(output->downloaded, frame, 0);
        if (ret < 0)
            return ret;
        frame = output->downloaded;
    }

    if (!output->frame_count)
    {
        output->width = frame->width;
        output->height = frame->height;
        output->pixel_format = frame->format;
        const char* name = av_get_pix_fmt_name(output->pixel_format);
        printf("Raw video: %dx%d, pixel format %s\n", frame->width, frame->height,
               name ? name : "unknown");
    }
    else if (frame->width != output->width || frame->height != output->height ||
             frame->format != output->pixel_format)
    {
        fprintf(stderr, "Frame dimensions or pixel format changed during raw video output\n");
        return AVERROR_INVALIDDATA;
    }

    const int size = av_image_get_buffer_size(frame->format, frame->width, frame->height, 1);
    if (size < 0)
        return size;
    av_fast_malloc(&output->buffer, &output->buffer_capacity, size);
    if (!output->buffer)
        return AVERROR(ENOMEM);

    const int written = av_image_copy_to_buffer(output->buffer, size,
                                                (const uint8_t* const*)frame->data,
                                                frame->linesize, frame->format, frame->width,
                                                frame->height, 1);
    if (written < 0)
        return written;
    if (fwrite(output->buffer, 1, written, output->file) != (size_t)written)
        return AVERROR(EIO);
    ++output->frame_count;
    return 0;
}

static int decode_packet(AVCodecContext* decoder, const AVPacket* packet, DecodeOutput* output)
{
    int ret = avcodec_send_packet(decoder, packet);
    if (ret < 0)
        return ret;

    for (;;)
    {
        ret = avcodec_receive_frame(decoder, output->frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;
        ret = write_frame(output);
        av_frame_unref(output->downloaded);
        av_frame_unref(output->frame);
        if (ret < 0)
            return ret;
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <device type> <input file> <output raw video>\n", argv[0]);
        return EXIT_FAILURE;
    }

    enum AVHWDeviceType type = av_hwdevice_find_type_by_name(argv[1]);
    if (type == AV_HWDEVICE_TYPE_NONE)
    {
        fprintf(stderr, "Unknown device type %s. Available types:", argv[1]);
        while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
            fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
        fputc('\n', stderr);
        return EXIT_FAILURE;
    }

    AVFormatContext* input = NULL;
    AVCodecContext* decoder = NULL;
    const AVCodec* codec = NULL;
    AVPacket* packet = NULL;
    DecodeOutput output = {0};
    const char* operation = "Open input";
    int ret = avformat_open_input(&input, argv[2], NULL, NULL);
    if (ret < 0)
        goto cleanup;

    operation = "Find stream information";
    ret = avformat_find_stream_info(input, NULL);
    if (ret < 0)
        goto cleanup;
    operation = "Find video stream";
    const int video = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (video < 0)
    {
        ret = video;
        goto cleanup;
    }

    output.hw_pixel_format = AV_PIX_FMT_NONE;
    for (int i = 0;; ++i)
    {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
        if (!config)
            break;
        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == type)
        {
            output.hw_pixel_format = config->pix_fmt;
            break;
        }
    }
    if (output.hw_pixel_format == AV_PIX_FMT_NONE)
    {
        fprintf(stderr, "Decoder %s has no hardware configuration for %s\n", codec->name, argv[1]);
        operation = "Select hardware decoder configuration";
        ret = AVERROR(ENOSYS);
        goto cleanup;
    }

    operation = "Allocate decoder and buffers";
    decoder = avcodec_alloc_context3(codec);
    packet = av_packet_alloc();
    output.frame = av_frame_alloc();
    output.downloaded = av_frame_alloc();
    if (!decoder || !packet || !output.frame || !output.downloaded)
    {
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }
    operation = "Copy codec parameters";
    ret = avcodec_parameters_to_context(decoder, input->streams[video]->codecpar);
    if (ret < 0)
        goto cleanup;
    decoder->get_format = select_hw_format;
    decoder->opaque = &output;

    operation = "Create hardware device";
    ret = av_hwdevice_ctx_create(&decoder->hw_device_ctx, type, NULL, NULL, 0);
    if (ret < 0)
        goto cleanup;
    operation = "Open decoder";
    ret = avcodec_open2(decoder, codec, NULL);
    if (ret < 0)
        goto cleanup;

    operation = "Open raw video output";
    output.file = fopen(argv[3], "wb");
    if (!output.file)
    {
        ret = AVERROR(errno);
        goto cleanup;
    }

    operation = "Read input";
    while ((ret = av_read_frame(input, packet)) >= 0)
    {
        if (packet->stream_index == video)
        {
            ret = decode_packet(decoder, packet, &output);
            if (ret < 0)
            {
                operation = "Decode or write video frame";
                goto cleanup;
            }
        }
        av_packet_unref(packet);
    }
    if (ret == AVERROR_EOF)
    {
        operation = "Drain decoder";
        ret = decode_packet(decoder, NULL, &output);
    }

cleanup:
    if (output.file && fclose(output.file) != 0 && ret >= 0)
    {
        operation = "Close raw video output";
        ret = AVERROR(EIO);
    }
    av_free(output.buffer);
    av_frame_free(&output.downloaded);
    av_frame_free(&output.frame);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
    avformat_close_input(&input);
    if (ret < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error, sizeof(error));
        fprintf(stderr, "%s: %s\n", operation, error);
        return EXIT_FAILURE;
    }
    printf("Decoded frames: %u\n", output.frame_count);
    return EXIT_SUCCESS;
}
