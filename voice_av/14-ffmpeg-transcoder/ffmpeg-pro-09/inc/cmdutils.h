/*
 * Various utilities for command line tools
 * copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FFTOOLS_CMDUTILS_H
#define FFTOOLS_CMDUTILS_H

#include <stdint.h>

#include "config.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

#ifdef _WIN32
#undef main /* We don't want SDL to override our main() */
#endif

/**
 * program name, defined by the program for show_version().
 */
extern const char program_name[];

/**
 * program birth year, defined by the program for show_banner()
 */
extern const int program_birth_year;

extern AVCodecContext *avcodec_opts[AVMEDIA_TYPE_NB];
extern AVFormatContext *avformat_opts;
extern AVDictionary *sws_dict;
extern AVDictionary *swr_opts;
extern AVDictionary *format_opts, *codec_opts, *resample_opts;
extern int hide_banner;

/**
 * Register a program-specific cleanup routine.
 */
void register_exit(void (*cb)(int ret));

/**
 * Wraps exit with a program-specific cleanup routine.
 */
__declspec(noreturn) void exit_program(int ret);

/**
 * Initialize dynamic library loading
 */
void init_dynload(void);

/**
 * Initialize the cmdutils option system, in particular
 * allocate the *_opts contexts.
 */
void init_opts(void);
/**
 * Uninitialize the cmdutils option system, in particular
 * free the *_opts contexts and their contents.
 */
void uninit_opts(void);

/**
 * Trivial log callback.
 * Only suitable for opt_help and similar since it lacks prefix handling.
 */
void log_callback_help(void* ptr, int level, const char* fmt, va_list vl);

/**
 * Override the cpuflags.
 */
int opt_cpuflags(void *optctx, const char *opt, const char *arg);

/**
 * Fallback for options that are not explicitly handled, these will be
 * parsed through AVOptions.
 */
int opt_default(void *optctx, const char *opt, const char *arg);

/**
 * Set the libav* libraries log level.
 */
int opt_loglevel(void *optctx, const char *opt, const char *arg);

int opt_report(const char *opt);

int opt_max_alloc(void *optctx, const char *opt, const char *arg);

int opt_codec_debug(void *optctx, const char *opt, const char *arg);

/**
 * Limit the execution time.
 */
int opt_timelimit(void *optctx, const char *opt, const char *arg);

/**
 * Parse a string and return its corresponding value as a double.
 * Exit from the application if the string cannot be correctly
 * parsed or the corresponding value is invalid.
 *
 * @param context the context of the value to be set (e.g. the
 * corresponding command line option name)
 * @param numstr the string to be parsed
 * @param type the type (OPT_INT64 or OPT_FLOAT) as which the
 * string should be parsed
 * @param min the minimum valid accepted value
 * @param max the maximum valid accepted value
 */
double parse_number_or_die(const char *context, const char *numstr, int type,
                           double min, double max);

/**
 * Parse a string specifying a time and return its corresponding
 * value as a number of microseconds. Exit from the application if
 * the string cannot be correctly parsed.
 *
 * @param context the context of the value to be set (e.g. the
 * corresponding command line option name)
 * @param timestr the string to be parsed
 * @param is_duration a flag which tells how to interpret timestr, if
 * not zero timestr is interpreted as a duration, otherwise as a
 * date
 *
 * @see av_parse_time()
 */
int64_t parse_time_or_die(const char *context, const char *timestr,
                          int is_duration);

typedef struct SpecifierOpt {
    char *specifier;    /**< v,  a  stream/chapter/program/... specifier */
    union {
        uint8_t *str;   // 对于编解码器是str的解析
        int        i;
        int64_t  i64;
        uint64_t ui64;
        float      f;
        double   dbl;
    } u;
} SpecifierOpt;

