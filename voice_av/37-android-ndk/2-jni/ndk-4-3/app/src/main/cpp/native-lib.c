#include <jni.h>

JNIEXPORT jstring JNICALL
Java_com_darren_ndk_14_13_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject obj /* this */) {
    char str[] = "Hello from C";
    return (*env)->NewStringUTF(env, str);
}
/*
 * Class:     com_darren_ndk_4_3_MainActivity
 * Method:    staticStringFromJNI
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_darren_ndk_14_13_MainActivity_staticStringFromJNI
        (JNIEnv *env, jclass jcs)
{
    char str[] = "Static Hello from C";
    return (*env)->NewStringUTF(env, str);
}