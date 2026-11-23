#!/bin/bash
NDK=/home/lqf/ndk/android-ndk-r22
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64/
API=21
BASEPATH=$(cd `dirname $0`; pwd)

function build_android
{
echo "Compiling FFmpeg for $CPU"
echo "Compiling FFmpeg link $FDK_LIB"
echo "Compiling FFmpeg link $X264_LIB"
echo "Compiling FFmpeg link $MP3_LIB"
./configure \
    --prefix=$PREFIX \
    --disable-neon \
    --disable-hwaccels \
    --enable-shared \
    --enable-jni \
    --disable-mediacodec \
    --disable-decoder=h264_mediacodec \
    --disable-static \
    --disable-doc \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-doc \
    --disable-symver \
    --extra-cflags="-I$X264_INCLUDE  -I$FDK_INCLUDE -I$MP3_INCLUDE" \
    --extra-ldflags="-L$X264_LIB -L$MP3_LIB -L$FDK_LIB" \
    --enable-nonfree \
    --enable-gpl \
    --enable-libfdk-aac \
    --enable-libx264 \
    --enable-libmp3lame \
    --cross-prefix=$CROSS_PREFIX \
    --target-os=android \
    --arch=$ARCH \
    --cpu=$CPU \
    --cc=$CC
    --cxx=$CXX
    --sysroot=$SYSROOT \
    --extra-cflags="-Os -fpic $OPTIMIZE_CFLAGS" \
    --extra-ldflags="$ADDI_LDFLAGS" \
    $ADDITIONAL_CONFIGURE_FLAG
make clean
make
make install
echo "The Compilation of FFmpeg for $CPU is completed"
}

#armv8-a
ARCH=arm64
CPU=armv8-a
CC=$TOOLCHAIN/bin/aarch64-linux-android$API-clang
CXX=$TOOLCHAIN/bin/aarch64-linux-android$API-clang++
SYSROOT=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/aarch64-linux-android-
PREFIX=$(pwd)/android/$CPU
OPTIMIZE_CFLAGS="-march=$CPU"
#第三方库
FDK_INCLUDE=$BASEPATH/fdk-aac/android/$CPU/include
FDK_LIB=$BASEPATH/fdk-aac/android/$CPU/lib
X264_INCLUDE=$BASEPATH/x264/android/$CPU/include
X264_LIB=$BASEPATH/x264/android/$CPU/lib
MP3_INCLUDE=$BASEPATH/lame/android/$CPU/include
MP3_LIB=$BASEPATH/lame/android/$CPU/lib
build_android

#armv7-a
ARCH=arm
CPU=armv7-a
CC=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang
CXX=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang++
SYSROOT=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/arm-linux-androideabi-
PREFIX=$(pwd)/android/$CPU
OPTIMIZE_CFLAGS="-mfloat-abi=softfp -mfpu=vfp -marm -march=$CPU "
#第三方库
FDK_INCLUDE=$BASEPATH/fdk-aac/android/$CPU/include
FDK_LIB=$BASEPATH/fdk-aac/android/$CPU/lib
X264_INCLUDE=$BASEPATH/x264/android/$CPU/include
X264_LIB=$BASEPATH/x264/android/$CPU/lib
MP3_INCLUDE=$BASEPATH/lame/android/$CPU/include
MP3_LIB=$BASEPATH/lame/android/$CPU/lib
build_android



