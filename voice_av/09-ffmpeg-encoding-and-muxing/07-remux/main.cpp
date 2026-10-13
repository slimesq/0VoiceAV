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

typedef struct InputStream {
    AVFormatContext *fmt_ctx;
    AVStream *audio_stream;
    AVStream *video_stream;
    AVCodecContext *audio_dec_ctx;
    AVCodecContext *video_dec_ctx;
    AVPacket *packt;
    AVFrame *frame;
}InputStream;

typedef struct OutputStream {
    AVFormatContext *fmt_ctx;
    AVStream *audio_stream;
    AVStream *video_stream;
    AVCodecContext *audio_enc_ctx;
    AVCodecContext *video_enc_ctx;
    AVPacket *packt;
    AVFrame *frame;
    AVFrame *sws_frame;
    struct SwsContext *sws_ctx;
    int64_t audio_start_pts;
    int64_t video_start_pts;
}OutputStream;

AVFrame *allocate_sws_frame(AVCodecContext *video_enc_ctx)
{
    int ret = 0;
    AVFrame *sws_frame = av_frame_alloc();
    if(sws_frame)
    {
        sws_frame->format = video_enc_ctx->pix_fmt;
        sws_frame->width = video_enc_ctx->width;
        sws_frame->height = video_enc_ctx->height;
        sws_frame->pict_type = AV_PICTURE_TYPE_NONE;
        //        ret = av_image_alloc(sws_frame->data, sws_frame->linesize, video_enc_ctx->width, video_enc_ctx->height, video_enc_ctx->pix_fmt, 32);

        ret = av_frame_get_buffer(sws_frame, 32);   // 分配buffer
        if(ret <0)
        {
            av_frame_free(&sws_frame);
            return NULL;
        }
    }
    return sws_frame;
}


static int mux_send_pkt(OutputStream *output_stream, AVCodecContext *enc_ctx,
                        AVStream *in_stream, AVStream *out_stream, AVPacket *pkt)
{
    int ret = 0;

    printf("encode type:%d pts:%lld dts:%lld\n", enc_ctx->codec_type,  pkt->pts, pkt->dts);

    /* rescale output packet timestamp values from codec to stream timebase */
    if (pkt->duration <= 0 && enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        pkt->duration = av_rescale_q(1, av_inv_q(enc_ctx->framerate), enc_ctx->time_base);
    av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);


    pkt->stream_index = out_stream->index;
    if(pkt)
        printf("enc_ctx type  :%d, pts:%lf, pts:%lld dts:%lld\n",  enc_ctx->codec_type, pkt->pts * av_q2d(out_stream->time_base), pkt->pts, pkt->dts);


    //    printf("rescale_ts :%d pts:%lld dts:%lld\n", enc_ctx->codec_type,  pkt->pts, pkt->dts);
    ret = av_interleaved_write_frame(output_stream->fmt_ctx, pkt);
    //    ret = av_write_frame(output_stream->fmt_ctx, pkt);
    if (ret < 0)
    {
        printf("write frame error, ret:%d\n", ret);
    }
    return ret;
}

