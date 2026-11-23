/* Adapted from the course example 07-02-extract-aac. */
#include <stdio.h>
#include <string.h>

#include <libavformat/avformat.h>

int main(int argc, char** argv)
{
    if (argc != 3 || strcmp(argv[1], argv[2]) == 0)
    {
        fprintf(stderr, "Usage: %s input output.aac (use a different output path)\n", argv[0]);
        return 1;
    }

    AVFormatContext* input = NULL;
    AVFormatContext* output = NULL;
    AVPacket* packet = NULL;
    char const* operation = "open input";
    int ret = avformat_open_input(&input, argv[1], NULL, NULL);
    if (ret < 0)
        goto cleanup;

    operation = "read stream information";
    ret = avformat_find_stream_info(input, NULL);
    if (ret < 0)
        goto cleanup;

    operation = "find audio stream";
    int const audio_index = av_find_best_stream(input, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index < 0)
    {
        ret = audio_index;
        goto cleanup;
    }

    AVStream const* source = input->streams[audio_index];
    if (source->codecpar->codec_id != AV_CODEC_ID_AAC)
    {
        operation = "selected audio stream is not AAC";
        ret = AVERROR(EINVAL);
        goto cleanup;
    }

    // The course source assembles ADTS headers by hand. The ADTS muxer
    // uses codec extradata and can also pass through packets already in ADTS.
    operation = "allocate ADTS muxer";
    ret = avformat_alloc_output_context2(&output, NULL, "adts", argv[2]);
    if (ret < 0)
        goto cleanup;

    operation = "allocate output stream";
    AVStream* destination = avformat_new_stream(output, NULL);
    if (!destination)
    {
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    operation = "copy codec parameters";
    ret = avcodec_parameters_copy(destination->codecpar, source->codecpar);
    if (ret < 0)
        goto cleanup;
    destination->codecpar->codec_tag = 0;
    destination->time_base = source->time_base;

    operation = "allocate packet";
    packet = av_packet_alloc();
    if (!packet)
    {
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    operation = "open output";
    ret = avio_open(&output->pb, argv[2], AVIO_FLAG_WRITE);
    if (ret < 0)
        goto cleanup;

    operation = "initialize ADTS muxer";
    ret = avformat_write_header(output, NULL);
    if (ret < 0)
        goto cleanup;

    operation = "read packet";
    while ((ret = av_read_frame(input, packet)) >= 0)
    {
        if (packet->stream_index == audio_index)
        {
            // The compressed payload is copied without decoding or encoding.
            av_packet_rescale_ts(packet, source->time_base, destination->time_base);
            packet->stream_index = destination->index;
            ret = av_write_frame(output, packet);
            if (ret < 0)
            {
                operation = "write ADTS packet";
                goto cleanup;
            }
        }
        av_packet_unref(packet);
    }
    if (ret != AVERROR_EOF)
        goto cleanup;

    operation = "finish ADTS output";
    ret = av_write_trailer(output);

cleanup:
    av_packet_free(&packet);
    avformat_close_input(&input);
    if (output && output->pb)
    {
        int const close_ret = avio_closep(&output->pb);
        if (ret >= 0 && close_ret < 0)
        {
            operation = "close output";
            ret = close_ret;
        }
    }
    avformat_free_context(output);
    if (ret < 0)
        fprintf(stderr, "%s: %s\n", operation, av_err2str(ret));
    return ret < 0 ? 1 : 0;
}
