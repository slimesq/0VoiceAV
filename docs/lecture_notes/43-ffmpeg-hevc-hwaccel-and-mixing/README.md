# 第43节 HEVC 硬件编解码与音频混流讲义

本组讲义承接项目第8、9、10章，覆盖 H.265 码流与解码、H.265 编码、NVIDIA H.264
硬件编码、通用硬件解码，以及两路音频混音与同步。目标是把已有的解码、编码、
滤镜和封装流程接起来，能够判断问题出在码流、时间戳、内存位置还是结束排空。

这是依据你提供的课程目录与[学习计划](http://39.97.44.190/0voice-av-study-tracker/)
补写的项目讲义，非原课逐字稿。`43` 是你提供的课程资料目录编号；网站将这一组归在
“H265编解码和硬件编解码”模块，不同于本项目的第10章过滤器目录。

## 1 讲义目录

| 课程主题 | 讲义 | 学完应能完成的事 |
| --- | --- | --- |
| 01 H265 NALU分析和解码 | [H.265 NALU分析与解码](AI/01-H265-NALU分析与解码.md) | 识别双字节头、VPS/SPS/PPS、Annex B与长度前缀，并接入解码流程 |
| 02 H265编码实践 | [H.265编码与封装实战](AI/02-H265编码与封装实战.md) | 配置libx265，处理global header、编码缓存和MP4时间戳 |
| 03 NVIDIA硬件编码H264 | [NVIDIA硬件编码H.264](AI/03-NVIDIA硬件编码H264.md) | 区分设备与构建能力，正确配置NVENC并准备真实输入帧 |
| 04 FFmpeg硬件解码 | [硬件解码与帧传输](AI/04-FFmpeg硬件解码与帧传输.md) | 建立硬解上下文，识别硬件帧，下载到CPU或连接兼容的GPU处理链 |
| 05 两路音视频混流中的音频混流 | [两路音频混流与时间同步](AI/05-两路音频混流与时间同步.md) | 混合PCM，维护音频时间线，连接AAC与muxer，理解RTC补偿策略 |

你提供的文件截图列出前三个视频；学习计划还包含04和05，因此本组一并补齐。
MP4合成的回顾部分和后续过滤器课程继续使用第9、10章，不重复扩写为其他课程。

## 2 与第8至10章的对应关系

| 已有基础 | 本次怎样扩展 | 对照代码 |
| --- | --- | --- |
| 第8章parser与解码循环 | 增加HEVC类型、参数集和硬件格式选择 | [视频解码](../../../voice_av/08-ffmpeg-demuxing-and-decoding/06-decode-video/main.c) |
| 第8章音频解码与重采样 | 两路独立解码、格式统一和采样时钟 | [音频重采样](../../../voice_av/08-ffmpeg-demuxing-and-decoding/10-2-audio-resample/src/main.c) |
| 第9章H.264编码 | 替换实现，重新检查私有选项与排空 | [视频编码](../../../voice_av/09-ffmpeg-encoding-and-muxing/02-encode-video/main.c) |
| 第9章MP4合成 | global header、各轨道时间基及写包转换 | [MP4 muxer](../../../voice_av/09-ffmpeg-encoding-and-muxing/05-mp4muxer-5/src/muxer.cpp) |
| 第10章滤镜图与amix | 多输入PTS、EOF、实时节拍与后续编码 | [AudioMixer](../../../voice_av/10-ffmpeg-filtering/03-audio-mix/src/audiomixer.cpp) |

原理复习可看[H.264与H.265对比讲义](../09-ffmpeg-encoding-and-muxing/0Voice/AI/H264与H265编码原理对比讲义.md)
以及[SPS与PPS实战讲义](../09-ffmpeg-encoding-and-muxing/0Voice/AI/H264_SPS与PPS解析及流媒体RTC实战讲义.md)。
本组重点是工程连接，不重复完整的编码工具原理。

## 3 配合学习计划的阅读顺序

以下按计划里连续的5组内容组织，不改变网站的日期或打卡状态。
每组可先用约40分钟阅读对应主题，实验和复述另留时间，避免把看完文字当作完成实操。

| 计划中的课程组合 | 本组阅读位置 | 当次最小产出 |
| --- | --- | --- |
| MP4合成回顾＋H265分析＋编码开头 | 第1讲，接第2讲第1至2节 | 导出HEVC裸流，识别三种参数集 |
| H265编码＋NVENC开头 | 第2讲，接第3讲第1至3节 | 解释global header、编码缓存与设备检查 |
| NVENC＋硬解主体 | 第3讲，接第4讲第1至5节 | 画出CPU帧与GPU帧的数据流向 |
| 硬解收尾＋音频混流开头 | 第4讲实验，第5讲第1至6节 | 下载一段硬解帧，混合不同音频格式 |
| 音频混流收尾＋过滤器课程 | 第5讲第7至11节，接第10章 | 画出双解码器、滤镜、FIFO、AAC与muxer的排空顺序 |

## 4 准备可复现实验

从仓库根目录用 PowerShell 创建本组独立实验目录：

```powershell
New-Item -ItemType Directory -Force .\voice_av\test_files\43-ffmpeg-hevc-hwaccel-and-mixing | Out-Null
Set-Location .\voice_av\test_files\43-ffmpeg-hevc-hwaccel-and-mixing
```

后续命令在该目录运行。媒体和输出遵循项目规则，由 Git 忽略；不依赖缺失的原课媒体。
先生成短 HEVC 样片和两段不同格式的声音：

```powershell
ffmpeg -y -f lavfi -i "testsrc2=size=320x180:rate=25" -t 2 -an -c:v libx265 -preset ultrafast -crf 28 -pix_fmt yuv420p -x265-params "pools=2:frame-threads=1:log-level=error" -tag:v hvc1 sample_hevc.mp4
ffmpeg -y -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=2" -ac 2 -c:a pcm_s16le voice.wav
ffmpeg -y -f lavfi -i "sine=frequency=660:sample_rate=44100:duration=3" -ac 1 -c:a pcm_s16le music.wav
```

这两段正弦波便于听出混合结果与验证时长，文件名不表示其中真的有人声或音乐。
`pools=2:frame-threads=1` 只用于限制这个短实验的 x265 并行度，不是正式产品的性能配置。
回到仓库根目录可执行 `Set-Location ..\..\..`。

正文以 FFmpeg 7.1 系列接口为基准：项目库为 Conan FFmpeg 7.1.3；本次命令行工具为7.1.1。
命令行工具和程序链接库需要分别检查能力，不能互相替代。

Linux 可使用相同的 FFmpeg 参数，将目录命令与路径分隔符换成当前 shell 对应写法。
硬件后端由平台和设备决定；本次运行验证环境是 Windows。

## 5 验收结果与环境边界

| 检查 | 本次结果 |
| --- | --- |
| HEVC编码、MP4转Annex B、头信息追踪、软件解码 | 通过，50帧YUV420P为4,320,000字节 |
| 正常与低延迟HEVC命令 | 均可完成并生成输出 |
| 讲义中的完整C函数 | 已用VS2022与项目FFmpeg 7.1.3编译、链接；NAL头解析及x265全局头初始化检查通过 |
| 第9章现有视频编码器的5帧HEVC输入 | 检出未发送NULL的排空问题；临时副本修正后保留5/5帧，原文件未改动 |
| Intel Arc上的D3D11VA HEVC硬解和下载 | 通过，50帧NV12为4,320,000字节 |
| 硬解下载后转为CPU YUV420P | 通过，50帧结果与软件解码逐字节一致 |
| 48 kHz双声道与44.1 kHz单声道混成AAC | 通过，输出48 kHz双声道，约3秒 |
| 音频时间标签与真实样本偏移对照 | 只改第二路PTS仍从第0点混入；adelay使其从第4800点混入；完整延迟混音示例为约3.1秒 |
| 第10章混音程序 | 通过，输出96 kHz双声道S16，3秒共1,152,000字节 |
| 保留HEVC视频并生成AAC混音轨道 | MP4输出通过 |
| NVIDIA NVENC/NVDEC执行 | 本机未识别到NVIDIA显卡，未运行该硬件路径 |

本次还实际确认：命令行FFmpeg列出了 `h264_nvenc`，但项目当前链接库对
`avcodec_find_encoder_by_name("h264_nvenc")` 返回NULL。
因此，实践NVENC C程序前需要准备包含NVENC的链接库和可用NVIDIA设备。
相关命令与API配置已讲解，但这不构成本机NVENC运行成功的结论。
NVENC的 `delay` 参数已按7.1.3源码和本机工具帮助核对；代码片段重新编译检查通过。

各讲的完整函数是可嵌入工程的代码片段；标为“调用片段”或“流程示意”的部分需要接到
已有上下文中。第2讲明确说明了现有示例需要先修正的收尾调用，按其步骤再做迁移实验。

## 6 最终自检

完成本组学习后，尝试不看正文解释以下问题：

1. NALU、访问单元、AVPacket、AVFrame和RTP包分别是什么边界？
2. HEVC参数集怎样随裸流、MP4和随机接入场景传递？
3. 换成x265或NVENC后，哪些代码可以复用，哪些配置必须重审？
4. 一个frame在CPU还是GPU上，怎样判断，怎样跨边界传输？
5. 两路音频的起始偏移、采样时钟和结束时机怎样保留到最终文件？

最后画出一条完整链路：双路输入分别解码，音频混音后编码，主视频走硬解与硬编，
最终写入MP4。给每条边标上数据类型、内存位置、时间基以及结束信号的传播方向。
