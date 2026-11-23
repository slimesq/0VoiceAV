/* Adapted from the course example 07-03-extract-h264. */
#include <stdio.h>
#include <string.h>

#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>

static int write_filtered_packets(AVBSFContext* filter, AVPacket* packet, FILE* output)
{
    int ret;
    while ((ret = av_bsf_receive_packet(filter, packet)) >= 0)
    {
        size_t const size = packet->size;
        size_t const written = fwrite(packet->data, 1, size, output);
        av_packet_unref(packet);
        if (written != size)
            return AVERROR(EIO);
    }
    return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ? 0 : ret;
}

int main(int argc, char** argv)
{
    if (argc != 3 || strcmp(argv[1], argv[2]) == 0)
    {
        fprintf(stderr, "Usage: %s input output.h264 (use a different output path)\n", argv[0]);
        return 1;
    }

    AVFormatContext* input = NULL;
    AVBSFContext* filter = NULL;
    AVPacket* packet = NULL;
    FILE* output = NULL;
    char const* operation = "open input";
    int ret = avformat_open_input(&input, argv[1], NULL, NULL);
    if (ret < 0)
        goto cleanup;

    operation = "read stream information";
    ret = avformat_find_stream_info(input, NULL);
    if (ret < 0)
        goto cleanup;

    operation = "find video stream";
    int const video_index = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index < 0)
    {
        ret = video_index;
        goto cleanup;
    }

    AVStream const* source = input->streams[video_index];
    if (source->codecpar->codec_id != AV_CODEC_ID_H264)
    {
        operation = "selected video stream is not H.264";
        ret = AVERROR(EINVAL);
        goto cleanup;
    }

    // The bitstream filter converts length-prefixed NAL units to Annex B
    // and inserts parameter sets when needed. No decoder is opened.
    operation = "find h264_mp4toannexb bitstream filter";
    AVBitStreamFilter const* bitstream_filter = av_bsf_get_by_name("h264_mp4toannexb");
    if (!bitstream_filter)
    {
        ret = AVERROR_BSF_NOT_FOUND;
        goto cleanup;
    }

    operation = "allocate bitstream filter";
    ret = av_bsf_alloc(bitstream_filter, &filter);
    if (ret < 0)
        goto cleanup;

    operation = "copy codec parameters";
    ret = avcodec_parameters_copy(filter->par_in, source->codecpar);
    if (ret < 0)
        goto cleanup;
    filter->time_base_in = source->time_base;

    operation = "initialize bitstream filter";
    ret = av_bsf_init(filter);
    if (ret < 0)
        goto cleanup;

    operation = "allocate packet";
    packet = av_packet_alloc();
    if (!packet)
    {
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    operation = "open output";
    output = fopen(argv[2], "wb");
    if (!output)
    {
        ret = AVERROR(errno);
        goto cleanup;
    }

    operation = "read packet";
    while ((ret = av_read_frame(input, packet)) >= 0)
    {
        if (packet->stream_index != video_index)
        {
            av_packet_unref(packet);
            continue;
        }
        // A successful send transfers packet ownership to the filter.
        ret = av_bsf_send_packet(filter, packet);
        if (ret < 0)
        {
            operation = "send packet to bitstream filter";
            goto cleanup;
        }
        ret = write_filtered_packets(filter, packet, output);
        if (ret < 0)
        {
            operation = "receive or write filtered packets";
            goto cleanup;
        }
    }
    if (ret != AVERROR_EOF)
        goto cleanup;

    operation = "flush bitstream filter";
    ret = av_bsf_send_packet(filter, NULL);
    if (ret >= 0)
        ret = write_filtered_packets(filter, packet, output);

cleanup:
    if (output && fclose(output) != 0 && ret >= 0)
    {
        operation = "close output";
        ret = AVERROR(EIO);
    }
    av_packet_free(&packet);
    av_bsf_free(&filter);
    avformat_close_input(&input);
    if (ret < 0)
        fprintf(stderr, "%s: %s\n", operation, av_err2str(ret));
    return ret < 0 ? 1 : 0;
}
