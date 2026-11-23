cmake_minimum_required(VERSION 3.22)

# The repository's main build uses the FFmpeg 7 examples.
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    return()
endif()

project(voice_av_07_buffer LANGUAGES C CXX)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/course/Standalone.cmake")
