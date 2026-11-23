# zero_player

RTMP 拉流播放器。

按[Windows 快速开始](../../../README.md#windows)准备 FFmpeg 7.1.3，并按
[Windows 桌面依赖说明](../../../docs/ffmpeg-dependency.md#windows-desktop-dependencies)准备 x64 SDL2 和 Qt 6 Widgets。
在仓库根目录执行以下命令。

```powershell
cmake --preset voice-av-windows-all
cmake --build --preset voice-av-windows-all-debug --target voice_av_15_zero_player
```

运行 `build/cmake/Debug/voice_av_15_zero_player.exe` 前，将 x64 Qt 和 SDL2 的
DLL 目录加入 PATH，并将 `QT_PLUGIN_PATH` 指向 Qt 安装目录下的 `plugins`。

2021-06-12
（1）修复ffmpeg多slice推送rtmp的问题，需要攒积同一帧的slice再送给解码器，否则出现解码P帧错误，以及花屏的问题
（2）修复播放，然后停止异常的问题

2021-08-31
（1）删除部分多余的文件
（2）增加支持缓存长度的设置
（3）增加变速播放以消耗多余的缓存

2021-09-03
（1）修复高版本QT编译错误的问题，主要是命名空间包含了头文件，比较大的坑。
