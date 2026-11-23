/* Adapted from the course example 07-01-ffmpeg-demux. */
#include <inttypes.h>
#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

static void print_stream_info(AVStream const* stream)
{
    AVCodecParameters const* parameters = stream->codecpar;
    char const* type = av_get_media_type_string(parameters->codec_type);
    printf("Stream %d: %s, codec=%s, time_base=%d/%d, duration=%s s\n",
           stream->index,
           type ? type : "unknown",
           avcodec_get_name(parameters->codec_id),
           stream->time_base.num,
           stream->time_base.den,
           av_ts2timestr(stream->duration, &stream->time_base));
    if (parameters->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        char const* sample_format = av_get_sample_fmt_name(parameters->format);
        printf("  rate=%d Hz, channels=%d, sample_fmt=%s\n",
               parameters->sample_rate,
               parameters->ch_layout.nb_channels,
               sample_format ? sample_format : "unknown");
    }
    else if (parameters->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        printf("  %dx%d, avg_frame_rate=%d/%d\n",
               parameters->width,
               parameters->height,
               stream->avg_frame_rate.num,
               stream->avg_frame_rate.den);
    }
}

int main(int argc, char** argv)
{
    static AVRational const microsecond_time_base = {1, AV_TIME_BASE};
    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [input]\n", argv[0]);
        return 1;
    }

    char const* filename = argc == 2 ? argv[1] : "believe.mp4";
    AVFormatContext* input = NULL;
    AVPacket* packet = NULL;
    char const* operation = "open input";
    int ret = avformat_open_input(&input, filename, NULL, NULL);
    if (ret < 0)
        goto cleanup;

    operation = "read stream information";
    ret = avformat_find_stream_info(input, NULL);
    if (ret < 0)
        goto cleanup;

    av_dump_format(input, 0, filename, 0);
    printf("Streams: %u, bitrate: %" PRId64 " bit/s, duration: %s s\n",
           input->nb_streams,
           input->bit_rate,
           av_ts2timestr(input->duration, &microsecond_time_base));
    for (unsigned int i = 0; i < input->nb_streams; ++i)
        print_stream_info(input->streams[i]);

    operation = "allocate packet";
    packet = av_packet_alloc();
    if (!packet)
    {
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    operation = "read packet";
    int64_t packet_count = 0;
    while ((ret = av_read_frame(input, packet)) >= 0)
    {
        if (packet_count < 10)
        {
            AVStream const* stream = input->streams[packet->stream_index];
            // PTS, DTS and duration are expressed in the stream's time base.
            printf("stream=%d pts=%s dts=%s duration=%s s size=%d pos=%" PRId64 "\n",
                   packet->stream_index,
                   av_ts2str(packet->pts),
                   av_ts2str(packet->dts),
                   av_ts2timestr(packet->duration, &stream->time_base),
                   packet->size,
                   packet->pos);
        }
        ++packet_count;
        av_packet_unref(packet);
    }

    if (ret == AVERROR_EOF)
    {
        printf("Read %" PRId64 " packets\n", packet_count);
        ret = 0;
    }

cleanup:
    if (ret < 0)
        fprintf(stderr, "%s: %s\n", operation, av_err2str(ret));
    av_packet_free(&packet);
    avformat_close_input(&input);
    return ret < 0 ? 1 : 0;
}
