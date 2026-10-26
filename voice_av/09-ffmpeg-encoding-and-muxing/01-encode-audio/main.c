/**
* @projectName   08-01-encode_audio
* @brief         音频编码
*               从本地读取PCM数据进行AAC编码
*           1. 输入PCM格式问题，通过AVCodec的sample_fmts参数获取具体的格式支持
*           （1）默认的aac编码器输入的PCM格式为:AV_SAMPLE_FMT_FLTP
*           （2）libfdk_aac编码器输入的PCM格式为AV_SAMPLE_FMT_S16.
*           2. 支持的采样率，通过AVCodec的supported_samplerates可以获取
* @author        Liao Qingfu
* @date          2020-04-15
*/

#include <errno.h>
#include <inttypes.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <urlmon.h>
#include <winbase.h>

/* 检测该编码器是否支持该采样格式 */
static int check_sample_fmt(AVCodec const* codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat* p = NULL;

    if (avcodec_get_supported_config(
            NULL, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (void const**)&p, NULL) < 0)
    {
        return 0;
    }

    if (!p)
        return 1;

    while (*p != AV_SAMPLE_FMT_NONE)
    {  // 通过AV_SAMPLE_FMT_NONE作为结束符
        if (*p == sample_fmt)
        {
            return 1;
        }
        ++p;
    }

    return 0;
}

/* 检测该编码器是否支持该采样率 */
static int check_sample_rate(AVCodec const* codec, int const sample_rate)
{
    int* p = 0;
    if (avcodec_get_supported_config(
            NULL, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (void const**)&p, NULL) < 0)
    {
        return 0;
    }

    if (!p)
        return 1;

    while (*p != 0)
    {  //  0作为退出条件，比如libfdk-aacenc.c的aac_sample_rates
        if (*p == sample_rate)
        {
            return 1;
        }
        ++p;
    }

    return 0;
}

/* 检测该编码器是否支持该采样率, 该函数只是作参考 */
static int check_channel_layout(AVCodec const* codec, AVChannelLayout const* channel_layout)
{
    AVChannelLayout* p = NULL;
    if (avcodec_get_supported_config(
            NULL, codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, (void const**)&p, NULL) < 0)
    {
        return 0;
    }
    if (!p)
    {
        return 1;
    }

    while (p->nb_channels)
    {
        if (av_channel_layout_compare(p, channel_layout) == 0)
            return 1;
        ++p;
    }

    return 0;
}

static void get_adts_header(AVCodecContext* ctx, uint8_t* adts_header, int aac_length)
{
    uint8_t freq_idx = 0;  //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
    switch (ctx->sample_rate)
    {
        case 96000:
            freq_idx = 0;
            break;
        case 88200:
            freq_idx = 1;
            break;
        case 64000:
            freq_idx = 2;
            break;
        case 48000:
            freq_idx = 3;
            break;
        case 44100:
            freq_idx = 4;
            break;
        case 32000:
            freq_idx = 5;
            break;
        case 24000:
            freq_idx = 6;
            break;
        case 22050:
            freq_idx = 7;
            break;
        case 16000:
            freq_idx = 8;
            break;
        case 12000:
            freq_idx = 9;
            break;
        case 11025:
            freq_idx = 10;
            break;
        case 8000:
            freq_idx = 11;
            break;
        case 7350:
            freq_idx = 12;
            break;
        default:
            freq_idx = 4;
            break;
    }
    uint8_t chanCfg = ctx->ch_layout.nb_channels;
    uint32_t frame_length = aac_length + 7;
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = ((ctx->profile) << 6) + (freq_idx << 2) + (chanCfg >> 2);
    adts_header[3] = (((chanCfg & 3) << 6) + (frame_length >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
}
/*
*
*/
static int encode(AVCodecContext* ctx, AVFrame* frame, AVPacket* pkt, FILE* output)
{
    int ret = avcodec_send_frame(ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        return -1;
    }

    /* read all the available output packets (in general there may be any number of them)*/
    // 编码和解码都是一样的,都是send 1次，然后receive多次,直到AVERROR(EAGAIN)或者AVERROR_EOF
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return 0;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error encoding auido frame\n");
            return -1;
        }

        size_t len = 0;
        printf("ctx->flags:0x%x & AV_CODEC_FLAG_GLOBAL_HEADER: 0x%x, name:%s\n",
               ctx->flags,
               ctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER,
               ctx->codec->name);
        if ((ctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER))
        {
            // 需要额外的adts header 写入
            uint8_t aac_header[7];
            get_adts_header(ctx, aac_header, pkt->size);
            len = fwrite(&aac_header, 1, 7, output);
            if (len != 7)
            {
                fprintf(stderr, "fwrite aac_header failed");
                return -1;
            }
        }
        len = fwrite(pkt->data, 1, pkt->size, output);
        if (len != pkt->size)
        {
            fprintf(stderr, "fwrite aac data failed");
            return -1;
        }
    }
    /*是否需要释放数据? avcodec_receive_packet第一个调用的就是av_packet_unref
    *所以我们不用手动去释放,这里有个小问题,不能将pkt直接插入到队列,因为播放器会释放数据
    *可以新分配一个pkt,然后使用av_packet_move_ref转移pkt对应的buffer
    */
    // av_packet_unref(pkt);

    return -1;
}

