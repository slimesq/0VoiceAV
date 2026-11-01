# 第10章：FFmpeg 过滤器实战

本章从桌面课程目录 `10-FFmpeg过滤器实战` 迁入，沿用第8、9章的 CMake
组织方式，使用项目统一的 FFmpeg 7.1.3。原始讲义见
[讲义索引](../../docs/lecture_notes/10-ffmpeg-filtering/README.md)。

原始压缩包中的工程编号仍是 `09-*`，这里按课程章节归入 `10-ffmpeg-filtering`，
保留示例编号；`01` 对应过滤器框架讲解，源材料没有对应的代码工程。

## 示例与原始材料对应关系

| 原始压缩包 | 项目目录 | CMake 目标 | 学习重点 |
| --- | --- | --- | --- |
| `09-02-video-watermark.rar` | `02-video-watermark` | `voice_av_10_02_video_watermark` | 手动创建滤镜、连接 pad；裁剪上半幅、上下翻转后覆盖到下半幅 |
| `09-03-audio-mix.rar` | `03-audio-mix` | `voice_av_10_03_audio_mix` | 多个 `abuffer` → `amix` → `aformat` → `abuffersink` |
| `09-04-video_watermark.rar` | `04-video-watermark` | `voice_av_10_04_video_watermark` | 两张 JPEG 解码后作为双输入，叠加水印并保存 JPEG |
| `09-05-crop_flip.rar` | `05-crop-flip` | `voice_av_10_05_crop_flip` | 用 `avfilter_graph_parse2` 解析字符串，完成与02相同的处理 |
| `ref-09-06-video_logo.rar` | `ref-06-video-logo` | `voice_av_10_ref_06_video_logo` | 视频解码 → `movie`/`overlay` → H.264 编码 |

单文件程序直接放在示例目录；有本地头文件的程序使用 `include/` 和 `src/`。
参考示例与第9章的 `ref-09-06-video-logo` 同源，采用该示例已迁移的编解码流程。

## 构建

从仓库根目录执行，依赖安装方式与第8、9章相同：

```powershell
cmake --preset voice-av-vs2022-x64
cmake --build --preset voice-av-vs2022-x64-debug --target voice_av_10_02_video_watermark voice_av_10_03_audio_mix voice_av_10_04_video_watermark voice_av_10_05_crop_flip voice_av_10_ref_06_video_logo
```

Ubuntu 使用现有 Linux preset，例如：

```bash
cmake --preset voice-av-linux-debug
cmake --build --preset voice-av-linux-debug --target voice_av_10_02_video_watermark voice_av_10_03_audio_mix voice_av_10_04_video_watermark voice_av_10_05_crop_flip voice_av_10_ref_06_video_logo
```

Windows 的输出目录为 `build/cmake/Debug`；Linux 为 `build/cmake-linux-debug/Debug`。

## 输入素材与运行

本次从原始压缩包提取的输入放在 `voice_av/test_files/10-ffmpeg-filtering/`。
该目录中的媒体和运行结果沿用项目规则，由 Git 忽略。

| 输入文件 | 使用示例 | 格式 |
| --- | --- | --- |
| `768x320.yuv` | 02、05共用 | YUV420P，768×320，按25帧/秒解释 |
| `48000_2_f32le.pcm` | 03 | 48000 Hz、双声道、交错排列、32位浮点、小端 |
| `48000_2_s16le.pcm` | 03 | 48000 Hz、双声道、交错排列、16位有符号整数、小端 |
| `main.jpg`、`logo.jpg` | 04 | 主图与水印 |
| `time1.flv`、`logo.jpg` | 参考示例 | 视频与水印 |

从仓库根目录进入素材目录后，运行默认示例：

```powershell
Push-Location .\voice_av\test_files\10-ffmpeg-filtering
..\..\..\build\cmake\Debug\voice_av_10_02_video_watermark.exe
..\..\..\build\cmake\Debug\voice_av_10_03_audio_mix.exe
..\..\..\build\cmake\Debug\voice_av_10_04_video_watermark.exe
..\..\..\build\cmake\Debug\voice_av_10_05_crop_flip.exe
..\..\..\build\cmake\Debug\voice_av_10_ref_06_video_logo.exe
Pop-Location
```

Linux 在同一素材目录运行 `../../../build/cmake-linux-debug/Debug/<目标名>`。

| 示例 | 可选命令行参数（按顺序） | 默认输出 |
| --- | --- | --- |
| 02、05 | `输入.yuv 输出.yuv 宽 高 帧率` | `out_crop_vfilter.yuv`、`out_crop_vfilter_2.yuv` |
| 03 | `输入浮点.pcm 输入整数.pcm 输出.pcm longest或shortest或first` | `output.pcm` |
| 04 | `主图.jpg 水印.jpg 输出.jpg x y` | `test-output3.jpg`，位置 `(100, 200)` |
| 参考示例 | `输入视频 输出.h264 滤镜字符串` | `test.h264` |

