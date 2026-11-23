#include "videocapturer.h"
#include "dlog.h"
#include "timeutil.h"
#include "avtimebase.h"
namespace LQF {
VideoCapturer::VideoCapturer()
{

}

VideoCapturer::~VideoCapturer()
{
    if(yuv_buf_)
        delete [] yuv_buf_;
}

RET_CODE VideoCapturer::Init(const Properties &properties)
{
    video_test_ = properties.GetProperty("video_test", 0);
    input_yuv_name_ = properties.GetProperty("input_yuv_name", "720x480_25fps_420p.yuv");
    x_ = properties.GetProperty("x", 0);
    y_ = properties.GetProperty("y", 0);
    width_ = properties.GetProperty("width", 1920);
    height_ = properties.GetProperty("height", 1080);
    pixel_format_ = properties.GetProperty("pixel_format", 0);
    fps_ = properties.GetProperty("fps", 25);
    frame_duration_ = 1000.0 / fps_;

    if(openYuvFile(input_yuv_name_.c_str()) != 0)
    {
        LogError("openYuvFile %s failed", input_yuv_name_.c_str());
        return RET_FAIL;
    }

    return RET_OK;
}

void VideoCapturer::Loop()
{
    LogInfo("into loop");

    yuv_buf_size = width_ * height_ * 1.5;
    yuv_buf_ = new uint8_t[yuv_buf_size];

    yuv_total_duration_ = 0;
    yuv_start_time_ = TimesUtil::GetTimeMillisecond();
    LogInfo("into loop while");
    while(true)
    {
        if(request_exit_)
            break;
        if(readYuvFile(yuv_buf_, yuv_buf_size) == 0)
        {
            if(!is_first_frame_) {
                is_first_frame_ = true;
                LogInfo("%s:t%u", AVPublishTime::GetInstance()->getVInTag(),
                        AVPublishTime::GetInstance()->getCurrenTime());
            }
            if(callable_object_)
            {
                callable_object_(yuv_buf_, yuv_buf_size);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    closeYuvFile();
}

int VideoCapturer::openYuvFile(const char *file_name)
{
    yuv_fp_ = fopen(file_name, "rb");
    if(!yuv_fp_)
    {
        return -1;
    }
    return 0;
}

int VideoCapturer::readYuvFile(uint8_t *yuv_buf, int32_t yuv_buf_size)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond();
    int64_t dif = cur_time - yuv_start_time_;
//    printf("%lld, %lld\n", yuv_total_duration_, dif);
    if((int64_t)yuv_total_duration_ > dif)
        return -1;
    // 该读取数据了
    size_t ret = ::fread(yuv_buf, 1, yuv_buf_size, yuv_fp_);
    if(ret != yuv_buf_size)
    {
        // 从文件头部开始读取
        ret = ::fseek(yuv_fp_, 0, SEEK_SET);
        ret = ::fread(yuv_buf, 1, yuv_buf_size, yuv_fp_);
        if(ret != yuv_buf_size)
        {

            return -1;
        }
    }
    LogDebug("yuv_total_duration_:%lldms, %lldms", (int64_t)yuv_total_duration_, dif);
    yuv_total_duration_ += frame_duration_;  //
    return 0;
}

int VideoCapturer::closeYuvFile()
{
    if(yuv_fp_)
        fclose(yuv_fp_);
    return 0;
}
}
