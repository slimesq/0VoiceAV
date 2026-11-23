#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_darren_ndk_14_13_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
/*
 * Class:     com_darren_ndk_4_3_MainActivity
 * Method:    staticStringFromJNI
 * Signature: ()Ljava/lang/String;
 */
extern "C" JNIEXPORT jstring JNICALL Java_com_darren_ndk_14_13_MainActivity_staticStringFromJNI
        (JNIEnv *env, jclass)
{
    std::string hello = "Static Hello from C++";
    return env->NewStringUTF(hello.c_str());
}