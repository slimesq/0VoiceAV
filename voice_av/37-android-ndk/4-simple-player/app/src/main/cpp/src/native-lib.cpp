#include <jni.h>
#include <string>
#include <android/native_window_jni.h>
#include "FFmpegPlayer.h"
#include "JavaCallHelper.h"

// 因为FFmpeg是C编写的，需要使用extern关键字进行C和C++混合编译
extern "C" {
#include "libavcodec/avcodec.h"
#include "macro.h"
}

ANativeWindow *pWindow;
static jclass globalVideoSurfaceClass = NULL;
static jobject globalVideoSurfaceObject = NULL;
FFmpegPlayer *pFFmpeg;
JavaCallHelper *p_java_call_helper;
JavaVM *javaVM;// 子线程需要通过JavaVM，才能获取到当前线程的env，然后才能反射

/**
 * 渲染一帧视频数据
 * @param data 输入源数据
 * @param linesize
 * @param w 宽
 * @param h 高
 */
ANativeWindow_Buffer window_buffer;
void render_frame(uint8_t *data, int linesize, int w, int h) {
//    LOG_D("render_frame");
    // 设置缓冲区的属性（宽、高、像素格式），像素格式要和SurfaceView的像素格式一致
    ANativeWindow_setBuffersGeometry(pWindow, w, h, WINDOW_FORMAT_RGBA_8888);

    // 绘制时的缓冲区，window的缓冲区
    if (ANativeWindow_lock(pWindow, &window_buffer, nullptr) < 0) {
        ANativeWindow_release(pWindow);
        pWindow = nullptr;
        return;
    }

    auto *window_data = static_cast<uint8_t *>(window_buffer.bits);
    // 计算一行有多少字节，stride是一行像素，需要乘以4，因为RGBA占4位
    int window_linesize = window_buffer.stride * 4;
    // 输入源数据（RGBA）
    uint8_t *src_data = data;
    for (int i = 0; i < window_buffer.height; ++i) {
        // 内存拷贝，一行一行的进行渲染效率高
        // 整块渲染虽然更快，但窗体大小和实际大小不一致，会出现错位
        memcpy(window_data + i * window_linesize, src_data + i * linesize,
               static_cast<size_t>(window_linesize));
    }
    if(ANativeWindow_unlockAndPost(pWindow) != 0) {
        LOG_E("ANativeWindow_unlockAndPost error");
        return;
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOG_D("JNI_OnLoad");
    javaVM = vm;// 获取JavaVM
    return JNI_VERSION_1_4;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_darren_ffmpeg_14_12_1player_MediaPlayer_native_1prepare(JNIEnv *env, jobject instance, jstring path_) {

    LOG_D("prepare");
    const char *path = env->GetStringUTFChars(path_, nullptr);

    p_java_call_helper = new JavaCallHelper(javaVM, env, instance); // 回调Java层MediaPlayer的函数

    pFFmpeg = new FFmpegPlayer(p_java_call_helper, path);   // jni播放核心
    pFFmpeg->setRenderCallback(render_frame);               // 设置渲染
    pFFmpeg->prepare();
    LOG_D("prepare leave");
    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_darren_ffmpeg_14_12_1player_MediaPlayer_native_1start(JNIEnv *env, jobject instance) {
    LOG_D("start");
    if (pFFmpeg != nullptr) {
        LOG_D("start play");
        pFFmpeg->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_darren_ffmpeg_14_12_1player_MediaPlayer_native_1stop(JNIEnv *env, jobject instance) {
    LOG_D("stop");
    if (pFFmpeg != nullptr) {
        pFFmpeg->stop();
        DELETE(pFFmpeg);
        DELETE(p_java_call_helper);
        LOG_D("stop ok");
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_darren_ffmpeg_14_12_1player_MediaPlayer_native_1set_1surface(JNIEnv *env, jobject obj,
                                                    jobject surface) {
    LOG_D("set_surface");
    if (pWindow != nullptr) {
        ANativeWindow_release(pWindow);
    }

    //LOGV("fun env is %p", env);
    jclass localVideoSurfaceClass = env->FindClass(
            "com/darren/ffmpeg_4_2_player/MediaPlayer");
    if (NULL == localVideoSurfaceClass) {
        LOG_E("FindClass VideoSurface failure.");
        return;
    }

    globalVideoSurfaceClass = (jclass) env->NewGlobalRef(
            localVideoSurfaceClass);
    if (NULL == globalVideoSurfaceClass) {
        LOG_E("localVideoSurfaceClass to globalVideoSurfaceClass failure.");
    }

    globalVideoSurfaceObject = (jclass) env->NewGlobalRef(obj);
    if (NULL == globalVideoSurfaceObject) {
        LOG_E("obj to globalVideoSurfaceObject failure.");
    }

    if (NULL == surface) {
        LOG_E("surface is null, destroy?");
        pWindow = NULL;
        return;
    }
    // 创建新的窗口用于视频显示
    pWindow = ANativeWindow_fromSurface(env, surface);
    if(pWindow)
    LOG_D("mANativeWindow ok");
    else
        LOG_D("mANativeWindow failed");
}