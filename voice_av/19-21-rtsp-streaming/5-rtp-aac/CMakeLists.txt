cmake_minimum_required(VERSION 3.22)
project(rtp_aac LANGUAGES C)

add_executable(rtp_aac
    src/main.c
    src/rtp-packet.c
    src/rtp-payload.c
    src/rtp-h264-pack.c
    src/rtp-h264-unpack.c
    src/h264-util.c
    src/aac-util.c
    src/rtp-payload-helper.c
    src/rtp-mpeg4-generic-pack.c
    src/rtp-mpeg4-generic-unpack.c
)
target_include_directories(rtp_aac PRIVATE inc)
target_compile_features(rtp_aac PRIVATE c_std_11)

if(WIN32)
    target_compile_definitions(rtp_aac PRIVATE WIN32)
    target_link_libraries(rtp_aac PRIVATE ws2_32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(rtp_aac PRIVATE linux _DEFAULT_SOURCE)
endif()

if(MSVC)
    target_compile_definitions(rtp_aac PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(rtp_aac PRIVATE /utf-8)
endif()

