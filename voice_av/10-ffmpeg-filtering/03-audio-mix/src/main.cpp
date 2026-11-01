#include "audiomixer.h"

#include <array>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <memory>
#include <string>

static int mix_files(AudioMixer& mixer, FILE* first, FILE* second, FILE* output)
{
    FILE* inputs[] = {first, second};
    uint32_t const block_sizes[] = {8192, 4096};  // 1024 samples/channel at 48 kHz.
    bool ended[] = {false, false};
    std::array<uint8_t, 8192> input_buffer{};
    std::array<uint8_t, 40000> output_buffer{};
    uint64_t total = 0;
    for (;;)
    {
        for (uint32_t i = 0; i < 2; ++i)
        {
            if (ended[i])
                continue;
            size_t bytes = fread(input_buffer.data(), 1, block_sizes[i], inputs[i]);
            if (ferror(inputs[i]))
                return AVERROR(EIO);
            if (bytes)
            {
                int ret = mixer.addFrame(i, input_buffer.data(), (uint32_t)bytes);
                if (ret < 0)
                    return ret;
            }
            if (bytes < block_sizes[i])
            {
                // Even the last partial block is sent before marking this input's EOF.
                int ret = mixer.addFrame(i, nullptr, 0);
                if (ret < 0)
                    return ret;
                ended[i] = true;
            }
        }
        for (;;)
        {
            int ret = mixer.getFrame(output_buffer.data(), (uint32_t)output_buffer.size());
            if (ret == AVERROR_EOF)
            {
                printf("Mixed %" PRIu64 " bytes (96000 Hz, stereo, s16le).\n", total);
                return 0;
            }
            if (ret == AVERROR(EAGAIN))
            {
                // After all sources close, a fully drained graph must end with EOF.
                if (ended[0] && ended[1])
                    return AVERROR(EIO);
                break;
            }
            if (ret < 0)
                return ret;
            if (fwrite(output_buffer.data(), 1, ret, output) != (size_t)ret)
                return AVERROR(EIO);
            total += ret;
        }
    }
}

int main(int argc, char** argv)
{
    if (argc > 5)
    {
        fprintf(stderr,
                "Usage: %s [input_f32le.pcm [input_s16le.pcm [output.pcm "
                "[longest|shortest|first]]]]\n",
                argv[0]);
        return 1;
    }
    char const* first = argc > 1 ? argv[1] : "48000_2_f32le.pcm";
    char const* second = argc > 2 ? argv[2] : "48000_2_s16le.pcm";
    char const* destination = argc > 3 ? argv[3] : "output.pcm";
    char const* duration = argc > 4 ? argv[4] : "longest";
    if (std::string(first) == destination || std::string(second) == destination)
    {
        fprintf(stderr, "Output must differ from both inputs.\n");
        return 1;
    }
    AudioMixer mixer;
    int ret = mixer.addAudioInput(0, 48000, 2, 32, AV_SAMPLE_FMT_FLT);
    if (ret >= 0)
        ret = mixer.addAudioInput(1, 48000, 2, 16, AV_SAMPLE_FMT_S16);
    if (ret >= 0)
        ret = mixer.addAudioOutput(96000, 2, 16, AV_SAMPLE_FMT_S16);
    if (ret >= 0)
        ret = mixer.init(duration);
    if (ret < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error, sizeof(error));
        fprintf(stderr, "Audio graph initialization failed: %s\n", error);
        return 1;
    }
    using File = std::unique_ptr<FILE, decltype(&fclose)>;
    File file1(fopen(first, "rb"), fclose);
    File file2(fopen(second, "rb"), fclose);
    if (!file1 || !file2)
    {
        perror("Open input PCM");
        return 1;
    }
    File output(fopen(destination, "wb"), fclose);
    if (!output)
    {
        perror("Open output PCM");
        return 1;
    }
    ret = mix_files(mixer, file1.get(), file2.get(), output.get());
    if (fclose(output.release()))
        ret = AVERROR(EIO);
    if (ret < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error, sizeof(error));
        fprintf(stderr, "Audio mix failed: %s\n", error);
    }
    return ret < 0 ? 1 : 0;
}
