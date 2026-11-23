CMAKE_MINIMUM_REQUIRED(VERSION 2.8.4)
set(PROJECT_NAME 07-01-ffmpeg-demux)

project(${PROJECT_NAME})

set(SRC_LIST main.c)

# C++11
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")  
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_FLAGS "-static ${CMAKE_CXX_FLAGS}")

include_directories("/home/lqf/ffmpeg_build/include")

# 设定执行文件生成路径
add_executable(${PROJECT_NAME} ${SRC_LIST})
# 注意自己的路径/home/lqf/ffmpeg_build
link_directories("/home/lqf/ffmpeg_build/lib")
link_directories("/usr/local/lib")
link_directories("/usr/lib")
link_directories("/usr/lib/x86_64-linux-gnu/")

#libavformat
target_link_libraries(${PROJECT_NAME}  "z" "m" )
#ibavfilter
target_link_libraries(${PROJECT_NAME}  "pthread" "m" "va" "freetype" "z" "png12" "z" "m" )
#libswresample
target_link_libraries(${PROJECT_NAME}  "m")
#libavutil
target_link_libraries(${PROJECT_NAME}  pthread  m va-drm va va-x11 va vdpau X11 m va Xv X11 Xext)
#libswscale
target_link_libraries(${PROJECT_NAME}  "m" )
#libx264
#target_link_libraries(${PROJECT_NAME}  pthread "m" dl)
#libx265
#target_link_libraries(${PROJECT_NAME}  "stdc++" "m" "rt" "dl" "numa" )
# 注意自己的路径/home/lqf/ffmpeg_build
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libvpx.a)
target_link_libraries(${PROJECT_NAME} pthread  m  /usr/lib/x86_64-linux-gnu/libdl.so)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libx264.a)
target_link_libraries(${PROJECT_NAME} /usr/lib/gcc/x86_64-linux-gnu/5/libstdc++.so m rt dl /usr/lib/x86_64-linux-gnu/libnuma.so)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libx265.a)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libopus.a)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libfdk-aac.a)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libmp3lame.a)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libavformat.a)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libavutil.a)
#libavcodec
target_link_libraries(${PROJECT_NAME} vorbisenc vorbis  pthread m z pthread m stdc++ rt numa va)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libavcodec.a)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libswresample.a)
target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libavutil.a)
#target_link_libraries(${PROJECT_NAME} /home/lqf/ffmpeg_build/lib/libpostproc.a)




