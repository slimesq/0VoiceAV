#ifndef AUDIOMIXER_H
#define AUDIOMIXER_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

// This byte-buffer interface accepts packed PCM only (FLT, S16, etc., not FLTP/S16P).
class AudioMixer
{
public:
    AudioMixer() = default;
    ~AudioMixer();

    int addAudioInput(uint32_t index,
                      uint32_t samplerate,
                      uint32_t channels,
                      uint32_t bitsPerSample,
                      AVSampleFormat format);
    int addAudioOutput(uint32_t samplerate,
                       uint32_t channels,
                       uint32_t bitsPerSample,
                       AVSampleFormat format);
    int init(char const* duration = "longest");
    int exit();
    // nullptr marks this input's EOF. Each input has its own sample-count PTS.
    int addFrame(uint32_t index, uint8_t const* inBuf, uint32_t size);
    // Bytes written, or AVERROR(EAGAIN)/AVERROR_EOF/another negative error.
    // ENOSPC keeps the pending frame so the caller can retry with a larger buffer.
    int getFrame(uint8_t* outBuf, uint32_t maxOutBufSize);

private:
    struct AudioInfo
    {
        uint32_t samplerate;
        uint32_t channels;
        AVSampleFormat format;
        AVFilterContext* context = nullptr;
        int64_t next_pts = 0;
        bool ended = false;
    };

    static bool validFormat(uint32_t samplerate,
                            uint32_t channels,
                            uint32_t bitsPerSample,
                            AVSampleFormat format);
    void resetGraph();
    std::mutex mutex_;
    std::map<uint32_t, AudioInfo> inputs_;
    std::unique_ptr<AudioInfo> output_;
    AVFilterGraph* graph_ = nullptr;
    AVFilterContext* sink_ = nullptr;
    AVFrame* pending_ = nullptr;
    bool initialized_ = false;
};

#endif
