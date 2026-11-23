cmake_minimum_required(VERSION 3.22)
project(rtp_h264 LANGUAGES C)

add_executable(rtp_h264
    src/main.c
    src/rtp-packet.c
    src/rtp-payload.c
    src/rtp-h264-pack.c
    src/rtp-h264-unpack.c
    src/h264-util.c
)
target_include_directories(rtp_h264 PRIVATE inc)
target_compile_features(rtp_h264 PRIVATE c_std_11)

if(WIN32)
    target_compile_definitions(rtp_h264 PRIVATE WIN32)
    target_link_libraries(rtp_h264 PRIVATE ws2_32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(rtp_h264 PRIVATE linux _DEFAULT_SOURCE)
endif()

if(MSVC)
    target_compile_definitions(rtp_h264 PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(rtp_h264 PRIVATE /utf-8)
endif()

