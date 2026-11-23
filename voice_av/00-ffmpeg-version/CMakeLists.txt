cmake_minimum_required(VERSION 3.22)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(voice_av_learning LANGUAGES C CXX)
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/VoiceAVExamples.cmake")
endif()

voice_av_add_example_executable(voice_av_learning main.cpp)
