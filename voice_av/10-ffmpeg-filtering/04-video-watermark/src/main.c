/* Decode two JPEGs, overlay the logo, then encode one JPEG. */
#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>

static int create_source(AVFilterGraph* graph,
                         AVFilterContext** source,
                         AVFrame const* frame,
                         char const* name)
{
    char args[256];
    AVRational sar = frame->sample_aspect_ratio;
    if (sar.num <= 0 || sar.den <= 0)
        sar = (AVRational){1, 1};
    snprintf(args,
             sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=1/25:pixel_aspect=%d/%d",
             frame->width,
             frame->height,
             frame->format,
             sar.num,
             sar.den);
    return avfilter_graph_create_filter(
        source, avfilter_get_by_name("buffer"), name, args, NULL, graph);
}

static int init_filters(AVFilterGraph* graph,
                        AVFilterContext** main_source,
                        AVFilterContext** logo_source,
                        AVFilterContext** sink,
                        AVFrame const* main_frame,
                        AVFrame const* logo_frame,
                        int x,
                        int y)
{
    int ret = 0;
    char description[128];
    AVFilterInOut* inputs = avfilter_inout_alloc();
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* logo_output = avfilter_inout_alloc();
    // Require a real full-range conversion before the JPEG encoder, not just a format relabel.
    const enum AVPixelFormat formats[] = {AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_NONE};
    if (!inputs || !outputs || !logo_output)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    outputs->next = logo_output;
    logo_output = NULL;  // now owned by the outputs list
    if ((ret = create_source(graph, main_source, main_frame, "main")) < 0 ||
        (ret = create_source(graph, logo_source, logo_frame, "logo")) < 0 ||
        (ret = avfilter_graph_create_filter(
             sink, avfilter_get_by_name("buffersink"), "sink", NULL, NULL, graph)) < 0 ||
        (ret = av_opt_set_int_list(
             *sink, "pix_fmts", formats, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto end;

    // The existing source outputs match [main] and [logo]; the sink input matches [out].
    outputs->name = av_strdup("main");
    outputs->filter_ctx = *main_source;
    outputs->pad_idx = 0;
    outputs->next->name = av_strdup("logo");
    outputs->next->filter_ctx = *logo_source;
    outputs->next->pad_idx = 0;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = *sink;
    inputs->pad_idx = 0;
    if (!outputs->name || !outputs->next->name || !inputs->name)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    snprintf(description, sizeof(description), "[main][logo]overlay=%d:%d:format=auto[out]", x, y);
    ret = avfilter_graph_parse_ptr(graph, description, &inputs, &outputs, NULL);
    if (ret >= 0 && (inputs || outputs))
        ret = AVERROR(EINVAL);
    if (ret >= 0)
        ret = avfilter_graph_config(graph, NULL);
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&logo_output);
    return ret;
}

static int parse_position(char const* text, int* position)
{
    char* end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < INT_MIN || value > INT_MAX)
        return AVERROR(EINVAL);
    *position = (int)value;
    return 0;
}

int main(int argc, char** argv)
{
    char const* main_name = argc > 1 ? argv[1] : "main.jpg";
    char const* logo_name = argc > 2 ? argv[2] : "logo.jpg";
    char const* output_name = argc > 3 ? argv[3] : "test-output3.jpg";
    int x = 100, y = 200, ret = 0;
    if (argc > 6 || argc == 5 ||
        (argc == 6 && (parse_position(argv[4], &x) < 0 || parse_position(argv[5], &y) < 0)) ||
        strcmp(output_name, main_name) == 0 || strcmp(output_name, logo_name) == 0)
    {
        fprintf(stderr, "Usage: %s [main.jpg [logo.jpg [output.jpg [x y]]]]\n", argv[0]);
        return 1;
    }
    AVFrame* main_frame = get_frame_from_jpegfile(main_name);
    AVFrame* logo_frame = get_frame_from_jpegfile(logo_name);
    AVFrame* result = av_frame_alloc();
    AVFilterGraph* graph = avfilter_graph_alloc();
    AVFilterContext *main_source = NULL, *logo_source = NULL, *sink = NULL;
    if (!main_frame || !logo_frame || !result || !graph)
    {
        ret = AVERROR(EINVAL);
        fprintf(stderr, "Cannot decode input JPEGs or allocate the filter graph.\n");
        goto end;
    }
    if ((ret = init_filters(
             graph, &main_source, &logo_source, &sink, main_frame, logo_frame, x, y)) < 0)
        goto end;
    main_frame->pts = logo_frame->pts = 0;
    if ((ret = av_buffersrc_add_frame_flags(main_source, main_frame, AV_BUFFERSRC_FLAG_KEEP_REF)) <
            0 ||
        (ret = av_buffersrc_add_frame_flags(logo_source, logo_frame, AV_BUFFERSRC_FLAG_KEEP_REF)) <
            0 ||
        (ret = av_buffersrc_close(main_source, 1, 0)) < 0 ||
        (ret = av_buffersrc_close(logo_source, 1, 0)) < 0 ||
        (ret = av_buffersink_get_frame(sink, result)) < 0 ||
        (ret = save_frame_to_jpeg(output_name, result)) < 0)
        goto end;
    printf("Saved %dx%d JPEG -> %s\n", result->width, result->height, output_name);
end:
    av_frame_free(&main_frame);
    av_frame_free(&logo_frame);
    av_frame_free(&result);
    avfilter_graph_free(&graph);
    if (ret < 0)
        fprintf(stderr, "Watermark failed: %s\n", av_err2str(ret));
    return ret < 0 ? 1 : 0;
}
