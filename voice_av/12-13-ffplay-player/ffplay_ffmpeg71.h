#ifndef VOICE_AV_FFPLAY_FFMPEG71_H
#define VOICE_AV_FFPLAY_FFMPEG71_H

#include <string.h>

#include <libavcodec/packet.h>
#include <libavfilter/buffersink.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/tx.h>
#include <libswresample/swresample.h>

static inline uint64_t ffplay_channel_mask(const AVChannelLayout *layout)
{
    return layout->order == AV_CHANNEL_ORDER_NATIVE ? layout->u.mask : 0;
}

static inline uint64_t ffplay_default_channel_mask(int channels)
{
    AVChannelLayout layout = {0};
    av_channel_layout_default(&layout, channels);
    uint64_t mask = ffplay_channel_mask(&layout);
    av_channel_layout_uninit(&layout);
    return mask;
}

static inline int ffplay_channel_count(uint64_t mask)
{
    AVChannelLayout layout = {0};
    av_channel_layout_from_mask(&layout, mask);
    return layout.nb_channels;
}

static inline void ffplay_describe_channel_mask(char *output, size_t size, int channels, uint64_t mask)
{
    AVChannelLayout layout = {0};
    if (av_channel_layout_from_mask(&layout, mask) < 0)
        av_channel_layout_default(&layout, channels);
    av_channel_layout_describe(&layout, output, size);
    av_channel_layout_uninit(&layout);
}

static inline uint64_t ffplay_sink_channel_mask(const AVFilterContext *sink)
{
    AVChannelLayout layout = {0};
    if (av_buffersink_get_ch_layout(sink, &layout) < 0)
        return 0;
    uint64_t mask = ffplay_channel_mask(&layout);
    av_channel_layout_uninit(&layout);
    return mask;
}

static inline SwrContext *ffplay_create_resampler(
    uint64_t output_mask, enum AVSampleFormat output_format, int output_rate,
    uint64_t input_mask, enum AVSampleFormat input_format, int input_rate)
{
    AVChannelLayout input_layout = {0}, output_layout = {0};
    SwrContext *resampler = NULL;
    if (av_channel_layout_from_mask(&input_layout, input_mask) < 0 ||
        av_channel_layout_from_mask(&output_layout, output_mask) < 0)
        return NULL;
    if (swr_alloc_set_opts2(&resampler, &output_layout, output_format, output_rate,
                           &input_layout, input_format, input_rate, 0, NULL) < 0)
        return NULL;
    return resampler;
}

static inline int ffplay_store_packet_position(AVPacket *packet)
{
    if (packet->buf && !packet->opaque_ref) {
        packet->opaque_ref = av_buffer_alloc(sizeof(packet->pos));
        if (!packet->opaque_ref)
            return AVERROR(ENOMEM);
        memcpy(packet->opaque_ref->data, &packet->pos, sizeof(packet->pos));
    }
    return 0;
}

static inline int64_t ffplay_packet_position(const AVFrame *frame)
{
    int64_t position = -1;
    if (frame->opaque_ref && frame->opaque_ref->size == sizeof(position))
        memcpy(&position, frame->opaque_ref->data, sizeof(position));
    return position;
}

typedef struct FFplayRDFT {
    AVTXContext *context;
    av_tx_fn transform;
    AVComplexFloat *output;
    size_t length;
} FFplayRDFT;

static inline void ffplay_rdft_free(FFplayRDFT *rdft)
{
    if (!rdft)
        return;
    av_tx_uninit(&rdft->context);
    av_free(rdft->output);
    av_free(rdft);
}

static inline FFplayRDFT *ffplay_rdft_init(int bits)
{
    const float scale = 1.0f;
    FFplayRDFT *rdft = av_mallocz(sizeof(*rdft));
    if (!rdft)
        return NULL;
    rdft->length = (size_t)1 << bits;
    rdft->output = av_malloc_array(rdft->length / 2 + 1, sizeof(*rdft->output));
    if (!rdft->output ||
        av_tx_init(&rdft->context, &rdft->transform, AV_TX_FLOAT_RDFT, 0,
                   (int)rdft->length, &scale, 0) < 0) {
        ffplay_rdft_free(rdft);
        return NULL;
    }
    return rdft;
}

static inline void ffplay_rdft_calc(FFplayRDFT *rdft, float *samples)
{
    rdft->transform(rdft->context, rdft->output, samples, sizeof(*samples));
    memcpy(samples, rdft->output, rdft->length * sizeof(*samples));
    // Preserve the packing used by the spectrum display.
    samples[1] = rdft->output[rdft->length / 2].re;
}

#endif
