# 课程讲义

讲义按课程章号排列。有对应源码的章节使用与 `voice_av/` 一致的目录名。
`0Voice/` 收录课程讲义和配套资料，`AI/` 收录 AI 补充讲义。
CMake 构建步骤见[快速开始](../../README.md#quick-start)。

| 章号 | 课程讲义 | AI 补充讲义 | 对应源码 |
| --- | --- | --- | --- |
| 01 | [音视频基础](01-audio-video-basics/0Voice/) | — | — |
| 02 | [开发环境搭建](02-development-environment/0Voice/) | — | [示例](../../voice_av/02-development-environment/) |
| 03–05 | [FFmpeg 命令实战](03-05-ffmpeg-cli/0Voice/) | — | — |
| 06 | [SDL 音视频渲染](06-sdl-rendering/0Voice/) | — | [示例](../../voice_av/06-sdl-rendering/) |
| 07 | [FFmpeg API 与内存模型](07-ffmpeg-basics/0Voice/) | — | [示例](../../voice_av/07-ffmpeg-basics/) |
| 08 | [FFmpeg 解封装与解码](08-ffmpeg-demuxing-and-decoding/0Voice/) | [MP4 格式分析](08-ffmpeg-demuxing-and-decoding/AI/) | [示例](../../voice_av/08-ffmpeg-demuxing-and-decoding/) |
| 09 | [FFmpeg 编码与封装](09-ffmpeg-encoding-and-muxing/0Voice/) | [AAC、H.264 与 H.265](09-ffmpeg-encoding-and-muxing/AI/) | [示例](../../voice_av/09-ffmpeg-encoding-and-muxing/) |
| 10 | [FFmpeg 过滤器](10-ffmpeg-filtering/0Voice/) · [阅读顺序](10-ffmpeg-filtering/README.md) | — | [示例](../../voice_av/10-ffmpeg-filtering/) |
| 11 | [FFmpeg 与 Qt 播放器](11-ffmpeg-qt-player/0Voice/) | — | [示例](../../voice_av/11-ffmpeg-qt-player/) |
| 12–13 | [ffplay 播放器剖析](12-13-ffplay-player/0Voice/) | — | [示例](../../voice_av/12-13-ffplay-player/) |
| 14 | [FFmpeg 媒体转换器](14-ffmpeg-transcoder/0Voice/) | — | [示例](../../voice_av/14-ffmpeg-transcoder/) |
| 15 | [RTMP 流媒体实战](15-rtmp-streaming/0Voice/) | — | [示例](../../voice_av/15-rtmp-streaming/) |
| 16 | [HLS、MPEG-TS 与 HTTP](16-hls-streaming/0Voice/) | — | [示例](../../voice_av/16-hls-streaming/) |
| 17–19 | [SRS 推拉流与集群](17-19-srs-streaming/0Voice/) | — | — |
| 19–21 | [RTSP、RTP、RTCP 与 SDP](19-21-rtsp-streaming/0Voice/) | — | [示例](../../voice_av/19-21-rtsp-streaming/) |
| 22–28 | [WebRTC 中级与 AppRTC](22-28-webrtc-intermediate/0Voice/) | — | [示例](../../voice_av/22-28-webrtc-intermediate/) |
| 29 | [P2P 框架](29-p2p/0Voice/) | — | — |
| 29–30 | [WebRTC 高级、通话设计与 QoS](29-30-webrtc-advanced/0Voice/) | — | — |
| 31–33 | [Janus 与信令日志](31-33-webrtc-janus/0Voice/) | — | [示例](../../voice_av/31-33-webrtc-janus/) |
| 34 | [SRS 4.0 WebRTC](34-srs-webrtc/0Voice/) | — | — |
| 37 | [Android NDK](37-android-ndk/0Voice/) | — | [示例](../../voice_av/37-android-ndk/) |
| 37 | [GDB、Valgrind 与 CMake](37-gdb-cmake/0Voice/) | — | [示例](../../voice_av/37-gdb-cmake/) |
| 43 | — | [HEVC、硬件编解码与混流](43-ffmpeg-hevc-hwaccel-and-mixing/AI/) · [阅读顺序](43-ffmpeg-hevc-hwaccel-and-mixing/README.md) | 参见第 08–10 章示例 |

## 跨主题参考

- RTMP、HTTP-FLV 与 FLV 解封装共用 [Adobe FLV/F4V 规范 10.1](08-ffmpeg-demuxing-and-decoding/0Voice/FLV-video_file_format_spec_v10_1.pdf) 和 [FLV 封装讲义](08-ffmpeg-demuxing-and-decoding/0Voice/FLV格式分析-FLV封装格式剖析.pdf)。
- RTMP、RTSP 与编解码实验可对照 [AAC ADTS](<08-ffmpeg-demuxing-and-decoding/0Voice/AAC ADTS格式分析.pdf>) 和 [H.264 NALU](<08-ffmpeg-demuxing-and-decoding/0Voice/H264 NALU分析.pdf>)。
- RTSP 和 WebRTC 的 RTP 资料可对照 [RFC 3550 原文](<19-21-rtsp-streaming/0Voice/3-RFC3550(英文)-RTP A Transport Protocol for Real-Time Applications.pdf>) 和 [中文译文](<19-21-rtsp-streaming/0Voice/3-RFC3550(中文)-RTP协议中文版.pdf>)。
- [Qt 课程环境讲义](<02-development-environment/0Voice/QT+FFmpeg4.2.1 Windows开发环境搭建.docx>)记录原课 SDK 环境，当前示例的构建步骤见[Windows 快速开始](../../README.md#windows)。
