#ifndef AUDIOOUTSDL_H
#define AUDIOOUTSDL_H

#include <mutex>
#include <functional>
#include <SDL.h>
#include "framequeue.h"
#include "mediabase.h"
#include "avsync.h"
#include "sonic.h"
extern "C" {
#include "libswresample/swresample.h"
}

using namespace LQF;
typedef struct AudioParams {
    int			freq;                   // 采样率
    int			channels;               // 通道数
    AVChannelLayout		channel_layout;         // 通道布局，比如2.1声道，5.1声道等
    enum AVSampleFormat	fmt;            // 音频采样格式，比如AV_SAMPLE_FMT_S16表示为有符号16bit深度，交错排列模式。
    int			frame_size;             // 一个采样单元占用的字节数（比如2通道时，则左右通道各采样一次合成一个采样单元）
    int			bytes_per_sec;          // 一秒时间的字节数，比如采样率48Khz，2 channel，16bit，则一秒48000*2*16/8=192000
} AudioParams;


class AudioOutSDL
{
public:
    AudioOutSDL(AVSync *avsync, PacketQueue *packet_queue, FrameQueue *frame_queue);
    virtual ~AudioOutSDL();
    /**
     * @brief Init
     * @param   "sample_fmt", 采样格式 AVSampleFormat对应的值，缺省为AV_SAMPLE_FMT_S16
     *          "sample_rate", 采样率，缺省为480000
     *          "channels",  采样通道，缺省为2通道
     * @return
     */
    virtual RET_CODE Init(const Properties &properties);
    void Release();
    uint8_t GetPrintFrameOutputTimeCount();
    int GetMaxPrintFrameOutputTime();
    bool IsNormalPlaybackRate();

    void AddAudioCacheInfoCallback(std::function<float(float cur_speed_factor, int packet_cache, int frame_cache)> callback)
    {
         audio_cache_info_callback_ = callback;
    }

public:
    uint32_t PRINT_MAX_FRAME_OUTPUT_TIME = 5;
    uint8_t frame_ount_time_count = 0;
    std::mutex lock_;//
    double audio_clock = 0;
    int audio_volume = 80;
    AVSync *avsync_ = NULL;

    uint8_t *audio_buf = NULL;             // 指向解码后的数据，它只是一个指针，实际存储解码后的数据在audio_buf1
    uint8_t *audio_buf1 = NULL;            //
    uint32_t audio_buf1_size = 4096*4;
    uint32_t audio_buf_size = 0;        // audio_buf指向数据帧的数据长度，以字节为单位
    uint32_t audio_buf_index = 0;       // audio_buf_index当前读取的位置，不能超过audio_buf_size
    // audio_buf_size = audio_buf_index + audio_write_buf_size
    int			audio_write_buf_size = 0;
    int			audio_hw_buf_size = 0;          // SDL音频缓冲区的大小(字节为单位)
    SDL_AudioDeviceID audio_dev;
    struct AudioParams audio_src = {0};           // 音频frame的参数
    struct AudioParams audio_tgt = {0};       // SDL支持的音频参数，重采样转换：audio_src->audio_tgt
    struct SwrContext *swr_ctx = NULL;         // 音频重采样context
    int audio_clock_serial = 0;

    int paused = 0;
    int muted = 0;
    uint32_t PRINT_MAX_FRAME_OUT_TIME = 5;
    uint32_t out_frames_ = 0;       // 统计输出的帧数

    PacketQueue *packet_queue_ = NULL;
    FrameQueue *frame_queue_ = NULL;
    std::function<void(bool cache_enough)> frame_enough__callback_ = NULL;
    std::function<float(float cur_speed_factor, int packet_cache, int frame_cache)> audio_cache_info_callback_ = NULL;
    float cur_speed_factor = 1.0;

    // 变速相关
    sonicStreamStruct *audio_speed_convert_ = NULL;
    bool playback_rate_change_ = false;     // 播放系数改变的时候才会置为true, 而且不为1
    void setFrame_enough__callback(const std::function<void (bool cache_enough)> &frame_enough__callback);

private:
    SDL_AudioSpec spec;
    int sample_rate_ = 48000;
    int sample_fmt_ = AUDIO_S16SYS;
    int channels_ = 2;

    int64_t pre_pts_ = 0;


};

#endif // AUDIOOUTSDL_H
