cmake_minimum_required(VERSION 3.22)

project(VoiceAVCourse LANGUAGES C CXX)

get_filename_component(VOICE_AV_COURSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(VOICE_AV_COURSE_SOURCES "${VOICE_AV_COURSE_ROOT}/voice_av")
set(VOICE_AV_COURSE_TARGETS "" CACHE STRING "Course target names or prefixes ending in *")
set(VOICE_AV_COURSE_FFMPEG_INTERNAL_ROOT "${VOICE_AV_COURSE_ROOT}/third_party/ffmpeg-src"
    CACHE PATH "Matching FFmpeg 7.1.3 source headers for the command-line courses")
set(VOICE_AV_COURSE_SDL2_ROOT "" CACHE PATH "Install root of the course SDL2 SDK")
set(VOICE_AV_COURSE_QT_ROOT "" CACHE PATH "Install prefix of the x64 Qt SDK")

if(WIN32 AND NOT VOICE_AV_COURSE_SDL2_ROOT)
    set(_sdl_sdk "${VOICE_AV_COURSE_ROOT}/build/course-deps/SDL2-2.0.10")
    if(EXISTS "${_sdl_sdk}/include/SDL.h")
        set(VOICE_AV_COURSE_SDL2_ROOT "${_sdl_sdk}")
    endif()
endif()
if(VOICE_AV_COURSE_QT_ROOT)
    cmake_path(ABSOLUTE_PATH VOICE_AV_COURSE_QT_ROOT NORMALIZE OUTPUT_VARIABLE _qt_prefix)
    if(NOT EXISTS "${_qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
        message(FATAL_ERROR "VOICE_AV_COURSE_QT_ROOT must point to an x64 Qt 6 installation.")
    endif()
    get_cmake_property(_cache_entries CACHE_VARIABLES)
    foreach(_entry IN LISTS _cache_entries)
        if(_entry MATCHES "^Qt6.*_DIR$")
            get_property(_qt_package_dir CACHE "${_entry}" PROPERTY VALUE)
            cmake_path(IS_PREFIX _qt_prefix "${_qt_package_dir}" NORMALIZE _inside_qt_prefix)
            if(NOT _inside_qt_prefix)
                unset(${_entry} CACHE)
            endif()
        endif()
    endforeach()
    set(Qt6_DIR "${_qt_prefix}/lib/cmake/Qt6")
    list(PREPEND CMAKE_PREFIX_PATH "${_qt_prefix}")
elseif(WIN32)
    find_program(_qt_program NAMES qmake6 qmake NO_CACHE)
    if(_qt_program)
        execute_process(COMMAND "${_qt_program}" -query QT_INSTALL_PREFIX
            OUTPUT_VARIABLE _qt_prefix OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(EXISTS "${_qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
            list(APPEND CMAKE_PREFIX_PATH "${_qt_prefix}")
        endif()
    endif()
endif()

include(CourseTargets.cmake)

# Chapters 02, 06–08: development, SDL rendering, and FFmpeg fundamentals.
set(_environment "02-development-environment")
set(_sdl "06-sdl-rendering")
set(_basics "07-ffmpeg-basics")
set(_decode "08-ffmpeg-demuxing-and-decoding")

voice_av_course_add(voice_av_02_demo_c "${_environment}/mac_demo_c"
    SOURCES main.c)
voice_av_course_add(voice_av_02_demo_cxx "${_environment}/mac_demo_cplus"
    SOURCES main.cpp)

foreach(_platform IN ITEMS linux mac)
    voice_av_course_add(voice_av_02_${_platform}_ffmpeg "${_environment}/${_platform}_1-ffmpeg"
        SOURCES main.c DEPENDENCIES FFMPEG)
    voice_av_course_add(voice_av_02_${_platform}_extract_aac
        "${_environment}/${_platform}_3-ffmpeg-extract-aac"
        SOURCES main.c DEPENDENCIES FFMPEG)
endforeach()

voice_av_course_add(voice_av_02_linux_sdl_basic "${_environment}/linux_2-sdl-basic"
    SOURCES main.c DEPENDENCIES SDL2)
voice_av_course_add(voice_av_02_linux_sdl_yuv "${_environment}/linux_2-sdl-yuv"
    SOURCES main.c DEPENDENCIES SDL2)
voice_av_course_add(voice_av_02_mac_sdl_yuv "${_environment}/mac_2-sdl"
    SOURCES main.c DEPENDENCIES SDL2)

foreach(_sample IN ITEMS 01-sdl-basic 02-sdl-window 03-sdl-event 04-sdl-thread 05-sdl-yuv 06-sdl-pcm)
    string(REPLACE "-" "_" _id "${_sample}")
    voice_av_course_add(voice_av_06_${_id} "${_sdl}/${_sample}"
        SOURCES main.c DEPENDENCIES SDL2)
endforeach()

voice_av_course_add(voice_av_07_buffer "${_basics}"
    SOURCES src/main.c src/avpacket.c src/avframe.c INCLUDE_DIRS inc DEPENDENCIES FFMPEG
    DEFINITIONS "$<$<C_COMPILER_ID:MSVC>:memccpy=_memccpy>"
        "$<$<PLATFORM_ID:Linux>:_DEFAULT_SOURCE>")

# The chapter 08–10 lessons already in the main CMake project build there.
voice_av_course_add(voice_av_08_camera_dshow "${_decode}/camera_sdl2_dshow"
    PLATFORMS Windows SOURCES main.cpp DEPENDENCIES FFMPEG SDL2)

# Ch. 11: the four Qt Widgets snapshots share the window classes and resources.
set(qt_player_sources
    inc/about.h
    inc/CustomSlider.h
    inc/ctrlbar.h
    inc/customthread.h
    inc/globalhelper.h
    inc/mainwid.h
    inc/medialist.h
    inc/playlist.h
    inc/settingwid.h
    inc/show.h
    inc/sonic.h
    inc/title.h
    src/main.cpp
    src/about.cpp
    src/CustomSlider.cpp
    src/customthread.cpp
    src/globalhelper.cpp
    src/settingwid.cpp
    src/ctrlbar.cpp
    src/mainwid.cpp
    src/medialist.cpp
    src/playlist.cpp
    src/show.cpp
    src/title.cpp
    src/sonic.cpp
    src/mainwid.ui
    src/ctrlbar.ui
    src/show.ui
    src/playlist.ui
    src/title.ui
    src/about.ui
    src/settingwid.ui
    src/mainwid.qrc
)
foreach(snapshot IN ITEMS 01 02 03 04)
    set(snapshot_sources ${qt_player_sources})
    if(snapshot STREQUAL "01" OR snapshot STREQUAL "02")
        list(APPEND snapshot_sources inc/videoctl.h src/videoctl.cpp)
    endif()
    if(snapshot STREQUAL "01")
        list(APPEND snapshot_sources inc/datactl.h)
    else()
        list(APPEND snapshot_sources
            inc/config.h inc/ff_fferror.h inc/ff_ffinc.h inc/ff_ffmsg.h inc/ff_ffmsg_queue.h
            inc/ff_ffplay.h inc/ff_ffplay_def.h inc/ijkplayer.h inc/ijkplayer_qt.h
            src/ijkplayer_qt.cpp src/ijkplayer.c src/ff_ffplay.c)
    endif()
    if(snapshot STREQUAL "03" OR snapshot STREQUAL "04")
        list(APPEND snapshot_sources inc/ijkplayer_internal.h inc/ijksdl_misc.h)
    endif()
    if(snapshot STREQUAL "04")
        list(APPEND snapshot_sources inc/slog.h src/slog.c)
    endif()
    voice_av_course_add_qt(
        voice_av_11_qt_player_${snapshot}
        "11-ffmpeg-qt-player/0voice_player_${snapshot}"
        SOURCES ${snapshot_sources}
        DEPENDENCIES FFMPEG SDL2 QT_WIDGETS THREADS
        INCLUDE_DIRS inc
        DEFINITIONS SDL_MAIN_HANDLED
    )
endforeach()

# Ch. 12–13: the synthetic clock examples provide main() directly. The
# ffplay snapshots use SDL's entry point and the FFmpeg 7 public API.
foreach(snapshot IN ITEMS 04 11)
    voice_av_course_add(
        voice_av_12_clock_${snapshot}
        "12-13-ffplay-player/${snapshot}-clock-test"
        SOURCES main.c
        DEPENDENCIES FFMPEG SDL2
        PLATFORMS Windows
        LIBRARIES "$<$<PLATFORM_ID:Windows>:winmm>"
    )
endforeach()
foreach(snapshot IN ITEMS 1-3 4 5 6 7-8 9-10 11-14 15-18)
    string(REPLACE "-" "_" suffix "${snapshot}")
    voice_av_course_add(
        voice_av_12_ffplay_${suffix}
        "12-13-ffplay-player/ffplay-pro-${snapshot}"
        SOURCES src/cmdutils.c src/ffplay.c inc/cmdutils.h inc/config.h
            ../ffplay_config.h ../ffplay_ffmpeg71.h
        DEPENDENCIES FFMPEG SDL2_MAIN
        INCLUDE_DIRS inc
        PLATFORMS Windows
        COMPILER_IDS MSVC
        LIBRARIES "$<$<PLATFORM_ID:Windows>:shell32>"
    )
endforeach()

# Ch. 14: the lesson CLI uses version-matched FFmpeg private headers for
# internal stream state and Windows atomic/thread compatibility.
foreach(snapshot IN ITEMS 03 04 07 08 09 10)
    voice_av_course_add(
        voice_av_14_ffmpeg_pro_${snapshot}
        "14-ffmpeg-transcoder/ffmpeg-pro-${snapshot}"
        SOURCES src/cmdutils.c src/ffmpeg.c src/ffmpeg_opt.c
            src/ffmpeg_filter.c src/ffmpeg_hw.c src/ffmpeg_cuvid.c
            inc/cmdutils.h inc/config.h inc/ffmpeg.h
        DEPENDENCIES FFMPEG_INTERNAL THREADS
        INCLUDE_DIRS inc "${VOICE_AV_COURSE_FFMPEG_INTERNAL_ROOT}/compat/atomics/win32"
        PLATFORMS Windows
        COMPILER_IDS MSVC
        DEFINITIONS WIN32_LEAN_AND_MEAN
        OPTIONS /FIdirect.h
        LIBRARIES shell32 ws2_32 psapi
    )
endforeach()

# Ch. 15: the expanded librtmp source in each snapshot is the version used
# by the course project. The publisher builds only the main_publish entry.
set(course_rtmp_sources
    librtmp/amf.c
    librtmp/hashswf.c
    librtmp/log.c
    librtmp/parseurl.c
    librtmp/rtmp.c
)
voice_av_course_add(
    voice_av_15_rtmp_publish
    "15-rtmp-streaming/rtmp-publish-20210903"
    SOURCES src/main_publish.cpp ${course_rtmp_sources}
        src/rtmp_publish.cpp src/aacencoder.cpp src/audioresample.cpp src/dlog.cpp
        src/rtmppusher.cpp src/looper.cpp src/h264encoder.cpp src/rtmpbase.cpp
        src/naluloop.cpp src/pushwork.cpp src/audiocapturer.cpp src/commonlooper.cpp
        src/mediabase.cpp src/aacrtmppackager.cpp src/videocapturer.cpp src/videooutsdl.cpp
        src/avtimebase.cpp
    INCLUDE_DIRS inc
    DEPENDENCIES FFMPEG SDL2 THREADS
    PLATFORMS Windows
    DEFINITIONS "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:_HAS_STD_BYTE=0>"
    LIBRARIES "$<$<PLATFORM_ID:Windows>:ws2_32>"
)
voice_av_course_add_qt(
    voice_av_15_zero_player
    "15-rtmp-streaming/zero_player_20210903"
    SOURCES src/main.cpp src/mainwindow.cpp src/qpainterdrawable.cpp ${course_rtmp_sources}
        src/dlog.cpp src/looper.cpp src/rtmpbase.cpp src/naluloop.cpp src/commonlooper.cpp
        src/mediabase.cpp src/videooutsdl.cpp src/audiooutsdl.cpp src/avsync.cpp
        src/framequeue.cpp src/rtmpplayer.cpp src/aacdecoder.cpp src/h264decoder.cpp
        src/audiodecodeloop.cpp src/pullwork.cpp src/videooutputloop.cpp src/avtimebase.cpp
        src/packetqueue.cpp src/imagescale.cpp src/videodecodeloop.cpp src/sonic.cpp
        src/mainwindow.ui src/qpainterdrawable.ui inc/mainwindow.h inc/qpainterdrawable.h
    INCLUDE_DIRS inc
    DEPENDENCIES FFMPEG SDL2 QT_WIDGETS THREADS
    PLATFORMS Windows
    DEFINITIONS __STDC_CONSTANT_MACROS QT_DEPRECATED_WARNINGS
    LIBRARIES "$<$<PLATFORM_ID:Windows>:ws2_32>" "$<$<PLATFORM_ID:Windows>:winmm>"
)

# Chapter 19–21. The RTP demos use only platform sockets.
set(_rtsp_course_dir 19-21-rtsp-streaming)
set(_rtp_common_sources
    src/main.c src/rtp-packet.c src/rtp-payload.c src/rtp-h264-pack.c
    src/rtp-h264-unpack.c src/h264-util.c
)
set(_course_socket_library "$<$<PLATFORM_ID:Windows>:ws2_32>")
set(_rtp_platform_definitions
    "$<$<PLATFORM_ID:Windows>:WIN32>"
    "$<$<PLATFORM_ID:Linux>:linux>"
    "$<$<PLATFORM_ID:Linux>:_DEFAULT_SOURCE>"
)

voice_av_course_add(voice_av_19_rtp_h264 "${_rtsp_course_dir}/4-rtp"
    SOURCES ${_rtp_common_sources}
    INCLUDE_DIRS inc
    PLATFORMS Windows Linux
    DEFINITIONS ${_rtp_platform_definitions}
    LIBRARIES "${_course_socket_library}"
)

voice_av_course_add(voice_av_19_rtp_aac "${_rtsp_course_dir}/5-rtp-aac"
    SOURCES ${_rtp_common_sources} src/aac-util.c src/rtp-payload-helper.c
        src/rtp-mpeg4-generic-pack.c src/rtp-mpeg4-generic-unpack.c
    INCLUDE_DIRS inc
    PLATFORMS Windows Linux
    DEFINITIONS ${_rtp_platform_definitions}
    LIBRARIES "${_course_socket_library}"
)

voice_av_course_add(voice_av_19_rtsp_publish_09_01 "${_rtsp_course_dir}/9-1-rtsp_publish"
    SOURCES src/main_publish.cpp src/aacencoder.cpp src/audioresample.cpp src/dlog.cpp
        src/looper.cpp src/h264encoder.cpp src/naluloop.cpp src/pushwork.cpp
        src/audiocapturer.cpp src/commonlooper.cpp src/mediabase.cpp src/videocapturer.cpp
        src/videooutsdl.cpp src/avsync.cpp src/avtimebase.cpp src/rtsppusher.cpp
    INCLUDE_DIRS inc
    DEPENDENCIES FFMPEG SDL2 THREADS
    # Course code brings std into global scope before including Windows headers.
    DEFINITIONS "$<$<CXX_COMPILER_ID:MSVC>:_HAS_STD_BYTE=0>"
    LIBRARIES "${_course_socket_library}"
)

set(_rtsp_stage_definitions "$<$<CXX_COMPILER_ID:MSVC>:strcasecmp=stricmp>")
function(voice_av_course_add_rtsp_stage stage target_suffix)
    set(_sources main.cpp commonlooper.cpp dlog.cpp audiocapturer.cpp pushwork.cpp ${ARGN})
    list(TRANSFORM _sources PREPEND "src/")
    voice_av_course_add("voice_av_19_rtsp_publish_09_${target_suffix}"
        "${_rtsp_course_dir}/9-${stage}-rtsp-publish"
        SOURCES ${_sources}
        INCLUDE_DIRS inc
        DEPENDENCIES FFMPEG THREADS
        DEFINITIONS ${_rtsp_stage_definitions}
        LIBRARIES "${_course_socket_library}"
    )
endfunction()

voice_av_course_add_rtsp_stage(2 2)
voice_av_course_add_rtsp_stage(3 3 videocapturer.cpp avpublishtime.cpp)

foreach(_version IN ITEMS fail ok)
    voice_av_course_add("voice_av_19_rtsp_publish_09_04_${_version}"
        "${_rtsp_course_dir}/9-4-rtsp-publish-${_version}"
        SOURCES src/main.cpp src/commonlooper.cpp src/dlog.cpp src/audiocapturer.cpp
            src/pushwork.cpp src/videocapturer.cpp src/avpublishtime.cpp src/aacencoder.cpp
        INCLUDE_DIRS inc
        DEPENDENCIES FFMPEG THREADS
        DEFINITIONS ${_rtsp_stage_definitions}
        LIBRARIES "${_course_socket_library}"
    )
endforeach()

set(_rtsp_encode_sources videocapturer.cpp avpublishtime.cpp aacencoder.cpp h264encoder.cpp)
foreach(_stage RANGE 5 6)
    voice_av_course_add_rtsp_stage(${_stage} ${_stage} ${_rtsp_encode_sources})
endforeach()
foreach(_stage RANGE 7 11)
    voice_av_course_add_rtsp_stage(${_stage} ${_stage} ${_rtsp_encode_sources} rtsppusher.cpp)
endforeach()

# Chapter 31–33. The Web clients are browser code. Each GLib C file is one lesson.
set(_janus_glib_dir 31-33-webrtc-janus/test-glib)
foreach(_example IN ITEMS g_async_queue g_hash_table g_list g_main_loop0 g_main_loop1)
    voice_av_course_add("voice_av_31_${_example}" "${_janus_glib_dir}"
        SOURCES "${_example}.c" DEPENDENCIES GLIB
    )
endforeach()
voice_av_course_add(voice_av_31_g_main_loop2 "${_janus_glib_dir}"
    SOURCES g_main_loop2.c DEPENDENCIES GLIB PLATFORMS Linux Darwin
)
foreach(_example IN ITEMS g_lock g_mutex_cond g_thread g_thread_pool)
    voice_av_course_add("voice_av_31_${_example}" "${_janus_glib_dir}"
        SOURCES "${_example}.c" DEPENDENCIES GLIB PLATFORMS Linux
        DEFINITIONS _DEFAULT_SOURCE
    )
endforeach()

# Chapter 37. Each GDB or Valgrind lesson has its own entry point.
set(_debug_course "37-gdb-cmake/src-gdb")
foreach(_lesson IN ITEMS core_dump jump next step1 watch)
    voice_av_course_add("voice_av_37_gdb_${_lesson}" "${_debug_course}/gdb"
        SOURCES "${_lesson}.c")
endforeach()
voice_av_course_add(voice_av_37_gdb_fork "${_debug_course}/gdb"
    SOURCES fork-1.c PLATFORMS Linux Darwin)
voice_av_course_add(voice_av_37_gdb_hello_server "${_debug_course}/gdb"
    SOURCES hello_server.c PLATFORMS Linux Darwin)
foreach(_lesson IN ITEMS badacc badlap badloop sample uninitial)
    voice_av_course_add("voice_av_37_valgrind_${_lesson}" "${_debug_course}/valgrind"
        SOURCES "${_lesson}.c" PLATFORMS Linux)
endforeach()
foreach(_lesson IN ITEMS badleak badleak2 badmac)
    voice_av_course_add("voice_av_37_valgrind_${_lesson}" "${_debug_course}/valgrind"
        SOURCES "${_lesson}.cpp" PLATFORMS Linux)
endforeach()

# The Makefile lessons with program sources also have independent CMake targets.
set(_make_course "37-gdb-cmake/src-cmake/Makefile")
file(GLOB _main_sources RELATIVE "${VOICE_AV_COURSE_SOURCES}/${_make_course}"
    "${VOICE_AV_COURSE_SOURCES}/${_make_course}/*/main.c")
foreach(_main_source IN LISTS _main_sources)
    get_filename_component(_lesson "${_main_source}" DIRECTORY)
    string(MAKE_C_IDENTIFIER "${_lesson}" _suffix)
    string(REGEX REPLACE "^_+" "" _suffix "${_suffix}")
    set(_dir "${_make_course}/${_lesson}")
    file(GLOB _sources RELATIVE "${VOICE_AV_COURSE_SOURCES}/${_dir}"
        "${VOICE_AV_COURSE_SOURCES}/${_dir}/*.c")
    voice_av_course_add("voice_av_37_make_${_suffix}" "${_dir}" SOURCES ${_sources})
endforeach()
voice_av_course_add(voice_av_37_make_huge "${_make_course}/huge"
    SOURCES source/huge/src/main.c source/bar/src/bar.c source/foo/src/foo.c
    INCLUDE_DIRS source/bar/inc source/foo/inc)

# Validate requested selectors and write the course target catalog.
get_property(_catalog GLOBAL PROPERTY VOICE_AV_COURSE_CATALOG)
get_property(_available GLOBAL PROPERTY VOICE_AV_COURSE_AVAILABLE)
get_property(_matched GLOBAL PROPERTY VOICE_AV_COURSE_MATCHED)
get_property(_selected GLOBAL PROPERTY VOICE_AV_COURSE_SELECTED)
foreach(_selector IN LISTS VOICE_AV_COURSE_TARGETS)
    if(NOT _selector IN_LIST _matched)
        message(FATAL_ERROR "No course target matches '${_selector}'")
    endif()
endforeach()
if(VOICE_AV_COURSE_TARGETS AND NOT _selected)
    message(FATAL_ERROR "No selected course targets support ${CMAKE_SYSTEM_NAME} with ${CMAKE_CXX_COMPILER_ID}")
endif()

set(_lines "Target | Platform/compiler availability\n")
list(SORT _catalog COMPARE NATURAL)
foreach(_name IN LISTS _catalog)
    if(_name IN_LIST _available)
        string(APPEND _lines "${_name} | available\n")
    else()
        string(APPEND _lines "${_name} | requires another toolchain\n")
    endif()
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/course-targets.txt" "${_lines}")
message(STATUS "Course target catalog: ${CMAKE_BINARY_DIR}/course-targets.txt")
if(NOT VOICE_AV_COURSE_TARGETS)
    message(STATUS "Set VOICE_AV_COURSE_TARGETS to configure selected examples")
endif()
