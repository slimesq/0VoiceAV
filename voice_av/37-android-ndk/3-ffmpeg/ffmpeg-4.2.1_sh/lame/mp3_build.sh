#!/bin/bash

NDK=/home/lqf/ndk/android-ndk-r22
HOST_TAG=linux-x86_64
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/$HOST_TAG

ANDROID_LIB_PATH="$(pwd)/android"
API=21

function build_android
{
echo "build for android $CPU"
./configure \
--host=$HOST \
--disable-shared \
--enable-static \
--prefix="$ANDROID_LIB_PATH/$CPU" 
make clean
make -j8
make install
echo "building for android $CPU completed"
}

CPU=armv8-a
HOST=aarch64-linux-android
export AR=$TOOLCHAIN/bin/aarch64-linux-android-ar
export AS=$TOOLCHAIN/bin/aarch64-linux-android-as
export LD=$TOOLCHAIN/bin/aarch64-linux-android-ld
export RANLIB=$TOOLCHAIN/bin/aarch64-linux-android-ranlib
export STRIP=$TOOLCHAIN/bin/aarch64-linux-android-strip
export CC=$TOOLCHAIN/bin/aarch64-linux-android$API-clang
export CXX=$TOOLCHAIN/bin/aarch64-linux-android$API-clang++
OPTIMIZE_CFLAGS="-march=$CPU"
export CFLAGS="-Os -fpic $OPTIMIZE_CFLAGS"
export CPPFLAGS="-Os -fpic $OPTIMIZE_CFLAGS"
build_android


#armv7-a
CPU=armv7-a
HOST=arm-linux-android
export AR=$TOOLCHAIN/bin/arm-linux-androideabi-ar
export AS=$TOOLCHAIN/bin/arm-linux-androideabi-as
export LD=$TOOLCHAIN/bin/arm-linux-androideabi-ld
export RANLIB=$TOOLCHAIN/bin/arm-linux-androideabi-ranlib
export STRIP=$TOOLCHAIN/bin/arm-linux-androideabi-strip
export CC=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang
export CXX=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang++
OPTIMIZE_CFLAGS="-mfloat-abi=softfp -mfpu=vfp -marm"
export CFLAGS="-Os -fpic $OPTIMIZE_CFLAGS"
export CPPFLAGS="-Os -fpic $OPTIMIZE_CFLAGS"
build_android

#x86
CPU=x86
HOST=i686-linux-android
export AR=$TOOLCHAIN/bin/i686-linux-android-ar
export AS=$TOOLCHAIN/bin/i686-linux-android-as
export LD=$TOOLCHAIN/bin/i686-linux-android-ld
export RANLIB=$TOOLCHAIN/bin/i686-linux-android-ranlib
export STRIP=$TOOLCHAIN/bin/i686-linux-android-strip
export CC=$TOOLCHAIN/bin/i686-linux-android$API-clang
export CXX=$TOOLCHAIN/bin/i686-linux-android$API-clang++
OPTIMIZE_CFLAGS="-mtune=intel -mssse3 -mfpmath=sse -m32"
export CFLAGS="-Os -fpic $OPTIMIZE_CFLAGS"
export CPPFLAGS="-Os -fpic $OPTIMIZE_CFLAGS"
build_android

#x86_64
CPU=x86_64
HOST=x86_64-linux-android
export AR=$TOOLCHAIN/bin/x86_64-linux-android-ar
export AS=$TOOLCHAIN/bin/x86_64-linux-android-as
export LD=$TOOLCHAIN/bin/x86_64-linux-android-ld
export RANLIB=$TOOLCHAIN/bin/x86_64-linux-android-ranlib
export STRIP=$TOOLCHAIN/bin/x86_64-linux-android-strip
export CC=$TOOLCHAIN/bin/x86_64-linux-android$API-clang
export CXX=$TOOLCHAIN/bin/x86_64-linux-android$API-clang++
OPTIMIZE_CFLAGS="-msse4.2 -mpopcnt -m64 -mtune=intel"
export CFLAGS="-Os -fpic $OPTIMIZE_CFLAGS"
export CPPFLAGS="-Os -fpic $OPTIMIZE_CFLAGS"
build_android
