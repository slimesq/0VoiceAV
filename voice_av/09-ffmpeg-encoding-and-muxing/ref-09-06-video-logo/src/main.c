/*
 * 参考程序，实现视频和logo的叠加效果
*/

#include <errno.h>
#include <stdio.h>
#include <libavutil/mathematics.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"

#ifdef __cplusplus
}
#endif


const char *filter_descr = "movie=logo.jpg[overlog];[in][overlog]overlay=10:main_h-overlay_h-10";

/* other way:
scale=78:24 [scl]; [scl] transpose=cclock // assumes "[in]" and "[out]" to be input output pads respectively
*/

static AVFormatContext *fmt_ctx;
static AVCodecContext *dec_ctx;
AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;
static int video_stream_index = -1;

AVCodecContext* enc_ctx;
FILE* out_file = NULL;


static int open_output_file(const char *filename)
{
    int ret = 0;
    AVDictionary *options = NULL;
    const AVCodec *enc_codec = avcodec_find_encoder_by_name("libx264");
    if (!enc_codec)
        return AVERROR_ENCODER_NOT_FOUND;
    enc_ctx = avcodec_alloc_context3(enc_codec);
    if (!enc_ctx)
        return AVERROR(ENOMEM);

    enc_ctx->time_base = av_buffersink_get_time_base(buffersink_ctx);
    enc_ctx->framerate = av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[video_stream_index], NULL);
    enc_ctx->pix_fmt = av_buffersink_get_format(buffersink_ctx);
    enc_ctx->width = av_buffersink_get_w(buffersink_ctx);
    enc_ctx->height = av_buffersink_get_h(buffersink_ctx);
    enc_ctx->sample_aspect_ratio = av_buffersink_get_sample_aspect_ratio(buffersink_ctx);
    enc_ctx->max_b_frames = dec_ctx->has_b_frames;
    enc_ctx->bit_rate = 1500 * 1024;
    enc_ctx->gop_size = 250;


    av_dict_set(&options, "profile", "main",0);
    av_dict_set(&options, "tune", "zerolatency",0);
    av_dict_set(&options, "preset", "medium", 0);
    ret = avcodec_open2(enc_ctx, enc_codec, &options);
    if (ret){
        av_log(NULL, AV_LOG_ERROR, "open2 error\n");
        av_dict_free(&options);
        return ret;
    }
    av_dict_free(&options);
    out_file = fopen(filename, "wb");
    if (!out_file)
        return -1;
    return ret;
}
static int open_input_file(const char *filename)
{
    int ret;
    const AVCodec *dec = NULL;

    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        return ret;
    }
    video_stream_index = ret;
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
        return AVERROR(ENOMEM);
    ret = avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);
    if (ret < 0)
        return ret;
    dec_ctx->pkt_timebase = fmt_ctx->streams[video_stream_index]->time_base;

    /* init the video decoder */
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        return ret;
    }

    return 0;
}

static int init_filters(const char *filters_descr)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
        time_base.num, time_base.den,
        dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
        args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
        NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
        AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    /*
    * Set the endpoints for the filter graph. The filter_graph will
    * be linked to the graph described by filters_descr.
    */

    /*
    * The buffer source output must be connected to the input pad of
    * the first filter described by filters_descr; since the first
    * filter input label is not specified, it is set to "in" by
    * default.
    */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    /*
    * The buffer sink input must be connected to the output pad of
    * the last filter described by filters_descr; since the last
    * filter output label is not specified, it is set to "out" by
    * default.
    */
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
        &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int encode_and_write(AVFrame *frame, AVPacket *packet)
{
    int ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0)
        return ret;
    for (;;) {
        ret = avcodec_receive_packet(enc_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;
        if (fwrite(packet->data, 1, packet->size, out_file) != (size_t)packet->size)
            ret = AVERROR(EIO);
        av_packet_unref(packet);
        if (ret < 0)
            return ret;
    }
}

static int drain_filters(AVFrame *frame, AVPacket *packet)
{
    for (;;) {
        int ret = av_buffersink_get_frame(buffersink_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;
        if (frame->pts != AV_NOPTS_VALUE)
            frame->pts = av_rescale_q(frame->pts, av_buffersink_get_time_base(buffersink_ctx), enc_ctx->time_base);
        frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_and_write(frame, packet);
        av_frame_unref(frame);
        if (ret < 0)
            return ret;
    }
}

static int decode_and_filter(AVPacket *input, AVFrame *frame, AVFrame *filtered, AVPacket *output)
{
    int ret = avcodec_send_packet(dec_ctx, input);
    if (ret < 0)
        return ret;
    for (;;) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;
        frame->pts = frame->best_effort_timestamp;
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        av_frame_unref(frame);
        if (ret < 0)
            return ret;
        ret = drain_filters(filtered, output);
        if (ret < 0)
            return ret;
    }
}

int main(int argc, char **argv)
{
    int ret = 0;
    AVPacket *packet = av_packet_alloc();
    AVPacket *encoded = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *filtered = av_frame_alloc();
    const char *input_file = argc > 1 ? argv[1] : "time1.flv";
    const char *output_file = argc > 2 ? argv[2] : "test.h264";
    const char *filters = argc > 3 ? argv[3] : filter_descr;

    if (!packet || !encoded || !frame || !filtered) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    if ((ret = open_input_file(input_file)) < 0 ||
        (ret = init_filters(filters)) < 0 ||
        (ret = open_output_file(output_file)) < 0)
        goto end;

    while ((ret = av_read_frame(fmt_ctx, packet)) >= 0) {
        if (packet->stream_index == video_stream_index)
            ret = decode_and_filter(packet, frame, filtered, encoded);
        av_packet_unref(packet);
        if (ret < 0)
            goto end;
    }
    if (ret != AVERROR_EOF)
        goto end;

    // Drain the decoder, then the filter graph, then the encoder to retain delayed frames.
    ret = decode_and_filter(NULL, frame, filtered, encoded);
    if (ret < 0)
        goto end;
    ret = av_buffersrc_add_frame_flags(buffersrc_ctx, NULL, 0);
    if (ret < 0)
        goto end;
    ret = drain_filters(filtered, encoded);
    if (ret < 0)
        goto end;
    ret = encode_and_write(NULL, encoded);

end:
    av_packet_free(&packet);
    av_packet_free(&encoded);
    av_frame_free(&frame);
    av_frame_free(&filtered);
    avfilter_graph_free(&filter_graph);
    avcodec_free_context(&dec_ctx);
    avcodec_free_context(&enc_ctx);
    avformat_close_input(&fmt_ctx);
    if (out_file && fclose(out_file) != 0 && ret >= 0)
        ret = AVERROR(EIO);
    out_file = NULL;
    if (ret < 0)
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
    return ret < 0 ? 1 : 0;
}
