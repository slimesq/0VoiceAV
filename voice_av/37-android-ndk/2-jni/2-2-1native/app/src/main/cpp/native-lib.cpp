#include <jni.h>
#include <string>
#include "Test.h"
extern "C" JNIEXPORT jstring JNICALL
Java_com_darren_mynative_MainActivity_voceiFromJNI( // 3. 需要和Java的方法名也对应上
        JNIEnv *env,
        jobject /* this */) {
    Test test;
    std::string hello = "Hello from 零声学院" + test.GetTest();

    return env->NewStringUTF(hello.c_str());
}