可以省略末尾参数使用默认值；02、05的宽高必须成对提供，04的 x、y 必须成对提供。
使用相对路径时，相对于进程当前工作目录解析；也可以给输入、输出传绝对路径。
参考示例默认通过 `movie=logo.jpg` 加载水印，因此也需要在素材目录运行，或传入自定义滤镜字符串。

### 视频裁剪与翻转（02、05）

```text
buffer → split ─────────────────────────→ overlay(main) → buffersink
            └→ crop(上半幅) → vflip ────→ overlay(覆盖层)
```

将上半幅上下翻转后，覆盖到画面的下半幅，输出尺寸保持不变。
为保持 YUV420P 色度采样对齐，输入宽度要求为偶数，高度要求为4的倍数。
源帧 PTS 按 `0, 1, 2, ...` 递增，`buffer` 的时间基为 `1/fps`。
程序还会生成 `<输出文件名>.graph.txt`，可查看实际协商后的滤镜图。

原始 YUV 约477 MiB，每个输出大小与输入相同。可先截取少量帧：

```powershell
ffmpeg -f rawvideo -pixel_format yuv420p -video_size 768x320 -framerate 25 -i 768x320.yuv -frames:v 25 -c:v copy -f rawvideo short.yuv
..\..\..\build\cmake\Debug\voice_av_10_02_video_watermark.exe short.yuv short-filtered.yuv
ffplay -f rawvideo -pixel_format yuv420p -video_size 768x320 -framerate 25 short-filtered.yuv
```

### 音频混音（03）

两路输入都是48 kHz双声道；滤镜图负责格式转换、混音及重采样，输出为
**96 kHz、双声道、交错排列的 s16le PCM**，没有文件头：

```powershell
ffplay -f s16le -ar 96000 -ac 2 output.pcm
```

`longest` 等待最长输入结束，`shortest` 在最短输入结束时结束，`first` 以第一路
输入为准。示例显式设置 `normalize=1:dropout_transition=0`：混音进行归一化，
某路结束后的增益调整不设置过渡时长。`AudioMixer` 的字节缓冲区接口仅接收 packed
格式，不接收 `FLTP`、`S16P` 等 planar 格式。

每路输入分别以 `1/sample_rate` 为时间基，以该路累计采样点数作为 PTS。
`getFrame()` 返回输出字节数；`EAGAIN` 表示需要继续送输入，`EOF` 表示输出结束。
文件尾的不足1024个采样点也会送入，每路单独关闭，最后读完重采样器缓存的输出。

### JPEG 与视频水印（04、参考示例）

04将两张图片解码成 `AVFrame`，通过命名端点 `[main]`、`[logo]`、`[out]`
连接滤镜字符串，并在编码 JPEG 前协商为全范围 `YUVJ420P`。

参考示例默认在视频左下角叠加 `logo.jpg`，输出 H.264 裸码流，只处理视频。
它不保留输入音轨，也不生成 MP4/FLV 容器。

```powershell
ffplay -f h264 test.h264
```

## 与原始课程代码的差异

- 不迁入压缩包附带的 FFmpeg 4.2.1 SDK、Qt `.pro`、旧构建目录和已生成的输出媒体。
- 使用 FFmpeg 7.1.3 的自动注册机制、`const AVFilter*` 和 `AVChannelLayout`，
  图像读写工具函数采用与第9章一致的现代编解码接口。
- 修正原始 `overlay` 位置参数、滤镜实例重名及部分错误处理；输入帧使用引用计数缓冲区。
- 正确处理 `linesize`、输入 PTS、各输入 EOF 和输出排空。视频参考示例依次排空
  解码器、滤镜图、编码器，避免丢失延迟帧。
- 增加命令行参数，移除等待按键的退出方式；文件读写、参数及滤镜错误返回非零退出码。

## 本次验证

Windows / VS2022 Debug、项目 FFmpeg 7.1.3 下，5个目标均编译、运行通过。
与本机 FFmpeg 7.1.1 命令行对照：

- 7帧130×100的 YUV420P 输入：02、05输出及命令行结果逐字节一致；残缺帧会报错。
- 两路长度不同且末块不足1024点的 PCM：三种 `duration` 模式输出均逐字节一致。
- 原始 JPEG：水印输出为可正常解码的983×502图片。
- 原始 `time1.flv`：水印处理后保留219/219帧，H.264 裸码流可完整解码。

Linux 命令沿用项目 preset，本次未在 Linux 上运行验证。