static int decode_and_encode_frame(InputStream *input_stream, OutputStream *output_stream, AVMediaType type,  AVPacket *pkt)
{
    int ret = 0;
    AVCodecContext *dec_ctx = NULL;
    AVCodecContext *enc_ctx = NULL;
    AVStream *in_st = NULL;
    AVStream *out_st = NULL;
    int64_t start_pts = AV_NOPTS_VALUE;
    AVFrame *dec_frame = input_stream->frame;
    AVFrame *enc_frame = output_stream->frame;
    AVPacket *enc_pkt = output_stream->packt;
    struct SwsContext *sws_ctx = NULL;
    if(type == AVMEDIA_TYPE_AUDIO)
    {
        dec_ctx = input_stream->audio_dec_ctx;
        enc_ctx = output_stream->audio_enc_ctx;
        in_st = input_stream->audio_stream;
        out_st = output_stream->audio_stream;
    }
    else if(type == AVMEDIA_TYPE_VIDEO)
    {
        dec_ctx = input_stream->video_dec_ctx;
        enc_ctx = output_stream->video_enc_ctx;
        in_st = input_stream->video_stream;
        out_st = output_stream->video_stream;
        sws_ctx = output_stream->sws_ctx;
    }
    else
    {
        printf("no support media type:%d\n", type);
        return -1;
    }


    ret = avcodec_send_packet(dec_ctx, pkt);
    if(pkt)
        printf("read type  :%d, pts:%lf, pts:%lld dts:%lld\n",  dec_ctx->codec_type, pkt->pts * av_q2d(in_st->time_base), pkt->pts, pkt->dts);
    if(ret < 0)
    {
        printf("avcodec_send_packet failed:%d\n", ret);
        return ret;
    }
    char err_str[256] = { 0 };

    while (ret >= 0)  // 读取解码，直到没有解码帧可以获取
    {
        ret = avcodec_receive_frame(dec_ctx, dec_frame);
        if (ret == AVERROR(EAGAIN))
        {
            ret = 0;
            break;
        }
        else if(ret == AVERROR_EOF)
        {
            printf("avcodec_receive_frame EOF\n");
        }
        else if (ret < 0)
        {
            av_strerror(ret, err_str, sizeof(err_str));
            printf("Error:%s while receiving a frame from the decoder\n", err_str);
            break;
        }
        else
        {
            //            av_strerror(ret, err_str, sizeof(err_str));
            //            printf("ret:%s, decode type:%d pts:%lld\n", err_str, dec_ctx->codec_type, dec_frame->pts);
        }
        dec_frame->pts = av_rescale_q(dec_frame->best_effort_timestamp, in_st->time_base, enc_ctx->time_base);
        //        if(dec_frame->buf[0])        // 打印referenc-counted，必须保证传入的是有效指针
        //        {    printf("%s(%d) ref_count(pkt) = %d, has_b_frames:%d\n", __FUNCTION__, __LINE__,
        //                    av_buffer_get_ref_count(dec_frame->buf[0]), dec_ctx->has_b_frames);
        //        }
        if(ret != AVERROR_EOF)
        {

            if(sws_ctx && enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            {
#if 1
                AVFrame *sws_frame = output_stream->sws_frame;
                /* make sure the frame data is writable */
                ret = av_frame_make_writable(sws_frame);
                ret = sws_scale(sws_ctx, (const uint8_t **) dec_frame->data, dec_frame->linesize, 0, dec_frame->height,
                                sws_frame->data, sws_frame->linesize);
                sws_frame->pts = dec_frame->pts;
                sws_frame->pkt_dts = dec_frame->pkt_dts;
                printf("sws_frame  pts:%lf, pts:%lld dts:%lld\n",  sws_frame->pts * av_q2d(in_st->time_base), sws_frame->pts, sws_frame->pkt_dts);

                ret = avcodec_send_frame(enc_ctx, sws_frame);
                if(sws_frame->buf)        // 打印referenc-counted，必须保证传入的是有效指针
                {    printf("%s(%d) ref_count(pkt) = %d\n", __FUNCTION__, __LINE__,
                            av_buffer_get_ref_count(sws_frame->buf[0]));
                }
                //                if(dec_frame->buf[0])        // 打印referenc-counted，必须保证传入的是有效指针
                //                {    printf("%s(%d) ref_count(pkt) = %d\n", __FUNCTION__, __LINE__,
                //                            av_buffer_get_ref_count(dec_frame->buf[0]));
                //                }
                //                av_frame_free(&sws_frame);
#endif
                av_frame_unref(dec_frame);
                if (ret < 0)
                {
                    printf("Error sending a video frame for encoding\n");
                    break;
                }
            }
            else
            {
                av_frame_move_ref(enc_frame, dec_frame);    // 正确的
                enc_frame->pict_type = AV_PICTURE_TYPE_NONE;
                ret = avcodec_send_frame(enc_ctx, enc_frame);
            }
        }
        else  // 没有解码帧可读了，意味着编码的packet也需要全部读取出来
        {
            printf("flush encoder\n");
            ret = avcodec_send_frame(enc_ctx, NULL);
        }
        av_frame_unref(enc_frame);      // 释放引用计数
        while (1)
        {
            // 循环读取packet写封装
            ret = avcodec_receive_packet(enc_ctx, enc_pkt);
            if (ret == AVERROR(EAGAIN))         // 没有可以读取的packet
            {
                //                printf("avcodec_receive_packet EAGAIN\n");
                ret = 0;
                break;
            }
            else if(ret == AVERROR_EOF)     // 全部读取完毕了
            {
                printf("avcodec_receive_packet AVERROR_EOF\n");
                return 0;
            }
            else if (ret < 0)
            {
                printf("[avcodec_receive_packet]Error during encoding, ret:%d\n", ret);
                return ret;
            }
            ret = mux_send_pkt(output_stream, enc_ctx, in_st, out_st, enc_pkt);
            if(ret < 0)
            {
                printf("mux_send_pkt failed\n");
                return ret;
            }
            // 写入数据
            av_packet_unref(enc_pkt);
        }

    }
    return ret;
}


static int open_decodec_context(int *stream_idx,
                                AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

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
        av_dict_set_int(&opts, "refcounted_frames", 1, 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0)
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

    AVDictionary *param = NULL;
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
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        enc_ctx->height = dec_ctx->height;
        enc_ctx->width = dec_ctx->width;
        enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
        enc_ctx->framerate = dec_ctx->framerate;

        // 先判断编码器支持的像素格式是否和要解码的像素格式一致
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P ;  // 可以用来指定像素格式进行测试 AV_PIX_FMT_YUVJ420P  AV_PIX_FMT_YUV420P
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
                return -1;
            }
        }

        /* video time_base can be set to whatever is handy and supported by encoder */
        enc_ctx->time_base =av_inv_q(dec_ctx->framerate);// {1, 1000};

        enc_ctx->gop_size = dec_ctx->gop_size;      // 可以自己设置gop
        enc_ctx->max_b_frames = 0;                  // 可以自己设置是否含有b帧
        //        av_dict_set(&param, "preset", "ultrafast", 0);
        //        av_dict_set(&param, "tune", "zerolatency", 0); // 控制编码输出延迟
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

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
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

    if ((ret = avcodec_open2(enc_ctx, encoder, &param)) < 0)
    {
        fprintf(stderr, "Failed to open %s codec\n",
                av_get_media_type_string(type));
        return ret;
    }

    *oenc_ctx = enc_ctx;
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    int stream_index = -1;
    if (argc < 3)
    {
        printf("Usage: %s input output \n", argv[0]);
        return 1;
    }
    char *input_file = argv[1];
    char *output_file = argv[2];

    InputStream input_stream;
    memset(&input_stream, 0, sizeof(InputStream));

    OutputStream output_stream;
    memset(&output_stream, 0, sizeof(OutputStream));

    // 输入流
    AVFormatContext *ifmt_ctx = NULL;
    AVCodecContext *video_dec_ctx = NULL;
    AVCodecContext *audio_dec_ctx = NULL;
    int video_stream_idx = -1;
    int audio_stream_idx = -1;
    AVPacket *pkt = NULL;

    // 输出流
    AVFormatContext *ofmt_ctx = NULL;
    AVCodecContext *audio_enc_ctx = NULL;
    AVCodecContext *video_enc_ctx = NULL;
    struct SwsContext *sws_ctx = NULL;  // 当解码器和编码器像素格式不一致时需要做scale
    AVStream *out_video_stream = NULL;
    AVStream *out_auido_stream = NULL;
    AVFrame *dec_frame = NULL;
    AVFrame *enc_frame = NULL;
    AVPacket *enc_pkt = NULL;
    AVFrame *dst_frame = NULL;


    if ((ret = avformat_open_input(&ifmt_ctx, input_file, 0, 0)) < 0)
    {
        printf("Could not open input file '%s' ret:%d\n", input_file, ret);
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
    {
        printf("Failed to retrieve input stream information");
        goto end;
    }
    input_stream.fmt_ctx = ifmt_ctx;

    if ((ret = open_decodec_context(&video_stream_idx, &video_dec_ctx, ifmt_ctx, AVMEDIA_TYPE_VIDEO)) < 0)
    {
        printf("fail to open vedio decode context, ret:%d\n", ret);
        goto end;
    }
    input_stream.video_dec_ctx = video_dec_ctx;
    input_stream.video_stream = ifmt_ctx->streams[video_stream_idx];

    if ((ret = open_decodec_context(&audio_stream_idx, &audio_dec_ctx, ifmt_ctx, AVMEDIA_TYPE_AUDIO)) < 0)
    {
        printf("fail to open audio decode context, ret:%d\n", ret);
        goto end;
    }
    input_stream.audio_dec_ctx = audio_dec_ctx;
    input_stream.audio_stream = ifmt_ctx->streams[audio_stream_idx];

    av_dump_format(ifmt_ctx, 0, input_file, 0);

    // 设置输出
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output_file);
    if (!ofmt_ctx)
    {
        printf("can not open ouout context");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    output_stream.fmt_ctx = ofmt_ctx;

    if ((ret = open_encodec_context(video_stream_idx, &video_enc_ctx, video_dec_ctx,ofmt_ctx, AVMEDIA_TYPE_VIDEO, &sws_ctx)) < 0)
    {
        char errStr[256] = { 0 };
        av_strerror(ret, errStr, sizeof(errStr));
        printf("video enc ctx init err:%sn", errStr);
        goto end;
    }
    output_stream.video_enc_ctx = video_enc_ctx;
    output_stream.sws_ctx = sws_ctx;

    // video stream
    out_video_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_video_stream)
    {
        printf("Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    output_stream.video_stream = out_video_stream;

    ret = avcodec_parameters_from_context(out_video_stream->codecpar, video_enc_ctx);
    if (ret < 0)
    {
        printf("Failed to copy codec parameters\n");
        goto end;
    }

    // audio stream
    out_auido_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_auido_stream)
    {
        printf("Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    output_stream.audio_stream = out_auido_stream;

    if ((ret = open_encodec_context(audio_stream_idx, &audio_enc_ctx, audio_dec_ctx, ofmt_ctx, AVMEDIA_TYPE_AUDIO, NULL)) < 0)
    {
        printf("audio enc ctx init err\n");
        goto end;
    }
    output_stream.audio_enc_ctx = audio_enc_ctx;
    ret = avcodec_parameters_from_context(out_auido_stream->codecpar, audio_enc_ctx);
    if (ret < 0)
    {
        printf("Failed to copy codec parameters\n");
        goto end;
    }

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


    dst_frame = av_frame_alloc();
    if(!dst_frame)
    {
        printf("av_frame_alloc dst_frame failed\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }


    pkt = av_packet_alloc();
    if(!pkt)
    {
        printf("av_packet_alloc pkt failed\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    dec_frame = av_frame_alloc();
    if(!dec_frame)
    {
        printf("av_frame_alloc dec_frame failed\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    enc_pkt = av_packet_alloc();
    if(!enc_pkt)
    {
        printf("av_packet_alloc enc_pkt failed\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    enc_frame = av_frame_alloc();
    if(!enc_frame)
    {
        printf("av_frame_alloc enc_frame failed\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    input_stream.packt = pkt;
    input_stream.frame = dec_frame;
    output_stream.packt = enc_pkt;
    output_stream.frame = enc_frame;

    if(ifmt_ctx->start_time != AV_NOPTS_VALUE) {
        if(audio_stream_idx >= 0)
            output_stream.audio_start_pts = av_rescale_q(ifmt_ctx->start_time,
                                                         AV_TIME_BASE_Q,
                                                         ifmt_ctx->streams[audio_stream_idx]->time_base);
        if(video_stream_idx >= 0)
            output_stream.video_start_pts = av_rescale_q(ifmt_ctx->start_time,
                                                         AV_TIME_BASE_Q,
                                                         ifmt_ctx->streams[video_stream_idx]->time_base);
    }else{
        output_stream.audio_start_pts = 0;
        output_stream.video_start_pts = 0;
    }

    if(output_stream.sws_ctx)
    {
        output_stream.sws_frame = allocate_sws_frame(video_enc_ctx);
        if (!output_stream.sws_frame) {
            ret = AVERROR(ENOMEM);
            goto end;
        }
    }


    while (1)
    {
        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret < 0)
        {
            break;
        }
        stream_index = pkt->stream_index;

        if (pkt->stream_index == video_stream_idx)
        {
            if (pkt->pts != AV_NOPTS_VALUE)
                pkt->pts -= output_stream.video_start_pts;
            if (pkt->dts != AV_NOPTS_VALUE)
                pkt->dts -= output_stream.video_start_pts;
            ret = decode_and_encode_frame(&input_stream, &output_stream, AVMEDIA_TYPE_VIDEO, pkt);
            if(ret < 0)
            {
                printf("decode_and_encode_frame video failed\n");
                goto end;
            }
        }
#if 1
        if (pkt->stream_index == audio_stream_idx)
        {
            if (pkt->pts != AV_NOPTS_VALUE)
                pkt->pts -= output_stream.audio_start_pts;
            if (pkt->dts != AV_NOPTS_VALUE)
                pkt->dts -= output_stream.audio_start_pts;
            ret = decode_and_encode_frame(&input_stream, &output_stream, AVMEDIA_TYPE_AUDIO, pkt);
            if(ret < 0)
            {
                printf("decode_and_encode_frame audio failed\n");
                goto end;
            }
        }
#endif
        av_packet_unref(pkt);       // 释放内存
    }
    if (ret != AVERROR_EOF)
        goto end;
    // fflush
    // fflush encode
    printf("flush video ........\n");
    ret = decode_and_encode_frame(&input_stream, &output_stream, AVMEDIA_TYPE_VIDEO, NULL);
    if(ret < 0)
    {
        printf("decode_and_encode_frame video failed\n");
        goto end;
    }
    printf("flush audio ........\n");
    ret = decode_and_encode_frame(&input_stream, &output_stream, AVMEDIA_TYPE_AUDIO, NULL);
    if(ret < 0)
    {
        printf("decode_and_encode_frame audio failed\n");
        goto end;
    }

    printf("stream end\n");
    ret = av_write_trailer(ofmt_ctx);

end:
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    if(video_dec_ctx)
        avcodec_free_context(&video_dec_ctx);
    if(audio_dec_ctx)
        avcodec_free_context(&audio_dec_ctx);
    if(sws_ctx)
        sws_freeContext(sws_ctx);

    if(dst_frame)
        av_frame_free(&dst_frame);

    if(pkt)
        av_packet_free(&pkt);
    if(enc_pkt)
        av_packet_free(&enc_pkt);
    if(dec_frame)
        av_frame_free(&dec_frame);
    if(enc_frame)
        av_frame_free(&enc_frame);
    if(output_stream.sws_frame) {
        av_frame_free(&output_stream.sws_frame);
    }
    printf("main finish\n");
    return ret;
}
