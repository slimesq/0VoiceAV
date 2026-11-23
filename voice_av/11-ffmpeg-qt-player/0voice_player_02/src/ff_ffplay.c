#include "ff_ffplay.h"

#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define IJKPLAYER_VERSION "0voice_ijkplayer_0.1"
static bool g_ffmpeg_global_inited = false;

static void init_dynload(void)
{
#ifdef _WIN32
    /* Calling SetDllDirectory with the empty string (but not NULL) removes the
     * current working directory from the DLL search path as a security pre-caution. */
    SetDllDirectoryA("");
#endif
}

static const char *ijk_version_info()
{
    return IJKPLAYER_VERSION;
}

void ffp_global_init()
{
    //    av_log_set_level(AV_LOG_TRACE);
    init_dynload();
    // 1. 对FFmpeg的初始化
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    // av_log_set_level(AV_LOG_DEBUG);
    /* register all codecs, demux and protocols */
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif
    avformat_network_init();

    av_init_packet(&flush_pkt);				// 初始化flush_packet
    flush_pkt.data = (uint8_t *)&flush_pkt; // 初始化为数据指向自己本身

    g_ffmpeg_global_inited = true;
}

void ffp_global_uninit()
{
    if(g_ffmpeg_global_inited) {
        return;
    }
    avformat_network_deinit();
}

FFPlayer *ffp_create()
{
    av_log(NULL, AV_LOG_INFO, "av_version_info: %s\n", av_version_info());
    av_log(NULL, AV_LOG_INFO, "ijk_version_info: %s\n", ijk_version_info());

    FFPlayer* ffp = (FFPlayer*) av_mallocz(sizeof(FFPlayer));
    if (!ffp)
        return NULL;

    msg_queue_init(&ffp->msg_queue);

    ffp_reset_internal(ffp);
    return ffp;
}
