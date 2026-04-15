cmake_minimum_required(VERSION 3.23)

project(VoiceAVLearning VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    VOICE_AV_USE_RELEASE_CONAN_DEPS
    "Use Release Conan packages when building non-Release Visual Studio configs"
    ON
)

if(VOICE_AV_USE_RELEASE_CONAN_DEPS)
    set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
    set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
endif()

list(PREPEND CMAKE_PREFIX_PATH
    "${CMAKE_BINARY_DIR}/generators"
    "${CMAKE_SOURCE_DIR}/build/generators"
)

find_package(ffmpeg REQUIRED CONFIG)

function(configure_voice_av_target target_name)
    if(MSVC)
        set_property(TARGET ${target_name} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
        )
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    if(TARGET ffmpeg::ffmpeg)
        target_link_libraries(${target_name} PRIVATE ffmpeg::ffmpeg)
    else()
        set(_ffmpeg_components
            ffmpeg::avdevice
            ffmpeg::avfilter
            ffmpeg::avformat
            ffmpeg::avcodec
            ffmpeg::swresample
            ffmpeg::swscale
            ffmpeg::avutil
            ffmpeg::postproc
        )

        foreach(_component IN LISTS _ffmpeg_components)
            if(TARGET ${_component})
                target_link_libraries(${target_name} PRIVATE ${_component})
            endif()
        endforeach()

        if(NOT TARGET ffmpeg::avformat)
            message(FATAL_ERROR "Conan generated ffmpeg package, but ffmpeg::avformat target was not found")
        endif()
    endif()
endfunction()

add_executable(voice_av_learning
    lessons/00-ffmpeg-version/main.cpp
)
configure_voice_av_target(voice_av_learning)

add_subdirectory(lessons/08-ffmpeg-demuxing-and-decoding-practice)
