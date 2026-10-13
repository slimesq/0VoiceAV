// 本程序主要解决一位学员将ts格式的H265转成MP4封装的H264格式。
// 零声教育 Darren 音视频高级开发课程
#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avio.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include <string.h>
#include <stdio.h>
#ifdef __cplusplus
}
#endif
int64_t audio_start_pts = AV_NOPTS_VALUE;
int64_t video_start_pts = AV_NOPTS_VALUE;
static int encode_and_save_pkt(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx,
                               AVStream *in_stream, AVStream *out_stream)
{
    (void)in_stream;
    AVPacket *pkt = av_packet_alloc();
    if(!pkt) { return AVERROR(ENOMEM); }
    int ret{0};
    while(true)
    {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { ret = 0; break; }
        if(ret < 0) { break; }

        int64_t &startPts = enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            ? video_start_pts : audio_start_pts;
        if(startPts == AV_NOPTS_VALUE) {
            startPts = pkt->dts != AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        }
        if(startPts != AV_NOPTS_VALUE) {
            if(pkt->pts != AV_NOPTS_VALUE) { pkt->pts -= startPts; }
            if(pkt->dts != AV_NOPTS_VALUE) { pkt->dts -= startPts; }
        }
        if(enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO && pkt->duration == 0) {
            pkt->duration = 1;
        }
        av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
        pkt->stream_index = out_stream->index;
        pkt->pos = -1;
        ret = av_interleaved_write_frame(ofmt_ctx, pkt);
        av_packet_unref(pkt);
        if(ret < 0) { break; }
    }
    av_packet_free(&pkt);
    return ret;
}

static int send_frame_and_save_packets(AVCodecContext *enc_ctx, AVFormatContext *ofmt_ctx,
                                      AVStream *out_stream, AVFrame *frame)
{
    int ret = avcodec_send_frame(enc_ctx, frame);
    if(ret == AVERROR(EAGAIN)) {
        // Preserve the same input frame while draining pending encoder packets.
        ret = encode_and_save_pkt(enc_ctx, ofmt_ctx, nullptr, out_stream);
        if(ret < 0) { return ret; }
        ret = avcodec_send_frame(enc_ctx, frame);
    }
    if(ret == AVERROR_EOF && !frame) { return 0; }
    if(ret < 0) { return ret; }
    return encode_and_save_pkt(enc_ctx, ofmt_ctx, nullptr, out_stream);
}

static int decode_and_send_frame(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx,
                                 struct SwsContext *sws_ctx, AVFrame *dst_frame,
                                 AVFormatContext *ofmt_ctx, AVStream *out_stream)
{
    AVFrame *frame = av_frame_alloc();
    if(!frame) { return AVERROR(ENOMEM); }
    int ret{0};
    while(true)
    {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { ret = 0; break; }
        if(ret < 0) { break; }

        // Decoder frame timestamps use the input stream packet time base.
        const int64_t inputPts = frame->best_effort_timestamp != AV_NOPTS_VALUE
            ? frame->best_effort_timestamp : frame->pts;
        const int64_t encoderPts = inputPts == AV_NOPTS_VALUE ? AV_NOPTS_VALUE
            : av_rescale_q(inputPts, dec_ctx->pkt_timebase, enc_ctx->time_base);
        AVFrame *encoderFrame = frame;
        if(sws_ctx && enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            ret = av_frame_make_writable(dst_frame);
            if(ret < 0) { break; }
            ret = sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
                            dst_frame->data, dst_frame->linesize);
            if(ret < 0) { break; }
            encoderFrame = dst_frame;
        }
        encoderFrame->pts = encoderPts;
        encoderFrame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = send_frame_and_save_packets(enc_ctx, ofmt_ctx, out_stream, encoderFrame);
        av_frame_unref(frame);
        if(ret < 0) { break; }
    }
    av_frame_free(&frame);
    return ret;
}

