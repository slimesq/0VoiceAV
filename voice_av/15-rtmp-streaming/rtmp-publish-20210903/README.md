# RTMP 推流

使用课程自带的 librtmp 和 FFmpeg 7.1.3 推送 RTMP 流。

## 构建

按[Windows 快速开始](../../../README.md#windows)准备 FFmpeg 7.1.3，并按
[Windows 桌面依赖说明](../../../docs/ffmpeg-dependency.md#windows-desktop-dependencies)准备 x64 SDL2，再从仓库根目录运行。

```powershell
cmake --preset voice-av-windows-all
cmake --build --preset voice-av-windows-all-debug --target voice_av_15_rtmp_publish
```

可执行文件为 `build/cmake/Debug/voice_av_15_rtmp_publish.exe`。
运行前将 x64 SDL2 的 DLL 目录加入 PATH。

## 测试媒体

按[推流测试素材说明](../../test_files/README.md#publisher-test-inputs)准备 PCM 和 YUV 输入。