typedef struct OptionDef {
    const char *name;   // 命令名 参数名,
    int flags;  // 可以理解为命令 key对应value的类型，可以进行|操作
#define HAS_ARG    0x0001 //即命令行含有参数选项的标志, 比如 -ss 需要后面带时间选项
#define OPT_BOOL   0x0002 //布尔型数据的标志
#define OPT_EXPERT 0x0004
#define OPT_STRING 0x0008   //字符串的标志
#define OPT_VIDEO  0x0010   //视频的标志
#define OPT_AUDIO  0x0020   //音频的标志
#define OPT_INT    0x0080   //int类型的标志
#define OPT_FLOAT  0x0100   //float类型的标志
#define OPT_SUBTITLE 0x0200 // 字幕的标志
#define OPT_INT64  0x0400   //int64的标志
#define OPT_EXIT   0x0800   //退出标志，即是处理完参数就退出程序，比如 -h显示帮助
#define OPT_DATA   0x1000   //数据的标志
#define OPT_PERFILE  0x2000     /* the option is per-file (currently ffmpeg-only).
                                   implied by OPT_OFFSET or OPT_SPEC */
#define OPT_OFFSET 0x4000       /* option is specified as an offset in a passed optctx */
#define OPT_SPEC   0x8000       /* option is to be stored in an array of SpecifierOpt.
                                   Implies OPT_OFFSET. Next element after the offset is
                                   an int containing element count in the array. */
#define OPT_TIME  0x10000       // 时间类型 比如 00:10:00
#define OPT_DOUBLE 0x20000      // double类型标志
#define OPT_INPUT  0x40000      // 为输入参数
#define OPT_OUTPUT 0x80000      // 为输出参数
     union {    // 共用体，解决
        void *dst_ptr;      // 解析后的数据保存到全局变量
        int (*func_arg)(void *, const char *, const char *); // 使用函数进行解析
        size_t off;     // 偏移位置，一般是指在OptionsContext的位置
    } u;
    const char *help;   // 参数选项的用处
    const char *argname;    // 参数选项名字
} OptionDef;    // 定义命令结构体
/**
1. 比如 &file_overwrite参数的值放到全局变量file_overwrite中。
2. 比如 .off       = OFFSET(codec_names) 参数的值放到结构体OptionsContext的codec_names中。
这个结构体变量将在open_files中定义。
3. 比如.func_arg = opt_video_frames表示要调用opt_video_frames函数处理此参数的值。
*/
/**
 * Print help for all options matching specified flags.
 *
 * @param options a list of options
 * @param msg title of this group. Only printed if at least one option matches.
 * @param req_flags print only options which have all those flags set.
 * @param rej_flags don't print options which have any of those flags set.
 * @param alt_flags print only options that have at least one of those flags set
 */
void show_help_options(const OptionDef *options, const char *msg, int req_flags,
                       int rej_flags, int alt_flags);

#if CONFIG_AVDEVICE
#define CMDUTILS_COMMON_OPTIONS_AVDEVICE                                                                                \
    { "sources"    , OPT_EXIT | HAS_ARG, { .func_arg = show_sources },                                                  \
      "list sources of the input device", "device" },                                                                   \
    { "sinks"      , OPT_EXIT | HAS_ARG, { .func_arg = show_sinks },                                                    \
      "list sinks of the output device", "device" },                                                                    \

#else
#define CMDUTILS_COMMON_OPTIONS_AVDEVICE
#endif

