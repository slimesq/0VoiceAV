# NVIDIA 硬件编码 H.264

> 第43节补充讲义 03 · 承接第9章编码循环 · FFmpeg 7.1 接口

本讲把视频编码的执行位置从 CPU 软件编码器切换到 NVIDIA 的视频编码硬件。
接口仍然是 `AVFrame → send_frame → receive_packet`，新增的重点是设备能力、
输入内存位置和编码器专用选项。

## 1 NVENC 与 NVDEC

| 名称 | 执行的任务 | FFmpeg 中常见入口 |
| --- | --- | --- |
| NVENC | 原始图像压缩为视频码流 | `h264_nvenc`、`hevc_nvenc` |
| NVDEC | 视频码流解码为图像 | `-hwaccel cuda` 配合支持的解码器 |
| CUDA | GPU计算及相关设备、内存接口 | CUDA设备上下文、CUDA帧与相关滤镜 |

NVENC/NVDEC 是专门的视频硬件，不能简单理解为“把 x264/x265 移到 CUDA 核心运行”。
硬编支持哪些编码标准、位深和色度格式，必须结合具体显卡与驱动确认。
可查阅 [NVIDIA 的 FFmpeg 指南](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/ffmpeg-with-nvidia-gpu/index.html)。

课程这一讲指定的是 **H.264 编码**，所以使用 `h264_nvenc`；想实验 HEVC 时使用
`hevc_nvenc`，并重新确认其能力和选项，不是仅把输出文件名改成 `.hevc`。

## 2 能力检查分三个层次

在 PowerShell 中检查命令行工具：

```powershell
Get-Command ffmpeg | Select-Object Source
ffmpeg -hide_banner -encoders | Select-String 'h264_nvenc|hevc_nvenc'
ffmpeg -hide_banner -h encoder=h264_nvenc
nvidia-smi
```

需要分别回答：

1. FFmpeg 构建时是否包含相应编码器？
2. 系统中是否有支持该编码功能的 NVIDIA 显卡和匹配的驱动？
3. 当前进程能否创建设备、初始化编码会话并接受所选参数？

编码器出现在列表中，只说明第一层通过。`nvidia-smi` 能运行，也不能代替一次真实编码测试。
运行已编译工具通常不要求另外安装整套 CUDA Toolkit；编译 GPU 滤镜等组件的要求需另查构建配置。

项目通过 Conan 链接 FFmpeg 7.1.3，PATH 中的 `ffmpeg.exe` 是另一套构建。
因此，项目程序还要用 `avcodec_find_encoder_by_name("h264_nvenc")` 单独检查。
项目的 `full` 依赖方案也不能直接等同于“全部 NVIDIA 功能已启用”，见
[依赖说明](../../../ffmpeg-dependency.md)中尚未覆盖的原生配置项。

本次编写环境识别到 Intel Arc 显卡，未识别到 NVIDIA 显卡。NVENC 命令保留为
具备 NVIDIA 环境时的练习；硬件解码可先做[下一讲](04-FFmpeg硬件解码与帧传输.md)中的 D3D11VA 实验。

## 3 先用 CPU 内存中的帧做硬编码

最容易验证的路径是：

```text
文件或采集 → CPU中的YUV帧 → NVENC内部上传与编码 → H.264 packet
```

这条路径使用硬件编码，但输入仍在系统内存。不要因为编码器名带 `nvenc`，就把
`frame->format` 设置成 `AV_PIX_FMT_CUDA`；CUDA 格式表示硬件帧，必须有真实的设备存储。

在本章实验目录运行：

```powershell
ffmpeg -y -i sample_hevc.mp4 -map 0:v:0 -an -vf format=nv12 -c:v h264_nvenc -preset p4 -tune ll -rc cbr -b:v 2M -maxrate 2M -bufsize 2M -g 25 -bf 0 -rc-lookahead 0 -zerolatency 1 -delay 0 nvenc_h264.mp4
ffprobe -v error -select_streams v:0 -count_frames -show_entries stream=codec_name,width,height,nb_read_frames -of default=noprint_wrappers=1 nvenc_h264.mp4
```

这里输入用 HEVC 样片，命令未启用硬解，因此是“软件解码＋硬件编码”。
预期输出为 H.264、320×180、50帧。输入、输出使用何种编码，与解码、编码是否使用硬件
是两组独立的问题。

| 选项 | 本例作用 | 不能据此推出的结论 |
| --- | --- | --- |
| `preset p4` | 选择速度与质量取舍 | 不能映射成 x264 的同级 preset |
| `tune ll` | 选择低延迟调优 | 不保证网络和播放器无缓存 |
| `rc cbr`、`b:v 2M` | 选择目标码率控制方式 | 不保证每帧字节数完全相同 |
| `bufsize 2M` | 配置码率控制缓冲模型大小，单位为bit | 不是网络发送队列的长度 |
| `bf 0` | 禁用B帧 | 不能排除全部编码流水线延迟 |
| `rc-lookahead 0` | 关闭该前向分析配置 | 不会自动排空已提交帧 |
| `zerolatency 1` | 设置相关无重排序延迟选项 | 不表示调用编码函数耗时为0 |
| `delay 0` | 降低FFmpeg封装层等待输出的缓存帧数 | 不能消除硬件实际编码耗时 |

