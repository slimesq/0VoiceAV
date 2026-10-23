/**
 * @file
 * API example for demuxing, decoding, filtering, encoding and muxing
 * @example based on transcoding.c
 */
#include <stdio.h>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}
static AVFormatContext* ifmt_ctx;
static AVFormatContext* ofmt_ctx;
typedef struct FilteringContext
{
    AVFilterContext* buffersink_ctx;
    AVFilterContext* buffersrc_ctx;
    AVFilterGraph* filter_graph;
} FilteringContext;
static FilteringContext* filter_ctx;

typedef struct StreamContext
{
    AVCodecContext* dec_ctx;
    AVCodecContext* enc_ctx;
} StreamContext;
static StreamContext* stream_ctx;

static inline char* error_string(int errnum)
{
    static char errbuf[128];
    av_strerror(errnum, errbuf, 127);
    return errbuf;
}

static int open_input_file(char const* filename)
{
    int ret;
    unsigned int i;

    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    stream_ctx = (StreamContext*)av_calloc(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream* stream = ifmt_ctx->streams[i];
        AVCodec const* dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext* codec_ctx;
        if (!dec)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }
        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx)
        {
            av_log(
                NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
            return AVERROR(ENOMEM);
        }
        stream_ctx[i].dec_ctx = codec_ctx;
        codec_ctx->pkt_timebase = stream->time_base;
        codec_ctx->time_base = stream->time_base;
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0)
        {
            av_log(NULL,
                   AV_LOG_ERROR,
                   "Failed to copy decoder parameters to input decoder context "
                   "for stream #%u\n",
                   i);
            return ret;
        }
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
            codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
        stream_ctx[i].dec_ctx = codec_ctx;
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(char const* filename)
{
    AVStream* out_stream;
    AVStream* in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec const* encoder;
    int ret;
    unsigned int i;

    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream)
        {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }

        in_stream = ifmt_ctx->streams[i];
        dec_ctx = stream_ctx[i].dec_ctx;

        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            /* in this example, we choose transcoding to same codec */
            //            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
            if (!encoder)
            {
                av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx)
            {
                av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
                return AVERROR(ENOMEM);
            }

            /* In this example, we transcode to same properties (picture size,
             * sample rate etc.). These properties can be changed for output
             * streams easily using filters */

            enc_ctx->height = dec_ctx->height;
            enc_ctx->width = dec_ctx->width;
            enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
            enc_ctx->framerate = dec_ctx->framerate;
            /* take first format from list of supported formats */
            if (encoder->pix_fmts)
                enc_ctx->pix_fmt = encoder->pix_fmts[0];
            else
                enc_ctx->pix_fmt = dec_ctx->pix_fmt;
            /* video time_base can be set to whatever is handy and supported by encoder */
            enc_ctx->time_base = av_inv_q(dec_ctx->framerate);

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0)
            {
                av_log(NULL,
                       AV_LOG_ERROR,
                       "Failed to copy encoder parameters to output stream #%u\n",
                       i);
                return ret;
            }

            out_stream->time_base = enc_ctx->time_base;
            stream_ctx[i].enc_ctx = enc_ctx;
        }
        else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            /* in this example, we choose transcoding to same codec */
            //            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
            if (!encoder)
            {
                av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx)
            {
                av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
                return AVERROR(ENOMEM);
            }

            /* In this example, we transcode to same properties (picture size,
           * sample rate etc.). These properties can be changed for output
           * streams easily using filters */

            enc_ctx->sample_rate = dec_ctx->sample_rate;
            ret = av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
            if (ret < 0)
            {
                avcodec_free_context(&enc_ctx);
                return ret;
            }
            /* take first format from list of supported formats */
            enc_ctx->sample_fmt = encoder->sample_fmts[0];
            enc_ctx->time_base = AVRational{1, enc_ctx->sample_rate};

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0)
            {
                av_log(NULL,
                       AV_LOG_ERROR,
                       "Failed to copy encoder parameters to output stream #%u\n",
                       i);
                return ret;
            }

            out_stream->time_base = enc_ctx->time_base;
            stream_ctx[i].enc_ctx = enc_ctx;
        }
        else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN)
        {
            av_log(NULL,
                   AV_LOG_FATAL,
                   "Elementary stream #%d is of unknown type, cannot proceed\n",
                   i);
            return AVERROR_INVALIDDATA;
        }
        else
        {
            /* if this stream must be remuxed */
            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
                return ret;
            }
            out_stream->time_base = in_stream->time_base;
        }
    }
    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int init_filter(FilteringContext* fctx,
                       AVCodecContext* dec_ctx,
                       AVCodecContext* enc_ctx,
                       char const* filter_spec)
{
    char args[512];
    char channel_layout[128];
    int ret = 0;
    AVFilter const* buffersrc = NULL;
    AVFilter const* buffersink = NULL;
    AVFilterContext* buffersrc_ctx = NULL;
    AVFilterContext* buffersink_ctx = NULL;
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();
    AVFilterGraph* filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink)
        {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        snprintf(args,
                 sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 dec_ctx->width,
                 dec_ctx->height,
                 dec_ctx->pix_fmt,
                 dec_ctx->pkt_timebase.num,
                 dec_ctx->pkt_timebase.den,
                 dec_ctx->sample_aspect_ratio.num,
                 dec_ctx->sample_aspect_ratio.den);

        ret =
            avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(
            &buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx,
                             "pix_fmts",
                             (uint8_t*)&enc_ctx->pix_fmt,
                             sizeof(enc_ctx->pix_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    }
    else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink)
        {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        ret =
            av_channel_layout_describe(&dec_ctx->ch_layout, channel_layout, sizeof(channel_layout));
        if (ret < 0)
            goto end;
        snprintf(args,
                 sizeof(args),
                 "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
                 dec_ctx->pkt_timebase.num,
                 dec_ctx->pkt_timebase.den,
                 dec_ctx->sample_rate,
                 av_get_sample_fmt_name(dec_ctx->sample_fmt),
                 channel_layout);
        ret =
            avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(
            &buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx,
                             "sample_fmts",
                             (uint8_t*)&enc_ctx->sample_fmt,
                             sizeof(enc_ctx->sample_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }

        ret =
            av_channel_layout_describe(&enc_ctx->ch_layout, channel_layout, sizeof(channel_layout));
        if (ret < 0)
            goto end;
        ret = av_opt_set(buffersink_ctx, "ch_layouts", channel_layout, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx,
                             "sample_rates",
                             (uint8_t*)&enc_ctx->sample_rate,
                             sizeof(enc_ctx->sample_rate),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }
    }
    else
    {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if (!outputs->name || !inputs->name)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if (ret < 0)
        avfilter_graph_free(&filter_graph);

    return ret;
}

static int init_filters(void)
{
    char const* filter_spec;
    unsigned int i;
    int ret;
    filter_ctx = (FilteringContext*)av_calloc(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        filter_ctx[i].buffersrc_ctx = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph = NULL;
        if (!(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ||
              ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;

        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            filter_spec = "null"; /* passthrough (dummy) filter for video */
        else
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
        ret =
            init_filter(&filter_ctx[i], stream_ctx[i].dec_ctx, stream_ctx[i].enc_ctx, filter_spec);
        if (ret)
            return ret;
    }
    return 0;
}

static int encode_write_frame(AVFrame* filt_frame, unsigned int stream_index, int* got_frame)
{
    AVPacket* enc_pkt = av_packet_alloc();
    if (!enc_pkt)
    {
        av_frame_free(&filt_frame);
        return AVERROR(ENOMEM);
    }
    if (got_frame)
        *got_frame = 0;
    int ret = avcodec_send_frame(stream_ctx[stream_index].enc_ctx, filt_frame);
    av_frame_free(&filt_frame);
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(stream_ctx[stream_index].enc_ctx, enc_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            ret = 0;
            break;
        }
        if (ret < 0)
            break;
        if (got_frame)
            *got_frame = 1;
        enc_pkt->stream_index = stream_index;
        if (enc_pkt->duration <= 0 &&
            stream_ctx[stream_index].enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            enc_pkt->duration = 1;
        av_packet_rescale_ts(enc_pkt,
                             stream_ctx[stream_index].enc_ctx->time_base,
                             ofmt_ctx->streams[stream_index]->time_base);
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        av_packet_unref(enc_pkt);
    }
    av_packet_free(&enc_pkt);
    return ret;
}

static int filter_encode_write_frame(AVFrame* frame, unsigned int stream_index)
{
    int ret;
    AVFrame* filt_frame;

    av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx, frame, 0);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }

    /* pull filtered frames from the filtergraph */
    while (1)
    {
        filt_frame = av_frame_alloc();
        if (!filt_frame)
        {
            ret = AVERROR(ENOMEM);
            break;
        }
        av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx, filt_frame);
        if (ret < 0)
        {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            av_frame_free(&filt_frame);
            break;
        }

        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
        if (filt_frame->pts != AV_NOPTS_VALUE)
            filt_frame->pts =
                av_rescale_q(filt_frame->pts,
                             av_buffersink_get_time_base(filter_ctx[stream_index].buffersink_ctx),
                             stream_ctx[stream_index].enc_ctx->time_base);
        ret = encode_write_frame(filt_frame, stream_index, NULL);
        if (ret < 0)
            break;
    }

    return ret;
}

static int flush_encoder(unsigned int stream_index)
{
    return encode_write_frame(NULL, stream_index, NULL);
}

static int decode_filter_packet(AVPacket const* packet, unsigned int stream_index)
{
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return AVERROR(ENOMEM);
    int ret = avcodec_send_packet(stream_ctx[stream_index].dec_ctx, packet);
    while (ret >= 0)
    {
        ret = avcodec_receive_frame(stream_ctx[stream_index].dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            ret = 0;
            break;
        }
        if (ret < 0)
            break;
        frame->pts = frame->best_effort_timestamp;
        ret = filter_encode_write_frame(frame, stream_index);
        av_frame_unref(frame);
    }
    av_frame_free(&frame);
    return ret;
}

// 传入参数 265_1920x1080-aac_8000_1.ts  264_1920x1080-aac_8000_1.mp4
int main(int argc, char** argv)
{
    int ret{0};
    AVPacket packet{};
    unsigned int stream_index;
    unsigned int i;

    if (argc != 3)
    {
        av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n", argv[0]);
        return 1;
    }

    if ((ret = open_input_file(argv[1])) < 0)
        goto end;
    if ((ret = open_output_file(argv[2])) < 0)
        goto end;
    if ((ret = init_filters()) < 0)
        goto end;

    /* read all packets */
    while (1)
    {
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            break;
        stream_index = packet.stream_index;
        if (ifmt_ctx->start_time != AV_NOPTS_VALUE)
        {
            int64_t const start = av_rescale_q(
                ifmt_ctx->start_time, AV_TIME_BASE_Q, ifmt_ctx->streams[stream_index]->time_base);
            if (packet.pts != AV_NOPTS_VALUE)
                packet.pts -= start;
            if (packet.dts != AV_NOPTS_VALUE)
                packet.dts -= start;
        }
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n", stream_index);

        if (filter_ctx[stream_index].filter_graph)
        {
            ret = decode_filter_packet(&packet, stream_index);
            if (ret < 0)
                goto end;
        }
        else
        {
            /* remux this frame without reencoding */
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 ofmt_ctx->streams[stream_index]->time_base);

            ret = av_interleaved_write_frame(ofmt_ctx, &packet);
            if (ret < 0)
                goto end;
        }
        av_packet_unref(&packet);
    }

    if (ret != AVERROR_EOF)
        goto end;

    /* flush filters and encoders */
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        /* flush filter */
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = decode_filter_packet(NULL, i);
        if (ret < 0)
            goto end;
        ret = filter_encode_write_frame(NULL, i);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }

        /* flush encoder */
        ret = flush_encoder(i);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }

    ret = av_write_trailer(ofmt_ctx);
end:
    av_packet_unref(&packet);
    for (i = 0; ifmt_ctx && stream_ctx && i < ifmt_ctx->nb_streams; i++)
    {
        avcodec_free_context(&stream_ctx[i].dec_ctx);
        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && stream_ctx[i].enc_ctx)
            avcodec_free_context(&stream_ctx[i].enc_ctx);
        if (filter_ctx && filter_ctx[i].filter_graph)
            avfilter_graph_free(&filter_ctx[i].filter_graph);
    }
    av_free(filter_ctx);
    av_free(stream_ctx);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", error_string(ret));
    printf("main finish\n");
    return ret < 0 ? 1 : 0;
}
