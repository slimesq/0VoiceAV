#ifndef AUDIOCAPTURER_H
#define AUDIOCAPTURER_H
#include <functional>
#include "commonlooper.h"
#include "mediabase.h"
namespace LQF
{
using std::function;

class AudioCapturer : public CommonLooper
{
public:
    AudioCapturer();
    virtual ~AudioCapturer();
    /**
     * @brief Init
     * @param "audio_test": 缺省为0，为1时数据读取本地文件进行播放
     *        "input_pcm_name": 测试模式时读取的文件路径
     *        "sample_rate": 采样率
     *        "channels": 采样通道
     *        "sample_fmt": 采样格式
     * @return
     */
    RET_CODE Init(const Properties& properties);

    virtual void Loop();
    void AddCallback(function<void(uint8_t*, int32_t)> callback)
    {
        callback_get_pcm_ = callback;
    }
private:
    // 初始化参数
    int audio_test_ = 0;
    std::string input_pcm_name_;
    // PCM file只是用来测试, 写死为s16格式 2通道 采样率48Khz
    // 1帧1024采样点持续的时间21.333333333333333333333333333333ms
    int openPcmFile(const char *file_name);
    int readPcmFile(uint8_t *pcm_buf, int32_t pcm_buf_size);
    int closePcmFile();
    int64_t pcm_start_time_ = 0;    // 起始时间
    double pcm_total_duration_ = 0;        // PCM读取累计的时间
    FILE *pcm_fp_ = NULL;


    function<void(uint8_t*, int32_t)> callback_get_pcm_ = NULL;
    uint8_t *pcm_buf_;
    int32_t pcm_buf_size_;
    const int PCM_BUF_MAX_SIZE = 32768; //

    bool is_first_frame_ = false;

    int sample_rate_ = 48000;
};
}
#endif // AUDIOCAPTURER_H
