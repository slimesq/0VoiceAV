#include "ijkplayer.h"
#include "ff_ffplay.h"
#include "ijkplayer_internal.h"
#include "ff_ffmsg.h"
#include "ff_ffmsg_queue.h"
#include "ijksdl_misc.h"
int ijkmp_prepare_async(IjkMediaPlayer *mp)
{

}

void ijkmp_global_init()
{
    ffp_global_init();
}

void ijkmp_global_uninit()
{
    ffp_global_uninit();
}

inline static void ijkmp_destroy(IjkMediaPlayer *mp)
{
    if (!mp)
        return;

    ffp_destroy_p(&mp->ffplayer);
    if (mp->msg_thread) {
        SDL_WaitThread(mp->msg_thread, NULL);
        mp->msg_thread = NULL;
    }

    if(mp->mutex)
    {
        SDL_DestroyMutex(mp->mutex);
    }

    freep((void**)&mp->data_source);
    memset(mp, 0, sizeof(IjkMediaPlayer));
    freep((void**)&mp);
}
inline static void ijkmp_destroy_p(IjkMediaPlayer **pmp)
{
    if (!pmp)
        return;

    ijkmp_destroy(*pmp);
    *pmp = NULL;
}

IjkMediaPlayer *ijkmp_create(int (*msg_loop)(void *), void *user)
{
    IjkMediaPlayer *mp = (IjkMediaPlayer *) mallocz(sizeof(IjkMediaPlayer));
    if (!mp)
        goto fail;

    mp->ffplayer = ffp_create();
    if (!mp->ffplayer)
        goto fail;

    mp->msg_loop = msg_loop;

    //    ijkmp_inc_ref(mp);
    mp->mutex = SDL_CreateMutex();
    if (!mp->mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        goto fail;
    }
    return mp;

fail:
    ijkmp_destroy_p(&mp);
    return NULL;
}







int ijkmp_get_msg(IjkMediaPlayer *mp, AVMessage *msg, int block)
{
    assert(mp);
    int retval;
    while (1) {
        int continue_wait_next_msg = 0;
        //        int retval = msg_queue_get(&mp->ffplayer->msg_queue, msg, block);
        if (retval <= 0)
            return retval;

        switch (msg->what) {
        case FFP_MSG_PREPARED:

            break;
        }
    }
}


int ijkmp_set_data_source(IjkMediaPlayer *mp, const char *url)
{
    assert(mp);
    assert(url);
    //       MPTRACE("ijkmp_set_data_source(url=\"%s\")\n", url);
    SDL_LockMutex(mp->mutex);
    //       int retval = ijkmp_set_data_source_l(mp, url);
    SDL_UnlockMutex(mp->mutex);
    //       MPTRACE("ijkmp_set_data_source(url=\"%s\")=%d\n", url, retval);
    return 0;
}
