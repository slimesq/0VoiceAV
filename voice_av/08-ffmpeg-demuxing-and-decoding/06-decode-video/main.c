/**
 * @projectName   voice_av_08_06_decode_video
 * @brief         解码视频，主要的测试格式h264和mpeg2
 *
 * @author        Liao Qingfu
 * @date          2020-01-16
 */
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libavcodec/codec.h"
#include "libavcodec/codec_id.h"
#include "libavcodec/packet.h"
#include "libavutil/avutil.h"

#define VIDEO_INBUF_SIZE 20480
#define VIDEO_REFILL_THRESH 4096

static char err_buf[128] = {0};
static char* av_get_err(int errnum)
{
    av_strerror(errnum, err_buf, 128);
    return err_buf;
}

static void print_video_format(AVFrame const* frame)
{
    printf("width: %u\n", frame->width);
    printf("height: %u\n", frame->height);
    printf("format: %u\n", frame->format);  // 格式需要注意
}

static void decode(AVCodecContext* dec_ctx, AVPacket* pkt, AVFrame* frame, FILE* outfile)
{
    int ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret == AVERROR(EAGAIN))
    {
        fprintf(stderr,
                "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
    }
    else if (ret < 0)
    {
        fprintf(stderr,
                "Error submitting the packet to the decoder, err:%s, pkt_size:%d\n",
                av_get_err(ret),
                pkt->size);
        return;
    }
    while (!ret)
    {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error during decoding, err:%s\n", av_get_err(ret));
            return;
        }
        static int flag = 0;
        if (flag == 0)
        {
            flag = 1;
            print_video_format(frame);
        }
        // yuv420p
        for (int i = 0; i < frame->height; ++i)
        {
            fwrite(frame->data[0] + i * frame->linesize[0], 1, frame->width, outfile);
        }
        for (int i = 0; i < frame->height / 2; ++i)
        {
            fwrite(frame->data[1] + i * frame->linesize[1], 1, frame->width / 2, outfile);
        }
        for (int i = 0; i < frame->height / 2; ++i)
        {
            fwrite(frame->data[2] + i * frame->linesize[2], 1, frame->width / 2, outfile);
        }
    }
}
// 注册测试的时候不同分辨率的问题
// 提取H264: ffmpeg -i source.200kbps.768x320_10s.flv -vcodec libx264 -an -f h264
// source.200kbps.768x320_10s.h264 提取MPEG2: ffmpeg -i source.200kbps.768x320_10s.flv -vcodec
// mpeg2video -an -f mpeg2video source.200kbps.768x320_10s.mpeg2 播放：ffplay -pixel_format yuv420p
// -video_size 768x320 -framerate 25  source.200kbps.768x320_10s.yuv
int main(int argc, char** argv)
{
    char const* outfilename;
    char const* filename;

    if (argc <= 2)
    {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(0);
    }
    filename = argv[1];
    outfilename = argv[2];
    FILE* infile = NULL;
    fopen_s(&infile, filename, "rb");
    if (!infile)
    {
        printf("%s open fail.\n", filename);
        return -1;
    }
    FILE* outfile = NULL;
    fopen_s(&outfile, outfilename, "wb");
    if (!infile)
    {
        printf("%s open fail.\n", outfilename);
        return -1;
    }

    enum AVCodecID video_codec_id = AV_CODEC_ID_H264;
    if (strstr(filename, "mpeg"))
    {
        video_codec_id = AV_CODEC_ID_MJPEG;
    }
    else if (strstr(filename, "h264"))
    {
        video_codec_id = AV_CODEC_ID_H264;
    }
    else
    {
        printf("The file type is not supported.\n");
        return -1;
    }

    AVCodecParserContext* parser = av_parser_init(video_codec_id);
    if (!parser)
    {
        printf("av_parser_init error.\n");
        return -1;
    }
    AVCodec const* decoder = avcodec_find_decoder(video_codec_id);
    if (!decoder)
    {
        printf("avcodec_find_decoder error.\n");
        return -1;
    }
    AVCodecContext* dec_cxt = avcodec_alloc_context3(decoder);
    int ret = avcodec_open2(dec_cxt, decoder, NULL);
    if (ret < 0)
    {
        printf("avcodec_open2 error.\n");
        return -1;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
    {
        printf("av_packet_alloc error.\n");
        return -1;
    }
    AVFrame* frame = av_frame_alloc();
    if (!frame)
    {
        printf("av_frame_alloc error in %d line.\n", __LINE__);
        return -1;
    }

    uint8_t inbuf[VIDEO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE] = {};
    int data_size = fread(inbuf, 1, VIDEO_INBUF_SIZE, infile);
    while (data_size > 0)
    {
        if (!frame)
        {
            if (!(frame = av_frame_alloc()))
            {
                printf("av_frame_alloc error in %d line.\n", __LINE__);
                return -1;
            }
        }

        int parse_size = av_parser_parse2(parser,
                                          dec_cxt,
                                          &pkt->data,
                                          &pkt->size,
                                          inbuf,
                                          data_size,
                                          AV_NOPTS_VALUE,
                                          AV_NOPTS_VALUE,
                                          0);
        decode(dec_cxt, pkt, frame, outfile);

        data_size -= parse_size;
        if (parse_size > 0)
        {
            memmove(inbuf, inbuf + parse_size, data_size);
        }
        else if (parse_size == EAGAIN || parse_size == 0)
        {
            int read_size = fread(inbuf + data_size, 1, VIDEO_INBUF_SIZE - data_size, infile);
            data_size += read_size;
        }
    }

    // 冲刷解码器
    pkt->data = NULL;
    pkt->size = 0;
    decode(dec_cxt, pkt, frame, outfile);

    fclose(infile);
    fclose(outfile);
    av_parser_close(parser);
    avcodec_free_context(&dec_cxt);
    av_frame_free(&frame);
    av_packet_free(&pkt);

    printf("main finish, please enter Enter and exit\n");
    return 0;
}
