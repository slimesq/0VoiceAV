#include <jni.h>
#include <string>

extern "C" {
#include "include/libavutil/avutil.h"
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_darren_ffmpeg_14_11_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++ FFmpeg version: ";
    hello += av_version_info();
    return env->NewStringUTF(hello.c_str());
}
