#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavutil/display.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"



#include <SDL.h>
#include <SDL_thread.h>


#ifdef __cplusplus
}
#endif

#define _WIN32
#ifdef _WIN32
#include <windows.h>
#endif
#include <assert.h>

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04          // 40ms
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1           // 100ms
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0            // AV严重不同步时

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

typedef struct Clock
{
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
    char name[20];          /* 存储时钟名称以方便debug */
} Clock;

typedef struct VideoState
{
    Clock audclk;
    Clock vidclk;
    Clock extclk;

    int audio_stream;   // 音频流索引

    int av_sync_type;   // 音视频同步类型

    double audio_clock; // 当前音频帧的PTS+当前帧Duration
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;

    int audio_hw_buf_size;
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index; /* in bytes */     // 更新拷贝位置
    int audio_write_buf_size;
    int audio_volume;           // 音量
    int muted;
    struct SwrContext *swr_ctx;
    int frame_drops_early;
    int frame_drops_late;

    int sample_array_index;
    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;
    double last_vis_time;
    SDL_Texture *vis_texture;
    SDL_Texture *sub_texture;
    SDL_Texture *vid_texture;

    int subtitle_stream;
    AVStream *subtitle_st;


    double frame_timer;         // 直供video使用
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int video_stream;
    AVStream *video_st;

    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
    struct SwsContext *img_convert_ctx;
    struct SwsContext *sub_convert_ctx;
    int eof;

    char *filename;
    int width, height, xleft, ytop;
    int step;

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond *continue_read_thread;
} VideoState;


/**
获取到的实际上是:最后一帧的pts 加上 从处理最后一帧开始到现在的时间,具体参考set_clock_at 和get_clock的代码
c->pts_drift=最后一帧的pts-从处理最后一帧时间
clock=c->pts_drift+现在的时候
get_clock(&is->vidclk) ==is->vidclk.pts, av_gettime_relative() / 1000000.0 -is->vidclk.last_updated  +is->vidclk.pts
 */
static double get_clock(Clock *c)
{
    //    if (*c->queue_serial != c->serial)
    //        return NAN;
    if (c->paused) {    //如果当前是暂停状态，则返回最新的pts即可，因为暂停时时间没走
        return c->pts;
    } else {        // 如果当前正处在播放状态，则返回的时间为最新的pts + 更新pts之后流逝的时间
        double time = av_gettime_relative() / 1000000.0;
        // printf("get_clock time = %0.3lf\n", time);
        // c->pts_drift= 最后一帧的pts - 从处理最后一帧时间
        // 正常速度播放时，speed=1.0
        /* 如果是正常速度播放，speed =1.0，则为c->pts_drift + time，即是c->pts - time1 + time2，即是可以
         * 计算出来当前时钟的位置因为time都是一直在走的
         */
        /* 并不支持真正的倍速播放，只是在使用外部时钟时的一个微调
         *
         */
        // printf("base %0.3lf, \n", (time - c->last_updated) * (1.0 - c->speed));
        // (time - c->last_updated) * (1.0 - c->speed) 外部时钟变速时有影响
        return c->pts_drift + time  - (time - c->last_updated) * (1.0 - c->speed);
    }
}

static void set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;               // 当前帧的pts
    c->last_updated = time;     // 最后更新的时间，实际上是当前的一个系统时间
    c->pts_drift = c->pts - time;// 当前帧pts和系统时间的差值，正常播放情况下两者的差值应该是比较固定的，因为两者都是以时间为基准进行线性增长
    static int s_print_interval = 0;
    if(s_print_interval++ > 50)  // 用来观察pts_drift是怎么变化的，一般暂停/快进/快退其值才会大变动，如果一直是长速播放则相对值是稳定的
    {
        s_print_interval = 0;
        //printf("pts_drift = %0.3lf\n", c->pts_drift );
    }

    c->serial = serial;
}

static void set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time); // 修正外部时钟
}

