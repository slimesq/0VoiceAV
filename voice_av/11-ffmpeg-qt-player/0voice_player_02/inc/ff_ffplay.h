#ifndef FF_FFPLAY_H
#define FF_FFPLAY_H

#include "ff_ffplay_def.h"
#include "ff_fferror.h"
#include "ff_ffmsg.h"

void      ffp_global_init();
void      ffp_global_uninit();
//void      ffp_global_set_log_report(int use_report);
//void      ffp_global_set_log_level(int log_level);
//void      ffp_global_set_inject_callback(ijk_inject_callback cb);
//void      ffp_io_stat_register(void (*cb)(const char *url, int type, int bytes));
//void      ffp_io_stat_complete_register(void (*cb)(const char *url,
//                                                   int64_t read_bytes, int64_t total_size,
//                                                   int64_t elpased_time, int64_t total_duration));

FFPlayer *ffp_create();
void      ffp_destroy(FFPlayer *ffp);
void      ffp_destroy_p(FFPlayer **pffp);
void      ffp_reset(FFPlayer *ffp);

#endif // FF_FFPLAY_H
