#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include "dlog.h"
#include "pushwork.h"
#include "messagequeue.h"

struct LoggerGuard
{
    ~LoggerGuard() { close_logger(); }
};

static void printUsage(const char *_program)
{
    std::cout << "Usage: " << _program
              << " <rtmp_url> <input_s16le.pcm> <input_yuv420p.yuv>"
                 " [duration_seconds=100] [width=720] [height=480] [fps=25]\n"
                 "Audio input: stereo signed 16-bit little-endian PCM, 48000 Hz.\n"
                 "Video input: planar YUV420P at the specified even dimensions and frame rate.\n"
                 "Both inputs loop until duration_seconds elapses.\n"
                 "Requires an RTMP server, AAC encoder and H.264 encoder.\n"
                 "Debug dumps are written to the current working directory.\n";
}

static bool parsePositiveInt(const char *_text, int &_value) noexcept
{
    const std::string_view text{_text};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), _value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && _value > 0;
}

static bool checkInputFile(const char *_name, const uintmax_t _minimumBytes)
{
    std::error_code error;
    const auto path = std::filesystem::u8path(_name);
    if(!std::filesystem::is_regular_file(path, error) || error) {
        std::cerr << "Cannot read input file: " << _name << '\n';
        return false;
    }
    std::ifstream input{path, std::ios::binary};
    if(!input) {
        std::cerr << "Cannot open input file: " << _name << '\n';
        return false;
    }
    const auto bytes = std::filesystem::file_size(path, error);
    if(error || bytes < _minimumBytes) {
        std::cerr << "Input does not contain one complete frame: " << _name << '\n';
        return false;
    }
    return true;
}

int main(int _argc, char **_argv)
{
    if(_argc == 1 || (_argc == 2 && std::string_view{_argv[1]} == "--help")) {
        printUsage(_argv[0]);
        return 0;
    }
    if(_argc < 4 || _argc > 8) {
        printUsage(_argv[0]);
        return 1;
    }
    const std::string url{_argv[1]};
    if(url.rfind("rtmp://", 0) != 0 && url.rfind("rtmps://", 0) != 0) {
        std::cerr << "Use an explicit rtmp:// or rtmps:// URL.\n";
        return 1;
    }
    int durationSeconds{100};
    int width{720};
    int height{480};
    int fps{25};
    int *options[]{&durationSeconds, &width, &height, &fps};
    for(int index{4}; index < _argc; ++index) {
        if(!parsePositiveInt(_argv[index], *options[index - 4])) {
            std::cerr << "Duration, dimensions and frame rate must be positive integers.\n";
            return 1;
        }
    }
    if(width % 2 != 0 || height % 2 != 0 || width > 8192 || height > 8192 || fps > 240) {
        std::cerr << "Use even dimensions up to 8192 and a frame rate up to 240.\n";
        return 1;
    }
    const uintmax_t yuvFrameBytes = static_cast<uintmax_t>(width) * height * 3 / 2;
    // Validate local input before establishing a network connection.
    if(!checkInputFile(_argv[2], 1024 * 2 * sizeof(int16_t))
            || !checkInputFile(_argv[3], yuvFrameBytes)) {
        return 1;
    }
    if(!init_logger("rtmp_push_logs", S_INFO)) {
        std::cerr << "Cannot initialize the logger.\n";
        return 1;
    }

    LoggerGuard loggerGuard{};
    MessageQueue messageQueue{};
    PushWork pushWork{&messageQueue};
    Properties properties{};
    properties.SetProperty("audio_test", 1);
    properties.SetProperty("input_pcm_name", _argv[2]);
    properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
    properties.SetProperty("mic_sample_rate", 48000);
    properties.SetProperty("mic_channels", 2);
    properties.SetProperty("audio_sample_rate", 48000);
    properties.SetProperty("audio_bitrate", 64 * 1024);
    properties.SetProperty("audio_channels", 2);
    properties.SetProperty("video_test", 1);
    properties.SetProperty("input_yuv_name", _argv[3]);
    properties.SetProperty("desktop_x", 0);
    properties.SetProperty("desktop_y", 0);
    properties.SetProperty("desktop_width", width);
    properties.SetProperty("desktop_height", height);
    properties.SetProperty("desktop_fps", fps);
    properties.SetProperty("video_bitrate", 512 * 1024);
    properties.SetProperty("rtmp_url", url.c_str());
    properties.SetProperty("rtmp_timeout", 5000);
    properties.SetProperty("rtmp_max_queue_duration", 1000);
    if(pushWork.Init(properties) != RET_OK) {
        LogError("PushWork init failed");
        return 1;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{durationSeconds};
    int result{0};
    while(std::chrono::steady_clock::now() < deadline) {
        AVMessage message{};
        const int status = messageQueue.msg_queue_get(&message, 100);
        if(status < 0) { break; }
        if(status == 1) {
            if(message.what == MSG_RTSP_ERROR) {
                LogError("RTMP write failed: %d", message.arg1);
                result = 1;
            } else if(message.what == MSG_RTSP_QUEUE_DURATION) {
                LogWarn("Queued duration: audio=%d ms, video=%d ms", message.arg1, message.arg2);
            }
            if(message.obj && message.free_l) { message.free_l(message.obj); }
            if(result != 0) { break; }
        }
    }
    pushWork.DeInit();
    messageQueue.msg_queue_abort();
    LogInfo("RTMP publish finished");
    return result;
}
