# 0voice_player 第 01 阶段

这一阶段将播放列表和控制栏接到 [`VideoCtl`](src/videoctl.cpp)。可从“打开文件”跟踪本地媒体的打开、FFmpeg 解码和 SDL2 输出，再阅读暂停、跳转与倍速的调用路径。

![课堂界面参考](../../../docs/lecture_notes/11-ffmpeg-qt-player/0Voice/播放器运行截图.png)

## Windows 平台编译调试

按[Windows 快速开始](../../../README.md#windows)准备 FFmpeg 7.1.3，并按
[Windows 桌面依赖说明](../../../docs/ffmpeg-dependency.md#windows-desktop-dependencies)准备 x64 SDL2 和 Qt 6 Widgets。在仓库根目录构建这一阶段。

```powershell
cmake --preset voice-av-windows-all
cmake --build --preset voice-av-windows-all-debug --target voice_av_11_qt_player_01
```

目标生成在 `build/cmake/Debug/voice_av_11_qt_player_01.exe`。运行前将 x64 Qt 和 SDL2 的 DLL 目录加入当前终端的 `PATH`。
