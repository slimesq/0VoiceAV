#ifndef IJKPLAYER_H
#define IJKPLAYER_H
#include "ff_ffmsg_queue.h"

#ifdef __cplusplus //如果 __cplusplus被定义 ，__cplusplus只有C++中才有 成立则表示为C++编译器
                    //使用__cplusplus来判定是否需要将  extern "C"{}加入到编译中来。实现同一段代可以使用C、C++编译器编译
extern "C"
{
#endif
typedef struct IjkMediaPlayer IjkMediaPlayer;
typedef struct AVMessage AVMessage;
void            ijkmp_global_init();
void            ijkmp_global_uninit();

IjkMediaPlayer *ijkmp_create(int (*msg_loop)(void*), void * user);
int ijkmp_set_data_source(IjkMediaPlayer *mp, const char *url);
int             ijkmp_prepare_async(IjkMediaPlayer *mp);

int ijkmp_get_msg(IjkMediaPlayer *mp, AVMessage *msg, int block);
#ifdef __cplusplus
}
#endif
#endif // IJKPLAYER_H