static int send_packet_and_decode(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx,
                                  struct SwsContext *sws_ctx, AVFrame *dst_frame,
                                  AVFormatContext *ofmt_ctx, AVStream *out_stream,
                                  const AVPacket *packet)
{
    int ret = avcodec_send_packet(dec_ctx, packet);
    if(ret == AVERROR(EAGAIN)) {
        ret = decode_and_send_frame(dec_ctx, enc_ctx, sws_ctx, dst_frame, ofmt_ctx, out_stream);
        if(ret < 0) { return ret; }
        ret = avcodec_send_packet(dec_ctx, packet);
    }
    if(ret == AVERROR_EOF && !packet) { return 0; }
    if(ret < 0) { return ret; }
    return decode_and_send_frame(dec_ctx, enc_ctx, sws_ctx, dst_frame, ofmt_ctx, out_stream);
}

static int open_decodec_context(int *stream_idx,
                                AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, &dec, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Could not find %s stream in input file \n",
                av_get_media_type_string(type));
        return ret;
    }
    else
    {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx)
        {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0)
        {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        if ((*dec_ctx)->codec_type == AVMEDIA_TYPE_VIDEO)
            (*dec_ctx)->framerate = av_guess_frame_rate(fmt_ctx, st, NULL);

        /* Init the decoders, with or without reference counting */
        if ((ret = avcodec_open2(*dec_ctx, dec, nullptr)) < 0)
        {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        (*dec_ctx)->pkt_timebase = st->time_base;
        *stream_idx = stream_index;
    }

    return 0;
}



static int open_encodec_context(int stream_index, AVCodecContext **oenc_ctx,
                                AVCodecContext *dec_ctx, AVFormatContext *ofmt_ctx, enum AVMediaType type,
                                struct SwsContext **sws_ctx)
{
    int ret;
    AVStream *st;
    const AVCodec *encoder = NULL;
    AVDictionary *opts = NULL;
    AVCodecContext *enc_ctx;

    if (type == AVMEDIA_TYPE_VIDEO)
    {/* find encoder for the stream */
        encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!encoder) {
            av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
            return AVERROR_INVALIDDATA;
        }
        enc_ctx = avcodec_alloc_context3(encoder);
        if (!enc_ctx)
        {
            printf("Failed to allocate the encoder context\n");
            return AVERROR(ENOMEM);
        }
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        enc_ctx->height = dec_ctx->height;
        enc_ctx->width = dec_ctx->width;
        enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
        enc_ctx->framerate = dec_ctx->framerate;
        if(enc_ctx->framerate.num <= 0 || enc_ctx->framerate.den <= 0) {
            enc_ctx->framerate = AVRational{25, 1};
        }

        // 先判断编码器支持的像素格式是否和要解码的像素格式一致
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;  // 可以用来指定像素格式进行测试AV_PIX_FMT_YUV420P
        if(enc_ctx->pix_fmt != dec_ctx->pix_fmt)
        {
            /* create scaling context */
            *sws_ctx = sws_getContext(dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                                      enc_ctx->width, enc_ctx->height, enc_ctx->pix_fmt,
                                      SWS_BILINEAR, NULL, NULL, NULL);
            if (!(*sws_ctx)) {
                printf("Impossible to create scale context for the conversion fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                       av_get_pix_fmt_name(dec_ctx->pix_fmt), dec_ctx->width, dec_ctx->height,
                       av_get_pix_fmt_name(enc_ctx->pix_fmt), enc_ctx->width, enc_ctx->height);
                avcodec_free_context(&enc_ctx);
                return AVERROR(EINVAL);
            }
        }

        /* video time_base can be set to whatever is handy and supported by encoder */
        enc_ctx->time_base = av_inv_q(enc_ctx->framerate);

