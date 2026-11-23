#ifndef UTIL_H
#define UTIL_H

#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavcodec/avcodec.h"
#include "libavutil/pixfmt.h"

AVFrame* av_frame_malloc(int width, int height, int format);
int save_frame_to_i420p(char const* filename, AVFrame const* frame);
int save_frame_to_jpeg(char const* filename, AVFrame const* frame);
AVFrame* get_frame_from_yuvile(char const* filename, int width, int height, int format);
AVFrame* get_frame_from_jpegfile(char const* filename);

#endif  // UTIL_H