这些是实验起点，不是所有码率和场景的最优参数。FFmpeg 7.1.3 的参数定义可核对
[NVENC H.264 选项源码](https://raw.githubusercontent.com/FFmpeg/FFmpeg/n7.1.3/libavcodec/nvenc_h264.c)。

这里显式设置 `delay=0`，是因为关闭B帧和lookahead后，FFmpeg的NVENC封装层仍可能
等待一定数量的表面再交付输出。这个等待量由 `delay` 对应的异步深度控制，逻辑可见
[NVENC输出调度源码](https://raw.githubusercontent.com/FFmpeg/FFmpeg/n7.1.3/libavcodec/nvenc.c)。
降低等待可能影响吞吐；应分别测首包等待和持续编码速度，再决定产品配置。

## 4 在第9章编码器中设置什么

先按名称获取 `h264_nvenc` 并据此分配 `AVCodecContext`。下列函数设置低延迟实验参数，
返回后由调用方执行 `avcodec_open2()`；失败时释放字典和 context。
若目标是 MP4，仍应在打开编码器前按输出格式设置 global header。

```c
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>

static int configure_nvenc(AVCodecContext* ctx, AVDictionary** options,
                            int width, int height)
{
    if (!ctx || !options || width <= 0 || height <= 0 || width % 2 || height % 2)
        return AVERROR(EINVAL);
    ctx->width = width;
    ctx->height = height;
    ctx->pix_fmt = AV_PIX_FMT_NV12;
    ctx->time_base = (AVRational){1, 25};
    ctx->framerate = (AVRational){25, 1};
    ctx->bit_rate = 2000000;
    ctx->rc_max_rate = 2000000;
    ctx->rc_buffer_size = 2000000;
    ctx->gop_size = 25;
    ctx->max_b_frames = 0;
    int ret = av_dict_set(options, "preset", "p4", 0);
    if (ret >= 0)
        ret = av_dict_set(options, "tune", "ll", 0);
    if (ret >= 0)
        ret = av_dict_set(options, "rc", "cbr", 0);
    if (ret >= 0)
        ret = av_dict_set(options, "rc-lookahead", "0", 0);
    if (ret >= 0)
        ret = av_dict_set(options, "zerolatency", "1", 0);
    if (ret >= 0)
        ret = av_dict_set(options, "delay", "0", 0);
    return ret;
}
```

成功打开后，检查传入字典中是否还有未消费的选项。不要直接沿用 x264 的
`tune=zerolatency` 或 x265 的 `x265-params`，这些私有参数属于具体实现。

后续仍要分配输入 frame、设置真实像素格式、逐帧送入并循环收包，结束时发送NULL并排空。
硬件编码器也可能缓存帧。

## 5 NV12 与 YUV420P 的差异

两者都是本例8位4:2:0的表示，但存储布局不同：

```text
YUV420P：data[0] 是Y，data[1] 是U，data[2] 是V
NV12：   data[0] 是Y，data[1] 是交错UV：U0 V0 U1 V1 ...
```

把 `frame->format` 从 YUV420P 改成 NV12，并不会自动把两个色度平面交错排列。
必须用 `sws_scale`、滤镜或明确的格式转换生成真实 NV12；每个平面也要考虑 `linesize`。
色彩范围、颜色矩阵和位深同样需要保留，不能只检查宽高。

## 6 再考虑 GPU 帧直连

```text
NVDEC → GPU帧 → 兼容的GPU滤镜 → GPU帧 → NVENC
```

这条路径要求实际GPU、后端、像素格式以及帧上下文彼此兼容。CUDA 输入的编码器通常需要
`pix_fmt = AV_PIX_FMT_CUDA`，并在打开时持有匹配的 `hw_frames_ctx` 引用。
若前面有滤镜，应使用实际送给编码器的输出帧所属的 frames context，不能无条件复制解码器的旧池。

对应的整链路命令为：

```powershell
ffmpeg -y -hwaccel cuda -hwaccel_output_format cuda -i sample_hevc.mp4 -map 0:v:0 -an -c:v h264_nvenc -preset p4 -tune ll -rc cbr -b:v 2M -maxrate 2M -bufsize 2M -g 25 -bf 0 -rc-lookahead 0 -zerolatency 1 -delay 0 nvdec_nvenc.mp4
```

这个输入是 HEVC，所以还要求显卡支持该流的 HEVC 硬解；支持 H.264 硬编并不能保证这条命令一定成功。

## 7 性能观察与排错

同时记录输出帧数、编码速度、CPU占用、视频编码引擎利用率、首包等待和稳态延迟。
文件离线转码的 `fps` 或 `speed` 不能直接代表 RTC 端到端延迟；磁盘读写、内存上传、
滤镜和队列都可能成为瓶颈。

| 报错或现象 | 检查方向 |
| --- | --- |
| Unknown encoder或查找返回NULL | 当前这套 FFmpeg 是否编入 NVENC |
| 无法加载CUDA或编码驱动库 | NVIDIA驱动、运行环境、远程会话可见性 |
| No capable devices | 显卡功能、编码格式和会话初始化条件 |
| Pixel format不支持 | 用该构建的编码器帮助检查支持格式，确认真实数据布局 |
| 硬编了但CPU仍很高 | 是否仍有软件解码、CPU滤镜或频繁上传下载 |

自测：解释“软件解码＋硬编”“硬解＋CPU滤镜＋硬编”“GPU帧直连”三条路径的内存流向，
再说明为什么仅替换编码器名字不能自动得到第三条路径。