        enc_ctx->gop_size = dec_ctx->gop_size;
        enc_ctx->max_b_frames = 0;
    }
    else if (type == AVMEDIA_TYPE_AUDIO)
    {
        /* find encoder for the stream */
        encoder = avcodec_find_encoder(dec_ctx->codec_id);
        if (!encoder) {
            printf( "Necessary encoder not found\n");
            return AVERROR_INVALIDDATA;
        }
        enc_ctx = avcodec_alloc_context3(encoder);
        if(!enc_ctx) {
            printf("avcodec_alloc_context3 AV_CODEC_ID_AAC failed\n");
            return -1;
        }

        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        enc_ctx->bit_rate = dec_ctx->bit_rate;
        enc_ctx->sample_rate = dec_ctx->sample_rate;
        enc_ctx->sample_fmt = dec_ctx->sample_fmt;
        ret = av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
        if (ret < 0) {
            avcodec_free_context(&enc_ctx);
            return ret;
        }
        enc_ctx->time_base = AVRational{1, enc_ctx->sample_rate};
    }
    else
    {
        return AVERROR(EINVAL);
    }

    if ((ret = avcodec_open2(enc_ctx, encoder, &opts)) < 0)
    {
        fprintf(stderr, "Failed to open %s codec\n",
                av_get_media_type_string(type));
        avcodec_free_context(&enc_ctx);
        av_dict_free(&opts);
        return ret;
    }
    av_dict_free(&opts);

    *oenc_ctx = enc_ctx;
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    AVPacket packet{};
    int stream_index = -1;
    if (argc < 3)
    {
        printf("Usage: %s input output \n", argv[0]);
        return 1;
    }
    char *input_file = argv[1];
    char *output_file = argv[2];

    // 输入流
    AVFormatContext *ifmt_ctx = NULL;
    AVCodecContext *video_dec_ctx = NULL;
    AVCodecContext *audio_dec_ctx = NULL;
    char *flv_name = input_file;
    int video_stream_idx = 0;
    int audio_stream_idx = 1;

    // 输出流
    AVFormatContext *ofmt_ctx = NULL;
    AVCodecContext *audio_enc_ctx = NULL;
    AVCodecContext *video_enc_ctx = NULL;
    struct SwsContext *sws_ctx = NULL;  // 当解码器和编码器像素格式不一致时需要做scale
    AVStream *out_video_stream;
    AVStream *out_auido_stream;
    AVFrame *dst_frame = av_frame_alloc();
    if(!dst_frame) { return 1; }
    if ((ret = avformat_open_input(&ifmt_ctx, flv_name, 0, 0)) < 0)
    {
        printf("Could not open input file '%s' ret:%d\n", flv_name, ret);
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
    {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    if ((ret = open_decodec_context(&video_stream_idx, &video_dec_ctx, ifmt_ctx, AVMEDIA_TYPE_VIDEO)) < 0)
    {
        printf("fail to open vedio decode context, ret:%d\n", ret);
        goto end;
    }
    if ((ret = open_decodec_context(&audio_stream_idx, &audio_dec_ctx, ifmt_ctx, AVMEDIA_TYPE_AUDIO)) < 0)
    {
        printf("fail to open audio decode context, ret:%d\n", ret);
        goto end;
    }
    av_dump_format(ifmt_ctx, 0, input_file, 0);

    // 设置输出
    ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output_file);
    if(ret < 0 || !ofmt_ctx)
    {
        printf("can not open output context");
        if(ret >= 0) { ret = AVERROR(ENOMEM); }
        goto end;
    }


    if ((ret = open_encodec_context(video_stream_idx, &video_enc_ctx, video_dec_ctx,ofmt_ctx, AVMEDIA_TYPE_VIDEO, &sws_ctx)) < 0)
    {
        char errStr[256] = { 0 };
        av_strerror(ret, errStr, sizeof(errStr));
        printf("video enc ctx init err:%sn", errStr);
        goto end;
    }
    // video stream
    out_video_stream =avformat_new_stream(ofmt_ctx, NULL);
    if (!out_video_stream)
    {
        printf("Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    ret = avcodec_parameters_from_context(out_video_stream->codecpar, video_enc_ctx);
    if (ret < 0)
    {
        printf("Failed to copy codec parameters\n");
        goto end;
    }

    out_video_stream->codecpar->codec_tag = 0;
    out_video_stream->time_base = video_enc_ctx->time_base;
    out_video_stream->avg_frame_rate = video_enc_ctx->framerate;

    // audio stream
    out_auido_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_auido_stream)
    {
        printf("Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    if ((ret = open_encodec_context(audio_stream_idx, &audio_enc_ctx, audio_dec_ctx, ofmt_ctx, AVMEDIA_TYPE_AUDIO, NULL)) < 0)
    {
        printf("audio enc ctx init err\n");
        goto end;
    }
    ret = avcodec_parameters_from_context(out_auido_stream->codecpar, audio_enc_ctx);
    if (ret < 0)
    {
        printf("Failed to copy codec parameters\n");
        goto end;
    }
    //    audio_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    out_auido_stream->time_base = audio_enc_ctx->time_base;
    out_auido_stream->codecpar->codec_tag = 0;

    av_dump_format(ofmt_ctx, 0, output_file, 1);

    // 打开文件
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, output_file, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            printf("Could not open output file '%s'\n", output_file);
            goto end;
        }
    }
    out_auido_stream->start_time = 0;
    out_video_stream->start_time = 0;
    ofmt_ctx->start_time = 0;

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0)
    {
        printf("Error occurred when opening output file\n");
        goto end;
    }


    if(sws_ctx)
    {
        dst_frame->format = video_enc_ctx->pix_fmt;
        dst_frame->width = video_enc_ctx->width;
        dst_frame->height = video_enc_ctx->height;
        ret = av_frame_get_buffer(dst_frame, 32);
        if(ret < 0) { goto end; }
        dst_frame->pict_type = AV_PICTURE_TYPE_NONE;
    }



    while(true)
    {
        ret = av_read_frame(ifmt_ctx, &packet);
        if(ret < 0) {
            if(ret == AVERROR_EOF) { ret = 0; }
            break;
        }
        if(packet.stream_index == video_stream_idx)
        {
            ret = send_packet_and_decode(video_dec_ctx, video_enc_ctx, sws_ctx, dst_frame,
                                          ofmt_ctx, out_video_stream, &packet);
        }
#if 0
        // This course debugging copy intentionally leaves the audio processing disabled.
        if(packet.stream_index == audio_stream_idx)
        {
            ret = send_packet_and_decode(audio_dec_ctx, audio_enc_ctx, nullptr, nullptr,
                                          ofmt_ctx, out_auido_stream, &packet);
        }
#endif
        av_packet_unref(&packet);
        if(ret < 0) { goto end; }
    }
    if(ret < 0) { goto end; }

    // Finish decoding before draining the video encoder.
    ret = send_packet_and_decode(video_dec_ctx, video_enc_ctx, sws_ctx, dst_frame,
                                  ofmt_ctx, out_video_stream, nullptr);
    if(ret < 0) { goto end; }
    ret = send_frame_and_save_packets(video_enc_ctx, ofmt_ctx, out_video_stream, nullptr);
    if(ret < 0) { goto end; }
#if 0
    ret = send_packet_and_decode(audio_dec_ctx, audio_enc_ctx, nullptr, nullptr,
                                  ofmt_ctx, out_auido_stream, nullptr);
    if(ret < 0) { goto end; }
    ret = send_frame_and_save_packets(audio_enc_ctx, ofmt_ctx, out_auido_stream, nullptr);
    if(ret < 0) { goto end; }
#endif
    printf("stream end\n");
    ret = av_write_trailer(ofmt_ctx);



end:
    av_packet_unref(&packet);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avcodec_free_context(&video_enc_ctx);
    avcodec_free_context(&audio_enc_ctx);
    if(sws_ctx)
    {
        sws_freeContext(sws_ctx);
    }
    av_frame_free(&dst_frame);
    if(ret < 0) {
        char errorText[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errorText, sizeof(errorText));
        fprintf(stderr, "Transcode failed: %s\n", errorText);
    }
    printf("main finish\n");
    return ret < 0 ? 1 : 0;
}
