#ifndef FFMPEG_MYFFMPEG_H
#define FFMPEG_MYFFMPEG_H

#include <pthread.h>
#include <android/log.h>
#include "VideoChannel.h"
#include "AudioChannel.h"

extern "C" {
#include "libavformat/avformat.h"
};

/**
 * 控制层
 */
class FFmpegPlayer {

public:

    FFmpegPlayer(JavaCallHelper *javaCallHelper, const char *dataSource);// 构造
    ~FFmpegPlayer();// 析构
    void prepare();// 准备播放
    void prepareFFmpeg();

    void start();// 开始播放
    void stop();
    void readPacket();// 获取音视频数据包

    void setRenderCallback(RenderFrame);
    pthread_t pid_prepare;// FFmpeg初始化线程，初始化完成后销毁
    pthread_t pid_stop;
    pthread_t pid_read;// 解码线程，直到播放完毕后销毁
    VideoChannel *p_video_channel;// 视频解码
    AudioChannel *p_audio_channel;// 音频解码
    AVFormatContext *p_fmt_ctx;// 总上下文
    bool is_playing;// 是否在播放
    JavaCallHelper *p_java_call_helper;// 回调Java层的接口
private:


    int st_index[AVMEDIA_TYPE_NB];
    int audio_stream;
    int video_stream;

    char *url;// 播放地址

    RenderFrame render_frame;
};


#endif //FFMPEG_MYFFMPEG_H
