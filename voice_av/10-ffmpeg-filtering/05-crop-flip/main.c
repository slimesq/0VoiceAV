/* Course example: parse the same split/crop/vflip/overlay graph from a filter string. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avstring.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

static int init_filters(AVFilterGraph* graph,
                        AVFilterContext** source,
                        AVFilterContext** sink,
                        int width,
                        int height,
                        int fps)
{
    char description[512];
    AVFilterInOut *inputs = NULL, *outputs = NULL;
    snprintf(description,
             sizeof(description),
             "buffer=video_size=%dx%d:pix_fmt=%d:time_base=1/%d:pixel_aspect=1/1:"
             "frame_rate=%d/1,split[main][tmp];"
             "[tmp]crop=iw:ih/2:0:0,vflip[flip];"
             "[main][flip]overlay=0:H/2,buffersink",
             width,
             height,
             AV_PIX_FMT_YUV420P,
             fps,
             fps);
    int ret = avfilter_graph_parse2(graph, description, &inputs, &outputs);
    // This expression includes its own source/sink; no pads may remain unconnected.
    if (ret >= 0 && (inputs || outputs))
        ret = AVERROR(EINVAL);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if (ret < 0)
        return ret;
    // Find by filter type instead of relying on parser-generated instance numbers.
    for (unsigned i = 0; i < graph->nb_filters; ++i)
    {
        AVFilterContext* filter = graph->filters[i];
        if (strcmp(filter->filter->name, "buffer") == 0)
            *source = filter;
        else if (strcmp(filter->filter->name, "buffersink") == 0)
            *sink = filter;
    }
    if (!*source || !*sink)
        return AVERROR_FILTER_NOT_FOUND;
    const enum AVPixelFormat formats[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    ret = av_opt_set_int_list(*sink, "pix_fmts", formats, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
        return ret;
    return avfilter_graph_config(graph, NULL);
}

static int positive_int(char const* text)
{
    char* end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    return errno || end == text || *end || value <= 0 || value > INT_MAX ? 0 : (int)value;
}

static int dump_graph(AVFilterGraph* graph, char const* output)
{
    char* description = avfilter_graph_dump(graph, NULL);
    char* filename = av_asprintf("%s.graph.txt", output);
    int ret = 0;
    FILE* file = NULL;
    if (!description || !filename)
        ret = AVERROR(ENOMEM);
    else if (!(file = fopen(filename, "wb")))
        ret = AVERROR(errno);
    else if (fwrite(description, 1, strlen(description), file) != strlen(description))
        ret = AVERROR(EIO);
    if (file && fclose(file))
        ret = AVERROR(EIO);
    av_free(description);
    av_free(filename);
    return ret;
}

static int write_available(AVFilterContext* sink,
                           AVFrame* frame,
                           FILE* output,
                           uint8_t* packed,
                           int capacity,
                           int64_t* count)
{
    for (;;)
    {
        int ret = av_buffersink_get_frame(sink, frame);
        if (ret < 0)
            return ret;
        // Filtered frames may be padded or have negative strides (vflip).
        ret = av_image_copy_to_buffer(packed,
                                      capacity,
                                      (uint8_t const* const*)frame->data,
                                      frame->linesize,
                                      frame->format,
                                      frame->width,
                                      frame->height,
                                      1);
        av_frame_unref(frame);
        if (ret < 0)
            return ret;
        if (fwrite(packed, 1, ret, output) != (size_t)ret)
            return AVERROR(EIO);
        ++*count;
    }
}

int main(int argc, char** argv)
{
    char const* input_name = argc > 1 ? argv[1] : "768x320.yuv";
    char const* output_name = argc > 2 ? argv[2] : "out_crop_vfilter_2.yuv";
    int width = argc > 3 ? positive_int(argv[3]) : 768;
    int height = argc > 4 ? positive_int(argv[4]) : 320;
    int fps = argc > 5 ? positive_int(argv[5]) : 25;
    if (argc > 6 || argc == 4 || !width || !height || !fps || width % 2 || height % 4 ||
        strcmp(input_name, output_name) == 0)
    {
        fprintf(stderr,
                "Usage: %s [input.yuv [output.yuv [width height [fps]]]]\n"
                "Input: YUV420P, even width, height divisible by 4.\n",
                argv[0]);
        return 1;
    }

    int ret = 0;
    int64_t input_count = 0, output_count = 0;
    int size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
    FILE *input = NULL, *output = NULL;
    uint8_t* packed = NULL;
    AVFilterGraph* graph = avfilter_graph_alloc();
    AVFilterContext *source = NULL, *sink = NULL;
    AVFrame* frame = av_frame_alloc();
    AVFrame* filtered = av_frame_alloc();
    uint8_t* planes[4] = {0};
    int strides[4] = {0};
    if (size < 0)
    {
        ret = size;
        goto end;
    }
    packed = av_malloc(size);
    if (!graph || !frame || !filtered || !packed)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    input = fopen(input_name, "rb");
    if (!input)
    {
        ret = AVERROR(errno);
        goto end;
    }
    ret = init_filters(graph, &source, &sink, width, height, fps);
    if (ret < 0 || (ret = dump_graph(graph, output_name)) < 0)
        goto end;
    output = fopen(output_name, "wb");
    if (!output)
    {
        ret = AVERROR(errno);
        goto end;
    }
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width;
    frame->height = height;
    frame->sample_aspect_ratio = (AVRational){1, 1};
    if ((ret = av_frame_get_buffer(frame, 32)) < 0 ||
        (ret = av_image_fill_arrays(planes, strides, packed, frame->format, width, height, 1)) < 0)
        goto end;
    for (;;)
    {
        size_t bytes = fread(packed, 1, size, input);
        if (bytes == 0 && !ferror(input))
            break;
        if (bytes != (size_t)size)
        {
            fprintf(stderr, "Incomplete raw frame or input read error.\n");
            ret = AVERROR(EIO);
            goto end;
        }
        if ((ret = av_frame_make_writable(frame)) < 0)
            goto end;
        av_image_copy(frame->data,
                      frame->linesize,
                      (uint8_t const**)planes,
                      strides,
                      frame->format,
                      width,
                      height);
        frame->pts = input_count++;
        ret = av_buffersrc_add_frame_flags(source, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
            goto end;
        ret = write_available(sink, filtered, output, packed, size, &output_count);
        if (ret != AVERROR(EAGAIN))
            goto end;
    }
    // EOF must reach both split branches; then drain all remaining output frames.
    ret = av_buffersrc_close(source, input_count, 0);
    if (ret < 0)
        goto end;
    ret = write_available(sink, filtered, output, packed, size, &output_count);
    if (ret == AVERROR_EOF)
        ret = 0;

end:
    if (input)
        fclose(input);
    if (output && fclose(output))
        ret = AVERROR(EIO);
    av_frame_free(&frame);
    av_frame_free(&filtered);
    avfilter_graph_free(&graph);
    av_free(packed);
    if (ret < 0)
        fprintf(stderr, "Filter failed: %s\n", av_err2str(ret));
    else
        printf("Filtered %" PRId64 " frames -> %s\n", output_count, output_name);
    return ret < 0 ? 1 : 0;
}
