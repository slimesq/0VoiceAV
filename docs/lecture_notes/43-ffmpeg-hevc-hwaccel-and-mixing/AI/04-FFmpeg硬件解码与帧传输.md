# FFmpeg 硬件解码与帧传输

> 第43节补充讲义 04 · 承接第8章解码与第10章滤镜 · FFmpeg 7.1 接口

硬件解码仍然接收压缩 packet，输出仍由 `AVFrame` 表示。关键变化是：frame 可能引用
设备表面，后续处理必须理解其存储位置。不能把所有 `frame->data[0]` 都当作 CPU 可读的 Y 平面。

## 1 软件帧与硬件帧

| 后端设备 | 常见硬件像素格式 | 图像实际存储 |
| --- | --- | --- |
| CUDA | `AV_PIX_FMT_CUDA` | CUDA设备存储 |
| D3D11VA | `AV_PIX_FMT_D3D11` | D3D11纹理等设备资源 |
| VAAPI | `AV_PIX_FMT_VAAPI` | VAAPI表面 |
| QSV | `AV_PIX_FMT_QSV` | QSV硬件表面 |

硬件像素格式描述设备资源的表示。硬件池还会有 `sw_format`，例如 NV12、P010；
它描述对应图像的像素布局。`AV_PIX_FMT_CUDA` 不表示“图像没有色度格式”，也不等于 NV12。

先检查 `frame->format` 和有效的 `frame->hw_frames_ctx`，再按对应后端的约定访问资源。
软件帧用 `av_frame_get_buffer()` 分配；硬件帧通常用匹配的帧池和
`av_hwframe_get_buffer()` 分配，二者不能仅靠修改format字段互换。
这里指应用主动分配图像存储；调用 `avcodec_receive_frame()` 接收解码结果时，
通常只需先 `av_frame_alloc()`，由解码器填充输出frame的缓冲区和引用。

普通文件写出、`sws_scale` 和许多 CPU 滤镜需要系统内存中的软件帧。
要处理硬件帧，应先转移到软件帧，或使用支持该后端的处理组件。

## 2 两种上下文各管什么

```text
AVHWDeviceContext                    设备与后端连接
    └─ AVHWFramesContext             一组硬件图像资源及其格式、尺寸
        └─ AVFrame                   引用其中的表面
```

程序通常通过 `AVBufferRef*` 持有上下文引用：

- `dec_ctx->hw_device_ctx` 用于提供设备。
- `dec_ctx->hw_frames_ctx` 或输出 frame 的 `hw_frames_ctx` 描述相应帧资源。
- `av_buffer_ref()` 增加引用，`av_buffer_unref()` 释放自己的引用。

