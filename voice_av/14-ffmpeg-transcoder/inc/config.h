#ifndef VOICE_AV_TRANSCODER_CONFIG_H
#define VOICE_AV_TRANSCODER_CONFIG_H

/* Feature macros for the lesson CLI and FFmpeg 7.1 private headers on MSVC x64. */
#define ARCH_X86 1
#define ARCH_X86_64 1
#define HAVE_BIGENDIAN 0
#define HAVE_FAST_UNALIGNED 1
#define HAVE_INLINE_ASM 0
#define HAVE_LIBC_MSVCRT 1
#define HAVE_MMX_INLINE 0

/* The Universal CRT provides the math functions used by FFmpeg's libm.h. */
#define HAVE_ATANF 1
#define HAVE_ATAN2F 1
#define HAVE_CBRT 1
#define HAVE_CBRTF 1
#define HAVE_COPYSIGN 1
#define HAVE_COSF 1
#define HAVE_ERF 1
#define HAVE_EXPF 1
#define HAVE_EXP2 1
#define HAVE_EXP2F 1
#define HAVE_HYPOT 1
#define HAVE_ISFINITE 1
#define HAVE_ISINF 1
#define HAVE_ISNAN 1
#define HAVE_LDEXPF 1
#define HAVE_LLRINT 1
#define HAVE_LLRINTF 1
#define HAVE_LOG2 1
#define HAVE_LOG2F 1
#define HAVE_LOG10F 1
#define HAVE_LRINT 1
#define HAVE_LRINTF 1
#define HAVE_POWF 1
#define HAVE_RINT 1
#define HAVE_ROUND 1
#define HAVE_ROUNDF 1
#define HAVE_SINF 1
#define HAVE_TRUNC 1
#define HAVE_TRUNCF 1

#define HAVE_ATOMICS_NATIVE 0
#define HAVE_THREADS 1
#define HAVE_W32THREADS 1

#define HAVE_DOS_PATHS 1
#define HAVE_IO_H 1
#define HAVE_DIRECT_H 1
#define HAVE_WINSOCK2_H 1
#define HAVE_SOCKLEN_T 1
#define HAVE_STRUCT_POLLFD 1
#define HAVE_CLOSESOCKET 1
#define HAVE_WINDOWS_H 1
#define HAVE_SETCONSOLECTRLHANDLER 1
#define HAVE_COMMANDLINETOARGVW 1
#define HAVE_GETPROCESSMEMORYINFO 1

#define CONFIG_AVCODEC 1
#define CONFIG_AVDEVICE 1
#define CONFIG_AVFILTER 1
#define CONFIG_AVFORMAT 1
#define CONFIG_AVUTIL 1
#define CONFIG_POSTPROC 1
#define CONFIG_SWRESAMPLE 1
#define CONFIG_SWSCALE 1
#define CONFIG_NETWORK 1
#define CONFIG_STATIC 1
#define CONFIG_CUVID 1

#define CONFIG_THIS_YEAR 2025
#define CC_IDENT "Microsoft Visual C/C++"
#define FFMPEG_DATADIR "."
#define AVCONV_DATADIR FFMPEG_DATADIR

#endif
