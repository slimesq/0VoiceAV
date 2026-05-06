#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "libavcodec/avcodec.h"
#include "libavcodec/codec.h"
#include "libavcodec/codec_id.h"
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/frame.h"
#include "libavutil/pixdesc.h"
#include "libavutil/samplefmt.h"

#define BUF_SIZE 20480

static char* av_get_err(int errnum)
{
    static char err_buf[128] = {0};
    av_strerror(errnum, err_buf, 128);
    return err_buf;
}

static void print_sample_format(AVFrame const* frame)
{
    printf("sample_rate:%d HZ\n", frame->sample_rate);
    printf("number of channels: %d\n", frame->ch_layout.nb_channels);
    printf("format:%u\n", frame->format);
}

static void decode(AVCodecContext* avctx, AVPacket* pkt, AVFrame* frame, FILE* out_file)
{
    int ret = avcodec_send_packet(avctx, pkt);
    if (ret != 0)
    {
        printf("avcodec_send_packet: %s\n", av_get_err(ret));
        return;
    }
    static int flag = 0;
    while (!avcodec_receive_frame(avctx, frame))
    {
        if (flag == 0)
        {
            flag = 1;
            print_sample_format(frame);

            size_t sample_size = av_get_bytes_per_sample(frame->format);

            for (int i = 0; i < frame->nb_samples; ++i)
            {
                for (int j = 0; j < frame->ch_layout.nb_channels; ++j)
                {
                    fwrite(frame->data[j] + sample_size * i, 1, sample_size, out_file);
                }
            }
        }
    }
}

int read_packet(void* opaque, uint8_t* buf, int buf_size)
{
    FILE* in_file = (FILE*)opaque;
    int read_size = fread(buf, 1, buf_size, in_file);
    printf("read_packet read_size:%d, buf_size:%d\n", read_size, buf_size);
    if (read_size < 0)
    {
        return AVERROR_EOF;
    }
    return read_size;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("usage: %s <intput file> <out file>\n", argv[0]);
        return -1;
    }
    char const* in_file_name = argv[1];
    char const* out_file_name = argv[2];
    FILE* in_file = NULL;
    FILE* out_file = NULL;

    // 1. 打开参数文件
    in_file = fopen(in_file_name, "rb");
    if (!in_file)
    {
        printf("open file %s failed\n", in_file_name);
        return -1;
    }
    out_file = fopen(out_file_name, "wb");
    if (!out_file)
    {
        printf("open file %s failed\n", out_file_name);
        return -1;
    }

    enum AVCodecID codec_id = AV_CODEC_ID_NONE;
    // 2. 找到指定的解码器并打开解码器上下文
    if (strstr(in_file_name, ".aac"))
    {
        codec_id = AV_CODEC_ID_AAC;
    }
    else if (strstr(in_file_name, ".mp3"))
    {
        codec_id = AV_CODEC_ID_MP3;
    }
    AVCodec const* codec = avcodec_find_decoder(codec_id);
    if (codec == NULL)
    {
        printf("avcodec_find_decoder failed\n");
        return -1;
    }
    AVCodecContext* avctx = avcodec_alloc_context3(codec);
    if (!avctx)
    {
        printf("avcodec_alloc_context3 failed\n");
        return -1;
    }
    int ret = avcodec_open2(avctx, codec, NULL);
    if (ret != 0)
    {
        printf("avcodec_open2 failed\n");
        return -1;
    }

    // 4. 注册avio_alloc_context
    unsigned char buf[BUF_SIZE] = {};
    AVIOContext* avio_ctx =
        avio_alloc_context(buf, BUF_SIZE, 0, (void*)in_file, read_packet, NULL, NULL);
    if (!avio_ctx)
    {
        printf("avio_alloc_context failed\n");
        fclose(out_file);
        fclose(in_file);
        return -1;
    }

    // 5. avformat_open_input
    AVFormatContext* fmt_ctx = avformat_alloc_context();
    fmt_ctx->pb = avio_ctx;
    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret != 0)
    {
        printf("avformat_open_input failed\n");
        fclose(out_file);
        fclose(in_file);
        return -1;
    }

    // 6. decode
    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
    {
        printf("av_packet_alloc failed\n");
        fclose(out_file);
        fclose(in_file);
        return -1;
    }
    AVFrame* frame = av_frame_alloc();
    if (!frame)
    {
        printf("av_frame_alloc failed\n");
        fclose(out_file);
        fclose(in_file);
        return -1;
    }

    while (!av_read_frame(fmt_ctx, pkt))
    {
        decode(avctx, pkt, frame, out_file);
    }

    printf("read file finish\n");
    // 冲刷解码器
    pkt->data = NULL;
    pkt->size = 0;
    decode(avctx, pkt, frame, out_file);

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avformat_close_input(&fmt_ctx);
    fclose(out_file);
    fclose(in_file);

    return 0;
}