#define CMDUTILS_COMMON_OPTIONS                                                                                         \
    { "L",           OPT_EXIT,             { .func_arg = show_license },     "show license" },                          \
    { "h",           OPT_EXIT,             { .func_arg = show_help },        "show help", "topic" },                    \
    { "?",           OPT_EXIT,             { .func_arg = show_help },        "show help", "topic" },                    \
    { "help",        OPT_EXIT,             { .func_arg = show_help },        "show help", "topic" },                    \
    { "-help",       OPT_EXIT,             { .func_arg = show_help },        "show help", "topic" },                    \
    { "version",     OPT_EXIT,             { .func_arg = show_version },     "show version" },                          \
    { "buildconf",   OPT_EXIT,             { .func_arg = show_buildconf },   "show build configuration" },              \
    { "formats",     OPT_EXIT,             { .func_arg = show_formats },     "show available formats" },                \
    { "muxers",      OPT_EXIT,             { .func_arg = show_muxers },      "show available muxers" },                 \
    { "demuxers",    OPT_EXIT,             { .func_arg = show_demuxers },    "show available demuxers" },               \
    { "devices",     OPT_EXIT,             { .func_arg = show_devices },     "show available devices" },                \
    { "codecs",      OPT_EXIT,             { .func_arg = show_codecs },      "show available codecs" },                 \
    { "decoders",    OPT_EXIT,             { .func_arg = show_decoders },    "show available decoders" },               \
    { "encoders",    OPT_EXIT,             { .func_arg = show_encoders },    "show available encoders" },               \
    { "bsfs",        OPT_EXIT,             { .func_arg = show_bsfs },        "show available bit stream filters" },     \
    { "protocols",   OPT_EXIT,             { .func_arg = show_protocols },   "show available protocols" },              \
    { "filters",     OPT_EXIT,             { .func_arg = show_filters },     "show available filters" },                \
    { "pix_fmts",    OPT_EXIT,             { .func_arg = show_pix_fmts },    "show available pixel formats" },          \
    { "layouts",     OPT_EXIT,             { .func_arg = show_layouts },     "show standard channel layouts" },         \
    { "sample_fmts", OPT_EXIT,             { .func_arg = show_sample_fmts }, "show available audio sample formats" },   \
    { "colors",      OPT_EXIT,             { .func_arg = show_colors },      "show available color names" },            \
    { "loglevel",    HAS_ARG,              { .func_arg = opt_loglevel },     "set logging level", "loglevel" },         \
    { "v",           HAS_ARG,              { .func_arg = opt_loglevel },     "set logging level", "loglevel" },         \
    { "report",      0,                    { (void*)opt_report },            "generate a report" },                     \
    { "max_alloc",   HAS_ARG,              { .func_arg = opt_max_alloc },    "set maximum size of a single allocated block", "bytes" }, \
    { "cpuflags",    HAS_ARG | OPT_EXPERT, { .func_arg = opt_cpuflags },     "force specific cpu flags", "flags" },     \
    { "hide_banner", OPT_BOOL | OPT_EXPERT, {&hide_banner},     "do not show program banner", "hide_banner" },          \
    CMDUTILS_COMMON_OPTIONS_AVDEVICE                                                                                    \

/**
 * Show help for all options with given flags in class and all its
 * children.
 */
void show_help_children(const AVClass *class, int flags);

/**
 * Per-fftool specific help handler. Implemented in each
 * fftool, called by show_help().
 */
void show_help_default(const char *opt, const char *arg);

/**
 * Generic -h handler common to all fftools.
 */
int show_help(void *optctx, const char *opt, const char *arg);

/**
 * Parse the command line arguments.
 *
 * @param optctx an opaque options context
 * @param argc   number of command line arguments
 * @param argv   values of command line arguments
 * @param options Array with the definitions required to interpret every
 * option of the form: -option_name [argument]
 * @param parse_arg_function Name of the function called to process every
 * argument without a leading option name flag. NULL if such arguments do
 * not have to be processed.
 */
void parse_options(void *optctx, int argc, char **argv, const OptionDef *options,
                   void (* parse_arg_function)(void *optctx, const char*));

/**
 * Parse one given option.
 *
 * @return on success 1 if arg was consumed, 0 otherwise; negative number on error
 */
int parse_option(void *optctx, const char *opt, const char *arg,
                 const OptionDef *options);

/**
 * An option extracted from the commandline.
 * Cannot use AVDictionary because of options like -map which can be
 * used multiple times.
 */
typedef struct Option {
    const OptionDef  *opt;      // 对应的参数解析
    const char       *key;      // 对应的key
    const char       *val;      // 解析出来的value
} Option;

typedef struct OptionGroupDef {
    /**< group name */
    const char *name;   // 组名，哪里设置是？
    /**
     * Option to be used as group separator. Can be NULL for groups which
     * are terminated by a non-option argument (e.g. ffmpeg output files)
     */
    const char *sep;    // 用来做组的分隔符
    /**
     * Option flags that must be set on each option that is
     * applied to this group
     */
    int flags;
} OptionGroupDef;

typedef struct OptionGroup {
    const OptionGroupDef *group_def;
    const char *arg;    // 定义了组别的名称

    Option *opts;   // 存储参数
    int  nb_opts;   // 所属组的参数个数
    // 专属参数会先存储到 AVDictionary
    AVDictionary *codec_opts;
    AVDictionary *format_opts;
    AVDictionary *resample_opts;
    AVDictionary *sws_dict;
    AVDictionary *swr_opts;
} OptionGroup;

