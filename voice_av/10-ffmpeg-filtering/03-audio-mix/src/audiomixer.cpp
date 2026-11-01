#include "audiomixer.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>

extern "C"
{
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
}

AudioMixer::~AudioMixer()
{
    exit();
}

bool AudioMixer::validFormat(uint32_t samplerate,
                             uint32_t channels,
                             uint32_t bitsPerSample,
                             AVSampleFormat format)
{
    int bytes = av_get_bytes_per_sample(format);
    return samplerate > 0 && samplerate <= INT_MAX && channels > 0 && channels <= 64 && bytes > 0 &&
        bitsPerSample == (uint32_t)bytes * 8 && !av_sample_fmt_is_planar(format);
}

int AudioMixer::addAudioInput(uint32_t index,
                              uint32_t samplerate,
                              uint32_t channels,
                              uint32_t bitsPerSample,
                              AVSampleFormat format)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (graph_ || !validFormat(samplerate, channels, bitsPerSample, format) || inputs_.count(index))
        return AVERROR(EINVAL);
    inputs_.emplace(index, AudioInfo{samplerate, channels, format});
    return 0;
}

int AudioMixer::addAudioOutput(uint32_t samplerate,
                               uint32_t channels,
                               uint32_t bitsPerSample,
                               AVSampleFormat format)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (graph_ || !validFormat(samplerate, channels, bitsPerSample, format))
        return AVERROR(EINVAL);
    output_ = std::make_unique<AudioInfo>(AudioInfo{samplerate, channels, format});
    return 0;
}

void AudioMixer::resetGraph()
{
    // The graph owns every filter context. Freeing them separately would double-free them.
    avfilter_graph_free(&graph_);
    av_frame_free(&pending_);
    sink_ = nullptr;
    initialized_ = false;
    for (auto& entry : inputs_)
    {
        entry.second.context = nullptr;
        entry.second.next_pts = 0;
        entry.second.ended = false;
    }
}

int AudioMixer::init(char const* duration)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (graph_ || inputs_.empty() || !output_ || !duration ||
        (strcmp(duration, "longest") && strcmp(duration, "shortest") && strcmp(duration, "first")))
        return AVERROR(EINVAL);
    graph_ = avfilter_graph_alloc();
    pending_ = av_frame_alloc();
    auto fail = [this](int error) {
        resetGraph();
        return error;
    };
    if (!graph_ || !pending_)
        return fail(AVERROR(ENOMEM));

    char args[512];
    AVFilterContext *mix = nullptr, *format = nullptr;
    snprintf(args,
             sizeof(args),
             "inputs=%zu:duration=%s:dropout_transition=0:normalize=1",
             inputs_.size(),
             duration);
    int ret = avfilter_graph_create_filter(
        &mix, avfilter_get_by_name("amix"), "mix", args, nullptr, graph_);
    if (ret < 0)
        return fail(ret);
    unsigned pad = 0;
    for (auto& entry : inputs_)
    {
        AudioInfo& input = entry.second;
        AVChannelLayout layout{};
        char layout_name[128], name[64];
        av_channel_layout_default(&layout, input.channels);
        ret = av_channel_layout_describe(&layout, layout_name, sizeof(layout_name));
        av_channel_layout_uninit(&layout);
        if (ret < 0)
            return fail(ret);
        snprintf(name, sizeof(name), "input_%u", entry.first);
        snprintf(args,
                 sizeof(args),
                 "sample_rate=%u:sample_fmt=%s:channel_layout=%s:time_base=1/%u",
                 input.samplerate,
                 av_get_sample_fmt_name(input.format),
                 layout_name,
                 input.samplerate);
        ret = avfilter_graph_create_filter(
            &input.context, avfilter_get_by_name("abuffer"), name, args, nullptr, graph_);
        if (ret < 0 || (ret = avfilter_link(input.context, 0, mix, pad++)) < 0)
            return fail(ret);
    }

    AVChannelLayout layout{};
    char layout_name[128];
    av_channel_layout_default(&layout, output_->channels);
    ret = av_channel_layout_describe(&layout, layout_name, sizeof(layout_name));
    av_channel_layout_uninit(&layout);
    if (ret < 0)
        return fail(ret);
    snprintf(args,
             sizeof(args),
             "sample_rates=%u:sample_fmts=%s:channel_layouts=%s",
             output_->samplerate,
             av_get_sample_fmt_name(output_->format),
             layout_name);
    if ((ret = avfilter_graph_create_filter(
             &format, avfilter_get_by_name("aformat"), "format", args, nullptr, graph_)) < 0 ||
        (ret = avfilter_graph_create_filter(
             &sink_, avfilter_get_by_name("abuffersink"), "sink", nullptr, nullptr, graph_)) < 0 ||
        (ret = avfilter_link(mix, 0, format, 0)) < 0 ||
        (ret = avfilter_link(format, 0, sink_, 0)) < 0 ||
        (ret = avfilter_graph_config(graph_, nullptr)) < 0)
        return fail(ret);
    initialized_ = true;
    return 0;
}

int AudioMixer::exit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    resetGraph();
    inputs_.clear();
    output_.reset();
    return 0;
}

int AudioMixer::addFrame(uint32_t index, uint8_t const* inBuf, uint32_t size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = inputs_.find(index);
    if (!initialized_ || it == inputs_.end())
        return AVERROR(EINVAL);
    AudioInfo& input = it->second;
    if (!inBuf)
    {
        if (size)
            return AVERROR(EINVAL);
        if (input.ended)
            return 0;
        int ret = av_buffersrc_close(input.context, input.next_pts, 0);
        if (ret >= 0)
            input.ended = true;
        return ret;
    }
    if (input.ended)
        return AVERROR_EOF;
    unsigned stride = input.channels * av_get_bytes_per_sample(input.format);
    if (!size || size % stride || size / stride > INT_MAX)
        return AVERROR(EINVAL);
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return AVERROR(ENOMEM);
    frame->format = input.format;
    frame->sample_rate = input.samplerate;
    av_channel_layout_default(&frame->ch_layout, input.channels);
    frame->nb_samples = size / stride;
    frame->pts = input.next_pts;
    int ret = av_frame_get_buffer(frame, 0);
    if (ret >= 0)
    {
        memcpy(frame->data[0], inBuf, size);
        ret = av_buffersrc_add_frame_flags(input.context, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret >= 0)
            input.next_pts += frame->nb_samples;
    }
    av_frame_free(&frame);
    return ret;
}

int AudioMixer::getFrame(uint8_t* outBuf, uint32_t maxOutBufSize)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !outBuf)
        return AVERROR(EINVAL);
    if (!pending_->nb_samples)
    {
        int ret = av_buffersink_get_frame(sink_, pending_);
        if (ret < 0)
            return ret;
    }
    int bytes = av_samples_get_buffer_size(nullptr,
                                           pending_->ch_layout.nb_channels,
                                           pending_->nb_samples,
                                           (AVSampleFormat)pending_->format,
                                           1);
    if (bytes < 0)
        return bytes;
    if ((uint32_t)bytes > maxOutBufSize)
        return AVERROR(ENOSPC);
    memcpy(outBuf, pending_->data[0], bytes);
    av_frame_unref(pending_);
    return bytes;
}