/*
 * 这里只支持2通道的转换
*/
void f32le_convert_to_fltp(float* f32le, float* fltp, int nb_samples)
{
    float* fltp_l = fltp;               // 左声道
    float* fltp_r = fltp + nb_samples;  // 右声道
    for (int i = 0; i < nb_samples; ++i)
    {
        fltp_l[i] = f32le[i * 2];      // 0 1   - 2 3
        fltp_r[i] = f32le[i * 2 + 1];  // 可以尝试注释左声道或者右声道听听声音
    }
}

/*
 * 提取测试文件：
 * （1）s16格式：ffmpeg -i buweishui.aac -ar 48000 -ac 2 -f s16le 48000_2_s16le.pcm
 * （2）flt格式：ffmpeg -i buweishui.aac -ar 48000 -ac 2 -f f32le 48000_2_f32le.pcm
 *      ffmpeg只能提取packed格式的PCM数据，在编码时候如果输入要为fltp则需要进行转换
 * 测试范例:
 * （1）48000_2_s16le.pcm libfdk_aac.aac libfdk_aac  // 如果编译的时候没有支持fdk aac则提示找不到编码器
 * （2）48000_2_f32le.pcm aac.aac aac // 我们这里只测试aac编码器，不测试fdkaac
*/
int main(int argc, char** argv)
{
    char* in_pcm_file = NULL;
    char* out_aac_file = NULL;
    FILE* infile = NULL;
    FILE* outfile = NULL;
    AVCodec const* codec = NULL;
    AVCodecContext* codec_ctx = NULL;
    AVFrame* frame = NULL;
    AVPacket* pkt = NULL;
    int ret = 1;
    uint8_t* pcm_buf = NULL;
    uint8_t* pcm_temp_buf = NULL;
    int force_coder = 0;  // 强制使用指定的编码器
    char* codec_name = NULL;

    if (argc < 3)
    {
        fprintf(stderr, "Usage:%s <input_file out_file [codec_name]>,argc:%d\n", argv[0], argc);
        return 1;
    }
    in_pcm_file = argv[1];   // 输入PCM文件
    out_aac_file = argv[2];  // 输出的AAC文件

    enum AVCodecID codec_id = AV_CODEC_ID_AAC;
    if (4 == argc)
    {
        if (strcmp(argv[3], "libfdk_aac") == 0)
        {
            force_coder = 1;  // 强制使用 libfdk_aac
            codec_name = "libfdk_aac";
        }
        else if (strcmp(argv[3], "aac") == 0)
        {
            force_coder = 1;
            codec_name = "aac";
        }
    }

    if (force_coder)
    {
        printf("force codec name: %s\n", codec_name);
    }
    else
    {
        printf("default codec name:%s\n", "aac");
    }

    if (force_coder == 0)
    {                                            // 没有强制使用编码器
        codec = avcodec_find_encoder(codec_id);  // 按ID查找则缺省的aac encode为aacenc.c
    }
    else
    {
        // 按名字查找指定的encode,对应AVCodec的name字段
        codec = avcodec_find_encoder_by_name(codec_name);
    }

    if (!codec)
    {
        fprintf(stderr, "Codec not found\n");
        ret = -1;
        goto cleanup;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return -1;
        goto cleanup;
    }
    codec_ctx->codec_id = codec->id;
    codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_ctx->bit_rate = 128 * 1024;
    av_channel_layout_default(&codec_ctx->ch_layout, 2);
    codec_ctx->sample_rate = 48000;  // 48000
    codec_ctx->time_base = (AVRational){1, codec_ctx->sample_rate};
    codec_ctx->profile = FF_PROFILE_AAC_LOW;

    if (strcmp(codec->name, "aac") == 0)
    {
        codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
    else if (strcmp(codec->name, "libfdk_aac") == 0)
    {
        codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    }
    else
    {
        codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }

    /* 检测支持采样格式支持情况 */
    if (!check_sample_fmt(codec, codec_ctx->sample_fmt))
    {
        fprintf(stderr,
                "Encoder does not support sample format:%s",
                av_get_sample_fmt_name(codec_ctx->sample_fmt));
        ret = -1;
        goto cleanup;
    }
    if (!check_sample_rate(codec, codec_ctx->sample_rate))
    {
        fprintf(stderr, "Encoder does not support sample rate:%d", codec_ctx->sample_rate);
        ret = -1;
        goto cleanup;
    }
    if (!check_channel_layout(codec, &codec_ctx->ch_layout))
    {
        fprintf(stderr, "Encoder does not support stereo channel layout\n");
        ret = -1;
        goto cleanup;
    }

    printf("\n\nAudio encode config\n");
    printf("bit_rate:%" PRId64 "kbps\n", codec_ctx->bit_rate / 1024);
    printf("sample_rate:%d\n", codec_ctx->sample_rate);
    printf("channels:%d\n", codec_ctx->ch_layout.nb_channels);
    // frame_size是在avcodec_open2后进行关联
    printf("1 frame_size:%d\n", codec_ctx->frame_size);

    codec_ctx->flags |=
        AV_CODEC_FLAG_GLOBAL_HEADER;  // ffmpeg默认的aac是不带adts,而fdk_aac默认带adts,这里我们强制不带
    /*将编码器上下文和编码器进行关联*/
    if (avcodec_open2(codec_ctx, codec, NULL))
    {
        fprintf(stderr, "Cound not open codec\n");
        ret = -1;
        goto cleanup;
    }
    printf("2 frame_size:%d\n\n", codec_ctx->frame_size);
    // 打开输入和输出文件
    infile = fopen(in_pcm_file, "rb");
    if (!infile)
    {
        fprintf(stderr, "Could not open %s\n", in_pcm_file);
        ret = -1;
        goto cleanup;
    }
    outfile = fopen(out_aac_file, "wb");
    if (!outfile)
    {
        fprintf(stderr, "Could not open %s\n", out_aac_file);
        ret = -1;
        goto cleanup;
    }

    /*packet for holding encoded output*/
    pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "Could not allocate the packet\n");
        ret = -1;
        goto cleanup;
    }

    /* frame containing input raw audio */
    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Could not allocate audio frame\n");
        ret = -1;
        goto cleanup;
    }

    /*每次送多少数据给编码器有
    *   (1) frame_size(每帧单个通道的采样点数)
    *   (2) sample_fmt(采样点格式)
    *   (3) channel_layout(通道布局情况)
    *   3要素决定
    */
    frame->nb_samples = codec_ctx->frame_size;
    frame->format = codec_ctx->sample_fmt;
    ret = av_channel_layout_copy(&(frame->ch_layout), &(codec_ctx->ch_layout));
    if (ret < 0)
        goto cleanup;
    printf("frame nb_sample:%d\n", frame->nb_samples);
    printf("frame smaple_fmt:%s\n", av_get_sample_fmt_name(frame->format));
    printf("frame channels:%d\n", frame->ch_layout.nb_channels);
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        ret = -1;
        goto cleanup;
    }
    // 计算出每一帧的数据，单个采样点的字节 * 通道数目 * 每帧采样点数量
    int frame_bytes =
        av_get_bytes_per_sample(frame->format) * frame->ch_layout.nb_channels * frame->nb_samples;
    printf("frame_bytes %d\n", frame_bytes);
    pcm_buf = (uint8_t*)malloc(frame_bytes);
    if (!pcm_buf)
    {
        printf("pcm_buf malloc failed\n");
        ret = -1;
        goto cleanup;
    }
    pcm_temp_buf = (uint8_t*)malloc(frame_bytes);
    if (!pcm_temp_buf)
    {
        printf("pcm_temp_buf malloc failed\n");
        ret = -1;
        goto cleanup;
    }
    int64_t pts = 0;
    printf("start encode\n");
    for (;;)
    {
        memset(pcm_buf, 0, frame_bytes);
        size_t read_bytes = fread(pcm_buf, 1, frame_bytes, infile);
        if (read_bytes <= 0)
        {
            printf("read file finish\n");
            break;
            // fseek(infile,0,SEEK_SET);
            // fflush(infile);
            // continue;
        }
        /*确保该frame可写，如果编码器内部保持了内存参考计数,则需要重新拷贝一个备份,
        目的是新写入的数据和编码器保存的数据不能产生冲突*/
        ret = av_frame_make_writable(frame);
        if (ret != 0)
            printf("av_frame_writable failed,ret = %d\n", ret);
        if (ret < 0)
            goto cleanup;

        if (AV_SAMPLE_FMT_S16 == frame->format)
        {
            memcpy(frame->data[0], pcm_buf, frame_bytes);
        }
        else
        {
            memset(pcm_temp_buf, 0, frame_bytes);
            f32le_convert_to_fltp((float*)pcm_buf, (float*)pcm_temp_buf, frame->nb_samples);
            int const plane_bytes = frame->nb_samples * av_get_bytes_per_sample(frame->format);
            memcpy(frame->data[0], pcm_temp_buf, plane_bytes);
            memcpy(frame->data[1], pcm_temp_buf + plane_bytes, plane_bytes);
        }

        // 设置 pts
        frame->pts = pts;
        pts += frame->nb_samples;
        ret = encode(codec_ctx, frame, pkt, outfile);
        if (ret < 0)
        {
            printf("encode fialed\n");
            goto cleanup;
        }
    }

    /*冲刷解码器*/
    if (ferror(infile))
    {
        ret = -1;
        goto cleanup;
    }
    ret = encode(codec_ctx, NULL, pkt, outfile);

// 关闭文件
cleanup:
    if (infile)
        fclose(infile);
    if (outfile && fclose(outfile) != 0)
        ret = -1;

    // 释放内存
    if (pcm_buf)
    {
        free(pcm_buf);
    }
    if (pcm_temp_buf)
    {
        free(pcm_temp_buf);
    }
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);

    return ret < 0 ? -1 : 0;
}