/**
 * A list of option groups that all have the same group type
 * (e.g. input files or output files)
 */
typedef struct OptionGroupList {
    const OptionGroupDef *group_def;

    OptionGroup *groups;    //
    int       nb_groups;    // 多少组
} OptionGroupList;

typedef struct OptionParseContext {
    // 全局命令分组
    OptionGroup global_opts;
    // 输入和输出的命令分组 (groups[0] 存储与输出文件相关参数，groups[1] 存储与输入文件相关参数)
    OptionGroupList *groups;
    int           nb_groups;

    /* 临时数组，存储输出、输入相关参数 parsing state */
    OptionGroup cur_group;
} OptionParseContext;

/**
 * Parse an options group and write results into optctx.
 *
 * @param optctx an app-specific options context. NULL for global options group
 */
int parse_optgroup(void *optctx, OptionGroup *g);

/**
 * Split the commandline into an intermediate form convenient for further
 * processing.
 *
 * The commandline is assumed to be composed of options which either belong to a
 * group (those with OPT_SPEC, OPT_OFFSET or OPT_PERFILE) or are global
 * (everything else).
 *
 * A group (defined by an OptionGroupDef struct) is a sequence of options
 * terminated by either a group separator option (e.g. -i) or a parameter that
 * is not an option (doesn't start with -). A group without a separator option
 * must always be first in the supplied groups list.
 *
 * All options within the same group are stored in one OptionGroup struct in an
 * OptionGroupList, all groups with the same group definition are stored in one
 * OptionGroupList in OptionParseContext.groups. The order of group lists is the
 * same as the order of group definitions.
 */
int split_commandline(OptionParseContext *octx, int argc, char *argv[],
                      const OptionDef *options,
                      const OptionGroupDef *groups, int nb_groups);

/**
 * Free all allocated memory in an OptionParseContext.
 */
void uninit_parse_context(OptionParseContext *octx);

/**
 * Find the '-loglevel' option in the command line args and apply it.
 */
void parse_loglevel(int argc, char **argv, const OptionDef *options);

/**
 * Return index of option opt in argv or 0 if not found.
 */
int locate_option(int argc, char **argv, const OptionDef *options,
                  const char *optname);

/**
 * Check if the given stream matches a stream specifier.
 *
 * @param s  Corresponding format context.
 * @param st Stream from s to be checked.
 * @param spec A stream specifier of the [v|a|s|d]:[\<stream index\>] form.
 *
 * @return 1 if the stream matches, 0 if it doesn't, <0 on error
 */
int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec);

/**
 * Filter out options for given codec.
 *
 * Create a new options dictionary containing only the options from
 * opts which apply to the codec with ID codec_id.
 *
 * @param opts     dictionary to place options in
 * @param codec_id ID of the codec that should be filtered for
 * @param s Corresponding format context.
 * @param st A stream from s for which the options should be filtered.
 * @param codec The particular codec for which the options should be filtered.
 *              If null, the default one is looked up according to the codec id.
 * @return a pointer to the created dictionary
 */
AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
                                AVFormatContext *s, AVStream *st, const AVCodec *codec);

/**
 * Setup AVCodecContext options for avformat_find_stream_info().
 *
 * Create an array of dictionaries, one dictionary for each stream
 * contained in s.
 * Each dictionary will contain the options from codec_opts which can
 * be applied to the corresponding stream codec context.
 *
 * @return pointer to the created array of dictionaries, NULL if it
 * cannot be created
 */
AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,
                                           AVDictionary *codec_opts);

/**
 * Print an error message to stderr, indicating filename and a human
 * readable description of the error code err.
 *
 * If strerror_r() is not available the use of this function in a
 * multithreaded application may be unsafe.
 *
 * @see av_strerror()
 */
void print_error(const char *filename, int err);

/**
 * Print the program banner to stderr. The banner contents depend on the
 * current version of the repository and of the libav* libraries used by
 * the program.
 */
void show_banner(int argc, char **argv, const OptionDef *options);

/**
 * Print the version of the program to stdout. The version message
 * depends on the current versions of the repository and of the libav*
 * libraries.
 * This option processing function does not utilize the arguments.
 */
