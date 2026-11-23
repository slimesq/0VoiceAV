/**
* 零声学院腾讯课堂：https://0voice.ke.qq.com
* 课程咨询：QQ 326873713 Darren
*/
#include <stdio.h>
#include <iostream>
#include <memory>
#include "dlog.h"


using namespace std;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}

#include "pushwork.h"
#include "timeutil.h"
using namespace LQF;

#undef main
//减少缓冲 ffplay.exe -i rtmp://xxxxxxx -fflags nobuffer
// 减少码流分析时间 ffplay.exe -i rtmp://xxxxxxx -analyzeduration 1000000 单位为微秒
// ffplay -i rtsp://192.168.2.132/live/livestream -fflags nobuffer -analyzeduration 1000000 -rtsp_transport udp

//#define RTSP_URL "rtsp://111.229.231.225/live/livestream"
#define RTSP_URL "rtsp://192.168.2.132/live/livestream"
// ffmpeg -re -i  rtsp_test_hd.flv  -vcodec copy -acodec copy  -f flv -y rtsp://111.229.231.225/live/livestream
// ffmpeg -re -i  rtsp_test_hd.flv  -vcodec copy -acodec copy  -f flv -y rtsp://192.168.1.12/live/livestream
//// ffmpeg -re -i  1920x832_25fps.flv  -vcodec copy -acodec copy  -f flv -y rtsp://111.229.231.225/live/livestream
int main(int argc, char* argv[])
{
    init_logger("rtsp_push.log", S_INFO);

    {
        PushWork pushwork; // 可以实例化多个，同时推送多路流

        Properties properties;
        // 音频test模式
        properties.SetProperty("audio_test", 1);    // 音频测试模式
        properties.SetProperty("input_pcm_name", "buweishui_48000_2_s16le.pcm");
        // 麦克风采样属性
        properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
        properties.SetProperty("mic_sample_rate", 48000);
        properties.SetProperty("mic_channels", 2);
        // 音频编码属性
        properties.SetProperty("audio_sample_rate", 48000);
        properties.SetProperty("audio_bitrate", 64*1024);
        properties.SetProperty("audio_channels", 2);

        //视频test模式
        properties.SetProperty("video_test", 1);
        properties.SetProperty("input_yuv_name", "720x480_25fps_420p.yuv");
        // 桌面录制属性
        properties.SetProperty("desktop_x", 0);
        properties.SetProperty("desktop_y", 0);
        properties.SetProperty("desktop_width", 720); //测试模式时和yuv文件的宽度一致
        properties.SetProperty("desktop_height", 480);  //测试模式时和yuv文件的高度一致
        //    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
        properties.SetProperty("desktop_fps", 25);//测试模式时和yuv文件的帧率一致
        // 视频编码属性
        properties.SetProperty("video_bitrate", 512*1024);  // 设置码率

        // 使用缺省的
        // rtmp推流
        properties.SetProperty("rtsp_url", RTSP_URL);//测试模式时和yuv文件的帧率一致
        properties.SetProperty("rtsp_debug", 1);
        if(pushwork.Init(properties) != RET_OK)
        {
            LogError("pushwork.Init failed");
            pushwork.DeInit();
//            getchar();
            return 0;
        }
        pushwork.Loop();


        //        int count = 0;
        //        while (true) {
        //            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //             if(count++ > 1200)
        //                break;
        //        }

    } // 测试析构
    //  屏幕打印：调试 耗时1200ms，直接运行700ms,
    // 写到本地文件：调试31ms，直接运行16ms
    //    int64_t cur_time = TimesUtil::GetTimeMillisecond();
    //    for(int i = 0; i < 1000; i++) {
    //        LogError(" properties.SetProperty properties.SetProperty properties.SetProperty");
    //    }
    //    printf("t:%lld", TimesUtil::GetTimeMillisecond() - cur_time);
    printf("rtsp push finish");
//    getchar();
    return 0;
}



