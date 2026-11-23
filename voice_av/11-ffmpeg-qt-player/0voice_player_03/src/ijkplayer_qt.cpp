#include "ijkplayer_qt.h"
#include "ff_ffmsg_queue.h"


IjkMediaPlayer *ijkmp_qt_create(int(*msg_loop)(void*), void *user)
{
    IjkMediaPlayer *mp = ijkmp_create(msg_loop, user);
    if (!mp)
        goto fail;

//    mp->ffplayer->vout = SDL_VoutAndroid_CreateForAndroidSurface();
//    if (!mp->ffplayer->vout)
//        goto fail;

//    mp->ffplayer->pipeline = ffpipeline_create_from_android(mp->ffplayer);
//    if (!mp->ffplayer->pipeline)
//        goto fail;

//    ffpipeline_set_vout(mp->ffplayer->pipeline, mp->ffplayer->vout);

    return mp;

fail:
//    ijkmp_dec_ref_p(&mp);
    return NULL;
}



static int message_loop_n(void *user)
{
    IjkPlayerQt *ijkplayerqt = (IjkPlayerQt *)user;
    IjkMediaPlayer *mp = ijkplayerqt->GetMediaPlayer();
    while (1) {
        AVMessage msg;

        int retval = ijkmp_get_msg(mp, &msg, 1);
        if (retval < 0)
            break;

        // block-get should never return 0

    }

    return 0;
}
static int message_loop(void *arg)
{
    message_loop_n(arg);


    return 0;
}


IjkPlayerQt::IjkPlayerQt(QObject *parent) :
    QObject(parent),
    mp_(NULL)
{

}

int IjkPlayerQt::Init()
{
    mp_ = ijkmp_qt_create(message_loop_n, (void *)this);
    if(!mp_)
    {
        return -1;
    }
    return 0;
}

void IjkPlayerQt::OnIjkMediaPlayer_prepareAsync(MediaPlayInfo *playinfo)
{
    int retval = 0;


    retval = ijkmp_prepare_async(mp_);
}