int show_version(void *optctx, const char *opt, const char *arg);

/**
 * Print the build configuration of the program to stdout. The contents
 * depend on the definition of FFMPEG_CONFIGURATION.
 * This option processing function does not utilize the arguments.
 */
int show_buildconf(void *optctx, const char *opt, const char *arg);

/**
 * Print the license of the program to stdout. The license depends on
 * the license of the libraries compiled into the program.
 * This option processing function does not utilize the arguments.
 */
int show_license(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the formats supported by the
 * program (including devices).
 * This option processing function does not utilize the arguments.
 */
int show_formats(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the muxers supported by the
 * program (including devices).
 * This option processing function does not utilize the arguments.
 */
int show_muxers(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the demuxer supported by the
 * program (including devices).
 * This option processing function does not utilize the arguments.
 */
int show_demuxers(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the devices supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_devices(void *optctx, const char *opt, const char *arg);

#if CONFIG_AVDEVICE
/**
 * Print a listing containing autodetected sinks of the output device.
 * Device name with options may be passed as an argument to limit results.
 */
int show_sinks(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing autodetected sources of the input device.
 * Device name with options may be passed as an argument to limit results.
 */
int show_sources(void *optctx, const char *opt, const char *arg);
#endif

/**
 * Print a listing containing all the codecs supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_codecs(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the decoders supported by the
 * program.
 */
int show_decoders(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the encoders supported by the
 * program.
 */
int show_encoders(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the filters supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_filters(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the bit stream filters supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_bsfs(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the protocols supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_protocols(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the pixel formats supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_pix_fmts(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the standard channel layouts supported by
 * the program.
 * This option processing function does not utilize the arguments.
 */
int show_layouts(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the sample formats supported by the
 * program.
 */
int show_sample_fmts(void *optctx, const char *opt, const char *arg);

/**
 * Print a listing containing all the color names and values recognized
 * by the program.
 */
int show_colors(void *optctx, const char *opt, const char *arg);

/**
 * Return a positive value if a line read from standard input
 * starts with [yY], otherwise return 0.
 */
int read_yesno(void);

/**
 * Get a file corresponding to a preset file.
 *
 * If is_path is non-zero, look for the file in the path preset_name.
 * Otherwise search for a file named arg.ffpreset in the directories
 * $FFMPEG_DATADIR (if set), $HOME/.ffmpeg, and in the datadir defined
 * at configuration time or in a "ffpresets" folder along the executable
 * on win32, in that order. If no such file is found and
 * codec_name is defined, then search for a file named
 * codec_name-preset_name.avpreset in the above-mentioned directories.
 *
 * @param filename buffer where the name of the found filename is written
 * @param filename_size size in bytes of the filename buffer
 * @param preset_name name of the preset to search
 * @param is_path tell if preset_name is a filename path
 * @param codec_name name of the codec for which to look for the
 * preset, may be NULL
 */
FILE *get_preset_file(char *filename, size_t filename_size,
                      const char *preset_name, int is_path, const char *codec_name);

/**
 * Realloc array to hold new_size elements of elem_size.
 * Calls exit() on failure.
 *
 * @param array array to reallocate
 * @param elem_size size in bytes of each element
 * @param size new element count will be written here
 * @param new_size number of elements to place in reallocated array
 * @return reallocated array
 */
void *grow_array(void *array, int elem_size, int *size, int new_size);

#define media_type_string av_get_media_type_string

#define GROW_ARRAY(array, nb_elems)\
    array = grow_array(array, sizeof(*array), &nb_elems, nb_elems + 1)

#define GET_PIX_FMT_NAME(pix_fmt)\
    const char *name = av_get_pix_fmt_name(pix_fmt);

#define GET_CODEC_NAME(id)\
    const char *name = avcodec_descriptor_get(id)->name;

#define GET_SAMPLE_FMT_NAME(sample_fmt)\
    const char *name = av_get_sample_fmt_name(sample_fmt)

#define GET_SAMPLE_RATE_NAME(rate)\
    char name[16];\
    snprintf(name, sizeof(name), "%d", rate);

double get_rotation(AVStream *st);

#endif /* FFTOOLS_CMDUTILS_H */