static void set_clock_speed(Clock *c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

static void init_clock(Clock *c, int *queue_serial, const char *name)
{
    // 2 wanted_nb_samples 1024 -> 921
    // 4 wanted_nb_samples 1024 -> 921  = 1.111
    // 0.5 wanted_nb_samples 1024 -> 1126 = 0.909
    c->speed = 1.05;               // 目前只有选择外部时钟的时候才会变速播放
    c->paused = 0;
    c->queue_serial = queue_serial;
    if(name)
    {
        strcpy(c->name, name);
    }
    set_clock(c, NAN, -1);
}

static void sync_clock_to_slave(Clock *c, Clock *slave)
{
    double clock = get_clock(c);    // 外部时钟的时间
    double slave_clock = get_clock(slave);  // slave时钟的时间
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))// 超过最大no sync
    {
        printf("slave->name = %s, fabs(clock - slave_clock) = %0.3lf\n", slave->name, fabs(clock - slave_clock));
        set_clock(c, slave_clock, slave->serial);
    }
}

static int get_master_sync_type(VideoState *is) {
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

/* get the current master clock value */
static double get_master_clock(VideoState *is)
{
    double val;

    switch (get_master_sync_type(is)) {
    case AV_SYNC_VIDEO_MASTER:
        val = get_clock(&is->vidclk);
        break;
    case AV_SYNC_AUDIO_MASTER:
        val = get_clock(&is->audclk);
        break;
    default:
        val = get_clock(&is->extclk);
        break;
    }
    return val;
}
#undef main
int main()
{

    avformat_network_init();

    VideoState *is = (VideoState *)av_mallocz(sizeof(VideoState));		// 分配结构体并初始化;

    // 初始化时钟
    is->vidclk.serial = 0;
    is->audclk.serial = 0;
    is->extclk.serial = 0;
    init_clock(&is->vidclk, &is->vidclk.serial, "vidclk");
    init_clock(&is->audclk, &is->audclk.serial, "audclk");
    init_clock(&is->extclk, &is->extclk.serial, "extclk");
    *(is->vidclk.queue_serial) = 0;
    *(is->audclk.queue_serial) = 0;
    *(is->extclk.queue_serial) = 0;

    double audio_pts = 0.020;
    // 设置音视频同步方式
    printf("set audio clk -> master_clock\n");
    is->av_sync_type = AV_SYNC_AUDIO_MASTER;
    // 这里只是假定存在音视频流
    is->audio_st = 1;
    is->video_st = 1;


    // 测试audio 设置pts，然后马上读取数值
    printf("audio set master_clock to %0.3lf\n", audio_pts);
    set_clock(&is->audclk, audio_pts, is->audclk.serial);
    // 从主时钟去读取当前值
    printf("get master_clock = %0.3lf\n\n", get_master_clock(is));
    // 休眠40ms再次去读取主时钟
    av_usleep(40000);
    printf("sleep 40ms, get master_clock = %0.3lf\n", get_master_clock(is));
    av_usleep(40000);
    printf("sleep 40ms, get master_clock = %0.3lf\n", get_master_clock(is));

    audio_pts += 0.020;
    printf("\naudio set clock to %0.3lf\n", audio_pts);
    set_clock(&is->audclk, audio_pts, is->audclk.serial);
    printf("video set clock to %0.3lf\n", audio_pts + 0.020);
    set_clock(&is->vidclk, audio_pts + 0.020, is->vidclk.serial);
    printf("ext   set clock to %0.3lf\n", audio_pts + 0.020*2);
    set_clock(&is->extclk, audio_pts + 0.020*2, is->vidclk.serial);
    // 从主时钟去读取当前值
    printf("get master_clock = %0.3lf\n", get_master_clock(is));

    // 休眠20ms再次去读取主时钟
    printf("sleep 20ms\n");
    av_usleep(20000);
//    is->av_sync_type = AV_SYNC_VIDEO_MASTER;
//    printf("change master clk to video clk\n");
//    printf("sleep 20ms, VIDEO get master_clock = %0.3lf\n", get_master_clock(is));
    // 休眠20ms再次去读取主时钟
    printf("sleep 20ms\n");
    av_usleep(20000);
    is->av_sync_type = AV_SYNC_AUDIO_MASTER;
    printf("AUDIO get master_clock = %0.3lf\n", get_master_clock(is));
    is->av_sync_type = AV_SYNC_VIDEO_MASTER;
    printf("VIDEO get master_clock = %0.3lf\n", get_master_clock(is));
    is->av_sync_type = AV_SYNC_EXTERNAL_CLOCK;
    printf("EXTER get master_clock = %0.3lf\n", get_master_clock(is));

    av_free(is);
    return 0;
}
