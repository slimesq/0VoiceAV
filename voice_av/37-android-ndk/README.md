# Android NDK 课程示例

在 Android Studio 中分别打开下表的工程目录。Gradle 构建应用并打包 APK，NDK 和 CMake 构建原生库。这三个工程使用项目声明的 Android SDK 30、Build Tools 30.0.3 和 CMake 3.10.2，Gradle Wrapper 版本为 5.4.1。

## 准备 FFmpeg 动态库

| 工程目录 | Gradle 配置的 ABI |
| --- | --- |
| [3-ffmpeg/ffmpeg_4_1](3-ffmpeg/ffmpeg_4_1/) | `arm64-v8a`、`armeabi-v7a` |
| [3-ffmpeg/ffmpeg_4_1-player](3-ffmpeg/ffmpeg_4_1-player/) | `arm64-v8a`、`armeabi-v7a` |
| [4-simple-player](4-simple-player/) | `armeabi-v7a` |

1. 准备与工程头文件匹配的 Android FFmpeg 4.2.1 动态库。模块自带的头文件使用 libavcodec 58.54.100。选择符合 App 最低 API 23 的交叉编译目标，并启用下列八个库。按实际环境配置课堂脚本中的 NDK 和依赖路径。[课程源码包](../02-development-environment/upstream-sources/ffmpeg-4.2.1.tar.bz2) · [NDK r22 编译讲义](<../../docs/lecture_notes/37-android-ndk/0Voice/Android利用ndk(r22)编译ffmpeg-4.2.1-0.1-beta.pdf>) · [课堂脚本](3-ffmpeg/ffmpeg-4.2.1_sh/ffmpeg_build.sh)
2. 将每个 ABI 的动态库放入该模块的 `app/src/main/jniLibs/<ABI>/`。FFmpeg 的 `armv8-a` 产物放入 `arm64-v8a`，`armv7-a` 产物放入 `armeabi-v7a`。如有额外的动态依赖，将同 ABI 的依赖库放入同一目录。
3. 确认每个 ABI 目录中有 `libavcodec.so`、`libavdevice.so`、`libavfilter.so`、`libavformat.so`、`libavutil.so`、`libpostproc.so`、`libswresample.so` 和 `libswscale.so`。CMake 会在配置时报告缺失文件和预期目录。

准备好动态库后，从对应工程目录构建 Debug APK。

```bash
bash ./gradlew :app:assembleDebug
```

Windows 可从同一目录运行 `.\gradlew.bat :app:assembleDebug`。
JNI 入门示例和上游播放器参考工程按各自的 Gradle 配置构建。