选取支持 `AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX` 的解码配置后，可以让解码器
依据设备和码流建立所需帧池；不必一开始就手工实现所有表面分配。
官方 [FFmpeg 7.1 hw_decode 示例](https://ffmpeg.org/doxygen/7.1/hw_decode_8c-example.html)
演示了配置查询、格式选择、设备创建与下载流程。

## 3 硬解初始化的关键代码

下面的函数从输入 `codecpar` 创建硬件解码器。`state` 由调用方持有，必须一直存活到
解码器释放，因为 `get_format` 回调还会使用它。为每个解码器保存独立 state，避免多路流共用可变全局格式。

```c
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>

typedef struct DecodeHwState
{
    enum AVPixelFormat format;
} DecodeHwState;

static enum AVPixelFormat choose_hw_format(AVCodecContext* ctx,
                                            const enum AVPixelFormat* candidates)
{
    const DecodeHwState* state = ctx->opaque;
    for (const enum AVPixelFormat* p = candidates; *p != AV_PIX_FMT_NONE; ++p)
        if (*p == state->format)
            return *p;
    return AV_PIX_FMT_NONE;
}

static int open_hw_decoder(AVCodecContext** result, const AVCodecParameters* parameters,
                            AVRational packet_time_base, enum AVHWDeviceType type,
                            DecodeHwState* state)
{
    if (!result)
        return AVERROR(EINVAL);
    *result = NULL;
    if (!parameters || !state || parameters->codec_type != AVMEDIA_TYPE_VIDEO ||
        type == AV_HWDEVICE_TYPE_NONE || packet_time_base.num <= 0 || packet_time_base.den <= 0)
        return AVERROR(EINVAL);
    const AVCodec* codec = avcodec_find_decoder(parameters->codec_id);
    AVCodecContext* ctx = NULL;
    AVBufferRef* device = NULL;
    int ret = AVERROR(ENOSYS);
    if (!codec)
        return AVERROR_DECODER_NOT_FOUND;
    state->format = AV_PIX_FMT_NONE;
    for (int i = 0; ; ++i)
    {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
        if (!config)
            break;
        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
            config->device_type == type)
        {
            state->format = config->pix_fmt;
            break;
        }
    }
    if (state->format == AV_PIX_FMT_NONE)
        return ret;
    ctx = avcodec_alloc_context3(codec);
    if (!ctx)
        return AVERROR(ENOMEM);
    ret = avcodec_parameters_to_context(ctx, parameters);
    if (ret < 0)
        goto end;
    ctx->pkt_timebase = packet_time_base;
    ctx->opaque = state;
    ctx->get_format = choose_hw_format;
    ret = av_hwdevice_ctx_create(&device, type, NULL, NULL, 0);
    if (ret < 0)
        goto end;
    ctx->hw_device_ctx = av_buffer_ref(device);
    if (!ctx->hw_device_ctx)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    ret = avcodec_open2(ctx, codec, NULL);
end:
    av_buffer_unref(&device);
    if (ret < 0)
        avcodec_free_context(&ctx);
    *result = ctx;
    return ret;
}
```

`parameters` 和 `packet_time_base` 通常来自选中的视频 `AVStream`。
调用方继续按第8章流程读包、送包、循环收帧，结束时送NULL并排空。
初始化失败时可由上层明确重新建立软件解码流程；不能忽略错误后假装硬解已启用。

`avcodec_open2()` 成功还不代表这条码流已完成硬件验证。图像尺寸、profile等信息
可能在实际解码时才触发表面池初始化和能力检查；`get_format` 也可能被再次调用。
应检查首次输出以及后续每个frame的实际格式，并处理送包、收帧阶段的错误。
若运行中需要退回软件解码，应重新建立匹配的解码状态，并从可恢复的随机接入位置继续，
不能在已经推进状态后随意把同一个packet送给另一个空解码器。

## 4 怎样下载到 CPU

当输出 frame 的格式是已选择的硬件格式时，用一个空的软件 frame 接收下载结果：

```c
#include <errno.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>

static int download_frame(AVFrame* software, const AVFrame* hardware)
{
    if (!software || !hardware || software == hardware || !hardware->hw_frames_ctx)
        return AVERROR(EINVAL);
    av_frame_unref(software);
    int ret = av_hwframe_transfer_data(software, hardware, 0);
    if (ret < 0)
        return ret;
    // The data transfer and the metadata/timestamps are separate operations.
    ret = av_frame_copy_props(software, hardware);
    if (ret < 0)
        av_frame_unref(software);
    return ret;
}
```

下载之后检查 `software->format`，再决定能否交给现有 CPU 滤镜或写文件。
8位4:2:0硬解结果常见为 NV12，10位可能为 P010；不能把所有结果都按三平面 YUV420P 解释。
需要时查询 `av_hwframe_transfer_get_formats()`，或读取实际 frames context 的能力。

CPU滤镜需要YUV420P而下载结果为NV12时，还要做像素格式转换。
`hwdownload` 负责跨内存边界，`format` 约束所需像素格式，图中的 `scale` 等组件完成实际转换。
这些步骤不能混为一谈。数据传输的可用格式与尺寸规则见本地
`third_party/ffmpeg-src/libavutil/hwcontext.h`。
硬件表面的分配尺寸可能大于显示尺寸，下载后也要保留实际 `linesize`。
导出紧密排列的裸图时，按有效宽高逐行复制，或使用 `av_image_copy_to_buffer()`，
不能把包含对齐填充的整块存储直接当作有效像素写出。

`av_hwframe_transfer_data()` 负责图像数据转移，不应被当作自动继承所有 PTS、色彩信息
和 side data 的万能复制函数。本例额外复制属性；若后续改变时间基，仍须重缩放时间戳。

## 5 三条处理路径

```text
软件处理：软件解码 → CPU帧 → CPU滤镜 → 软件或硬件编码

硬解后下载：硬件解码 → GPU帧 → 下载 → CPU滤镜 → 上传/硬件编码

设备内处理：硬件解码 → GPU帧 → 兼容GPU滤镜 → GPU帧 → 硬件编码
```

第二条依然可能有价值，例如软件解码成本很高而必须使用某个 CPU 滤镜。
第三条通常减少主机与设备之间的往返，但要求整条链路兼容。
“零拷贝”在这里关注避免不必要的 CPU 往返，不保证硬件内部从未发生复制。

第10章的 `crop/vflip/overlay` 示例主要处理软件帧。要接入 GPU 输出，需要明确增加
下载和格式转换，或换成对应的 GPU 滤镜；不能只把 `buffer` 的格式改成 CUDA 就完成迁移。

显存路径编码时，应在编码器打开前配置匹配的硬件 frames context。
若从 CPU 上传，通常要先创建并初始化硬件帧池，再分配硬件帧并转移数据；
仅给一个普通 frame 挂上 `hw_frames_ctx` 引用，不会把其 CPU 数据变成有效 GPU 图像。

## 6 本机可做的 D3D11VA 实验

先在本章实验目录检查后端，再用8位 HEVC 样片进行硬解和下载：

```powershell
ffmpeg -hide_banner -hwaccels
ffmpeg -y -loglevel verbose -hwaccel d3d11va -hwaccel_output_format d3d11 -i sample_hevc.mp4 -vf "hwdownload,format=nv12" -an -f rawvideo download_nv12.yuv
ffplay -f rawvideo -pixel_format nv12 -video_size 320x180 -framerate 25 download_nv12.yuv
```

若要输出第10章CPU示例使用的YUV420P，将滤镜参数换成
`-vf "hwdownload,format=nv12,scale=iw:ih,format=yuv420p"`，并换一个输出文件名。
先用设备支持的NV12下载，再在CPU上转换；不要假设设备能直接下载为任意软件格式。

显式要求硬件格式，并使用 `hwdownload`，便于发现实际是否走到了设备表面路径。
仅凭列出了 `d3d11va`，或进程退出码为0，就判断硬解成功还不够。
同时查看 verbose 日志、输出帧格式和输出帧数。

本次在 Intel Arc / Windows 上，该路径输出50帧 NV12，共4,320,000字节。
这只验证了这段8位 HEVC 样片，不代表显卡支持所有 HEVC profile、位深和分辨率。

具有匹配 NVIDIA 环境时可以尝试：

```powershell
ffmpeg -y -hwaccel cuda -hwaccel_output_format cuda -i sample_hevc.mp4 -vf "hwdownload,format=nv12" -an -f rawvideo cuda_download_nv12.yuv
```

这两条下载命令面向本章8位样片。10位输入须重新确认可传输格式，不能照搬 NV12。

## 7 持有帧也会影响吞吐

硬件解码表面池不是无限的。如果应用把每一帧长期放进队列，既不处理也不释放，
解码器就可能因为可用表面不足而停顿。增加池大小不能替代有界队列和及时释放引用。

正确的所有权关系是：需要保留时 `av_frame_ref/clone`，用完 `av_frame_unref/free`。
释放解码器后，下游仍持有的帧可能继续持有设备资源；关闭链路时还要清理这些引用。

## 8 排错与自测

| 现象 | 先检查什么 |
| --- | --- |
| 没有匹配硬件配置 | decoder的 `avcodec_get_hw_config()` 结果和目标设备类型 |
| get_format失败 | 提交的硬件格式是否真的在候选列表中 |
| 读取data指针崩溃或画面错乱 | 是否把设备表面当作CPU地址，是否误认NV12/P010为YUV420P |
| 滤镜无法自动转换格式 | 两端分别是什么内存类型，是否缺少上传、下载或格式转换 |
| 一段时间后解码卡住 | 队列中是否积压了硬件frame引用 |

练习：在同一张图上标出 device context、frames context 和 frame 的关系；
再解释为什么下载改变的是存储位置，`pkt_timebase` 和 PTS 的含义不会因此自动改变。
