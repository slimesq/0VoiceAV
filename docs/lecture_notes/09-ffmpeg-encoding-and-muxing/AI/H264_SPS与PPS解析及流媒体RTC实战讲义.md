# H.264 SPS 与 PPS 解析及流媒体 RTC 实战讲义

> 项目章节 09 · Markdown 讲义 · 2026-11-01
>
> 面向流媒体、RTSP、RTMP、WebRTC、SFU 和音视频网关开发。
>
> 学习主线：先从传输封装中恢复完整 NAL，再把 NAL 载荷转换为 RBSP，按位读取参数，最后按流与参数 ID 管理解码配置。正文以 H.264 单层码流为主，H.265 的迁移区别见后文。

## 01 学习目标与开发场景

接收一个新视频流时，开发者通常需要回答：这是什么 Profile？分辨率是多少？解码器是否支持？参数集是否完整？为什么收到关键帧仍然无法解码？切流或分辨率变化以后，旧配置还能不能用？

学完本讲，应能：

1. 区分 SPS、PPS、Slice、NAL 与视频帧。
2. 识别 Annex B、长度前缀码流、avcC 与 RTP 载荷。
3. 解释 EBSP、RBSP、防竞争字节和指数哥伦布编码。
4. 按语法分支解析 SPS/PPS，并计算编码尺寸与裁剪尺寸。
5. 为 RTC 接收链路设计参数集缓存、切流与异常恢复流程。
6. 使用 FFmpeg 验证抓包和手算结果。

SPS/PPS 的解析属于码流控制与配置处理，不需要先解码出像素。完整实现应遵循 [H.264 标准](https://www.itu.int/rec/T-REC-H.264-201610-S/en)的语法与取值约束。

| 阅读目的 | 章节 | 应先建立的认识 |
|---|---|---|
| 接入与排错 | 02 至 04、11 至 14 | 先恢复完整 NAL，再检查参数集和图像依赖 |
| 实现逐位解析 | 05 至 10 | 去除防竞争字节，按语法顺序读取，再计算尺寸 |
| 扩展到 HEVC | 15 | 复用流程思想，重新实现 H.265 的语法与封包处理 |

首次阅读可先理解 ue 的例子和第 09 节的逐位推导，再回看第 06 节的 C 实现。

## 02 SPS PPS 与 Slice 的引用关系

| 名称 | 英文全称 | 主要职责 |
|---|---|---|
| SPS | Sequence Parameter Set | 描述序列级编码配置，例如 Profile、编码尺寸、参考图像相关限制和可选 VUI |
| PPS | Picture Parameter Set | 描述一组图像编码配置，例如熵编码方式、默认参考索引数量及部分量化、滤波控制 |
| Slice | Slice | 一幅图像中的编码切片，头部通过 PPS ID 选择相关配置 |

名称中的 Picture 不表示“每个画面都必须发送一个新 PPS”。一个 PPS 可以被多幅图像复用；一个流也可以存在多个 SPS 和 PPS。

```text
Slice Header
    pic_parameter_set_id = 3
                │
                ▼
PPS 3
    seq_parameter_set_id = 1
                │
                ▼
SPS 1
    Profile、尺寸、位深、POC 相关配置、可选 VUI……
```

生效关系由引用决定。收到一个新 SPS，不意味着下一幅图像一定使用它；不能仅维护“最后收到的一个 SPS”和“最后收到的一个 PPS”。

例如：

```text
已缓存：SPS 0、SPS 1、PPS 0 → SPS 0、PPS 3 → SPS 1
当前 Slice 引用 PPS 0
当前使用：PPS 0 与 SPS 0
```

参数集及其引用思想见 [RFC 6184 的参数集说明](https://www.rfc-editor.org/rfc/rfc6184#section-1.2)。

## 03 NAL 帧与访问单元

H.264 的普通 NAL 头占 1 字节：

```text
bit 7       bit 6..5       bit 4..0
F           NRI            Type
1 bit       2 bits         5 bits
```

- F 是 forbidden_zero_bit，合法普通码流中应为 0。
- NRI 是 nal_ref_idc，与参考重要性等语义有关。
- Type 是 nal_unit_type，表示载荷类型。

```c
uint8_t nal_type = nal_header & 0x1F;
```

| NAL 类型 | 含义 |
|---|---|
| 1 | 非 IDR 图像的编码 Slice |
| 5 | IDR 图像的编码 Slice |
| 6 | SEI |
| 7 | SPS |
| 8 | PPS |
| 9 | AUD |

一个视频帧可能包含多个 Slice NAL；SPS/PPS 自身也不是图像帧。Access Unit，简称 AU，组织一幅主编码图像及其相关 NAL，不能将“一个 NAL”“一个 RTP 包”“一帧图像”视为固定的一一对应关系。

I Slice 表示帧内编码类型，IDR 表示一种解码刷新语义。存在非 IDR 的 I 图像，不能仅凭“I”就推断从这里一定可以无条件接入播放。NAL 类型定义可对照 [FFmpeg 的 H.264 定义](https://ffmpeg.org/doxygen/trunk/h264_8h_source.html)。

## 04 解析前先识别外层格式

| 输入来源 | 外层组织 | 进入 SPS/PPS 位解析前应做什么 |
|---|---|---|
| Annex B 裸流或常见 TS 解复用结果 | 起始码分隔 NAL | 找到 NAL 边界，去掉起始码 |
| 常见 MP4 视频 sample | 长度字段加 NAL | 从 avcC 读取长度字段大小，逐个取出 NAL |
| MP4 的 avcC | 解码配置记录 | 读取记录内的 SPS/PPS 数组及每项长度 |
| RTSP SDP 参数 | Base64 编码的参数集 | Base64 解码后取得 NAL |
| RTP 单 NAL 包 | RTP 头后直接是 NAL | 解析 RTP 头及扩展、去掉 RTP padding，取得载荷 |
| RTP STAP-A | 多个 NAL 的聚合 | 按聚合内部长度逐项拆出 NAL |
| RTP FU-A | 一个 NAL 的分片 | 完成排序和重组，恢复原始 NAL 头 |

### Annex B

```text
00 00 00 01 | SPS NAL
00 00 01    | PPS NAL
00 00 00 01 | IDR Slice NAL
```

起始码可能为 3 字节或 4 字节，流边界还可能有额外零字节。不能把每个 NAL 的起始位置都硬编码为“跳过 4 字节”，也不能只寻找下一段固定四字节起始码。

### avcC 与长度前缀

avcC 是 AVCDecoderConfigurationRecord。其基本内容包括版本、Profile、兼容性、Level、lengthSizeMinusOne，以及带长度的 SPS/PPS 数组。相应 Profile 还可能有扩展字段。

视频 sample 中的 NAL 长度字段大小由配置确定，常见为 4 字节；参数集数组中每项的长度字段则是记录语法规定的 16 位字段。两种长度不能混淆。

```text
avcC 配置记录：配置字段 | SPS 数组 | PPS 数组 | 可能的扩展
视频 sample ：NAL 长度 | NAL 字节 | NAL 长度 | NAL 字节……
```

avcC 不是由起始码分隔的裸流。实际解析可对照 [FFmpeg 的 AVC 配置记录处理](https://ffmpeg.org/doxygen/trunk/avc_8c_source.html)。

### 分离原始数据与位解析数据

参数集缓存建议保留两份职责不同的内容：

- 原始完整 NAL：包括 NAL 头，用于转发、生成解码配置或按目标格式重新封装。
- 解析结果与临时 RBSP：用于读取字段、检查能力和配置变化。

不要把去掉防竞争字节后的临时 RBSP 直接当作原始 NAL 转发。

## 05 EBSP 到 RBSP 的转换

RBSP 是 Raw Byte Sequence Payload，原始字节序列载荷，用来按位解释 SPS/PPS 等语法。将 RBSP 按规则插入防竞争字节后，得到 EBSP，即 Encapsulated Byte Sequence Payload，封装字节序列载荷。这里的载荷都不包含前面的 NAL 头。

为了避免载荷内部出现会被误认作起始码的序列，编码端可能插入 emulation_prevention_three_byte，即防竞争字节 0x03。解析端必须先还原 RBSP，再读取字段。

| NAL 载荷中的字节 | 去除对应防竞争字节后 |
|---|---|
| `00 00 03 00` | `00 00 00` |
| `00 00 03 01` | `00 00 01` |
| `00 00 03 03` | `00 00 03` |

实际处理顺序：

```text
完整 NAL
   ↓ 检查并移除 1 字节 NAL 头
EBSP 载荷
   ↓ 按规则去除防竞争字节
RBSP
   ↓ 按位解析语法元素
字段与配置
```

不能删除数据中所有的 0x03。应根据前导零、后续字节和边界判断是否合法；遇到截断或非法组合应报告解析失败。

RBSP 结尾还包含 rbsp_trailing_bits，即停止位 1 和用于字节对齐的零位。它们不属于某个普通参数字段。判断 PPS 是否还有扩展字段，应依据 more_rbsp_data 的语义，不能仅用“当前还没到字节末尾”判断。

## 06 位读取与指数哥伦布编码

H.264 语法表中的标记决定如何读取字段：

| 标记 | 读取方式 | 示例 |
|---|---|---|
| `u(n)` | 读取 n 位无符号值 | `profile_idc: u(8)` |
| `ue(v)` | 无符号指数哥伦布编码 | SPS ID、PPS ID、部分尺寸字段 |
| `se(v)` | 有符号指数哥伦布编码 | 量化偏移、部分 POC 偏移 |

### ue 的计算

先数连续前导零的个数 k，再读取停止的 1 后面的 k 位后缀：

```text
codeNum = 2^k − 1 + suffix
```

| 位串 | k | 后缀 | 解码值 |
|---|---|---|---|
| `1` | 0 | 空 | 0 |
| `010` | 1 | 0 | 1 |
| `011` | 1 | 1 | 2 |
| `00100` | 2 | 00 | 3 |
| `00101` | 2 | 01 | 4 |
| `00000101000` | 5 | 01000 | 39 |

最后一个例子：`2^5 − 1 + 8 = 39`。这也说明字段不是从固定字节偏移开始读取的。

se 在 ue 解出的 codeNum 上进行映射：

```text
codeNum：0   1   2   3   4   5
se 值 ：0  +1  −1  +2  −2  +3
```

### 实现选读 带边界检查的 C 位读取基础

以下代码只实现位读取原语，不是完整 SPS/PPS 解析器。它限制 ue 的前导零不超过 31，并在输入不足或超出该实现范围时失败；语法字段还需要自己的取值检查。

```c
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;
    size_t bits;
    size_t pos;
} BitReader;

static int br_init(BitReader *br, const uint8_t *data, size_t size)
{
    if (!br || (size && !data) || size > SIZE_MAX / 8)
        return 0;
    br->data = data;
    br->bits = size * 8;
    br->pos = 0;
    return 1;
}

static int br_u(BitReader *br, unsigned n, uint32_t *out)
{
    if (!br || !out || n > 32 || br->pos > br->bits ||
        n > br->bits - br->pos)
        return 0;

    uint32_t value = 0;
    for (unsigned i = 0; i < n; ++i) {
        size_t p = br->pos++;
        uint32_t bit = (br->data[p / 8] >> (7 - p % 8)) & 1u;
        value = (value << 1) | bit;
    }
    *out = value;
    return 1;
}

static int br_ue(BitReader *br, uint32_t *out)
{
    if (!out)
        return 0;
    unsigned zeros = 0;
    uint32_t bit, suffix;
    for (;;) {
        if (!br_u(br, 1, &bit))
            return 0;
        if (bit)
            break;
        if (++zeros > 31)
            return 0;
    }
    if (!br_u(br, zeros, &suffix))
        return 0;
    *out = (uint32_t)(((uint64_t)1 << zeros) - 1 + suffix);
    return 1;
}

static int br_se(BitReader *br, int32_t *out)
{
    uint32_t code;
    if (!out || !br_ue(br, &code))
        return 0;
    int64_t value = (code & 1)
        ? (int64_t)(((uint64_t)code + 1) / 2)
        : -(int64_t)(code / 2);
    *out = (int32_t)value;
    return 1;
}
```

解析失败后应终止当前语法分支，避免用未初始化的输出值继续计算尺寸或分配内存。工业实现可对照 [FFmpeg 的 SPS/PPS 解析源码](https://ffmpeg.org/doxygen/9.0/h264__ps_8c_source.html)。

## 07 SPS 的解析顺序与关键分支

下面是主要语法路线，不是可以省略其他字段的固定偏移表。

| 顺序 | 主要字段或分支 | 开发用途 |
|---|---|---|
| 1 | `profile_idc`、约束标志、`level_idc` | 判断编码工具与能力要求 |
| 2 | `seq_parameter_set_id` | 建立 SPS ID 索引，范围为 0 到 31 |
| 3 | 特定 Profile 的附加语法 | 色度格式、位深、独立颜色平面和缩放矩阵 |
| 4 | `log2_max_frame_num_minus4` | 解释后续 Slice 中的 frame_num 位宽 |
| 5 | `pic_order_cnt_type` 及其分支 | 解释图像显示顺序相关语法 |
| 6 | `max_num_ref_frames` 等 | 理解参考图像需求 |
| 7 | 宏块宽度、map unit 高度与逐行标志 | 计算编码图像尺寸 |
| 8 | `frame_cropping_flag` 与裁剪偏移 | 计算裁剪后的尺寸 |
| 9 | `vui_parameters_present_flag` | 继续解析可选 VUI |
| 10 | RBSP 结束语法 | 检查尾部与完整性 |

### High 等 Profile 的附加字段

例如 High Profile 使用附加语法，包括 chroma_format_idc、bit_depth_luma_minus8、bit_depth_chroma_minus8 等。是否进入该分支应按标准列出的 Profile 集合判断，不能简单写成“profile_idc 大于某个数”。

若 chroma_format_idc 为 3，还要读取 separate_colour_plane_flag。出现缩放矩阵时，需要继续按语法消费各列表及其 se 编码的差值；只跳过一个“矩阵存在”标志，会让后续字段全部错位。

### POC 分支

| `pic_order_cnt_type` | 主要处理 |
|---|---|
| 0 | 读取 `log2_max_pic_order_cnt_lsb_minus4` 等相关配置 |
| 1 | 读取标志、有符号偏移，以及参考周期偏移数组 |
| 2 | SPS 中没有上述两组附加字段 |

不能默认所有流的 pic_order_cnt_type 都为 0。本讲的实际 SPS 示例为 2。

`max_num_ref_frames` 也不能当作连续 B 帧数量。它涉及参考图像的数量约束；B 帧配置、参考图像管理和显示重排需要分别理解。

### VUI 与帧率

VUI 可以包含宽高比、颜色描述、时序、HRD 和比特流约束等信息，字段通常由存在标志控制。

在常见的逐行 H.264 时序语义下，人们会计算：

```text
名义帧率候选 = time_scale / (2 × num_units_in_tick)
```

但 SPS 时序不是完整的实际播放时间轴。要结合 fixed_frame_rate_flag、图像或场结构等语义，并以容器时间戳或实际传输时序解释播放，不能只取这个比值就断言视频是固定帧率。

## 08 从 SPS 计算编码尺寸与裁剪尺寸

设：

```text
M = pic_width_in_mbs_minus1 + 1
U = pic_height_in_map_units_minus1 + 1
F = frame_mbs_only_flag

coded_width  = 16 × M
coded_height = 16 × U × (2 − F)
```

裁剪偏移的单位由色度格式与逐行标志决定，并不总是一个像素。

定义 ChromaArrayType：普通情况等于 chroma_format_idc；独立颜色平面情况按标准视为 0。

| ChromaArrayType | 格式 | cropUnitX | cropUnitY |
|---|---|---|---|
| 0 | 无色度数组或独立颜色平面情况 | 1 | `2 − F` |
| 1 | 4:2:0 | 2 | `2 × (2 − F)` |
| 2 | 4:2:2 | 2 | `2 − F` |
| 3 | 4:4:4 | 1 | `2 − F` |

```text
width  = coded_width  − cropUnitX × (crop_left + crop_right)
height = coded_height − cropUnitY × (crop_top  + crop_bottom)
```

乘法与加法应使用足够宽的整数，并检查裁剪量不超过编码尺寸、最终尺寸为正且符合应用限制。SAR、旋转或 WebRTC CVO 等显示信息也不能当成这组裁剪字段。

## 09 一组可以复算的真实 SPS 与 PPS

以下参数集来自本机 FFmpeg 7.1.1 与 libx264 生成的黑色视频样本，输入尺寸为 640×360、输入帧率为 30。它们是该次编码的结果，换版本或参数可能得到不同字节。

```text
SPS NAL，包含 1 字节 NAL 头，不包含起始码：
67 42 c0 1e d9 00 a0 2f f9 70 11 00 00 03 00 01
00 00 03 00 3c 0f 16 2e 48

PPS NAL：
68 cb 83 cb 20
```

先做三步：检查 NAL Type、去掉 NAL 头、对载荷去除合法防竞争字节。SPS 中两处 00 00 03 需要按规则处理。

### 从位串读到尺寸字段

下表偏移从还原后的 RBSP 第一位算起，**不包含 1 字节 NAL 头，也不包含已移除的防竞争字节**。当前 Profile 不需要进入 High 等 Profile 的附加语法分支。

| RBSP 起始位 | 字段 | 编码位串 | 读取结果 |
|---|---|---|---|
| 0 | profile_idc | `01000010` | 66 |
| 8 | 约束与保留位字节 | `11000000` | 0xC0，继续按各标志位解释 |
| 16 | level_idc | `00011110` | 30 |
| 24 | seq_parameter_set_id | `1` | ue=0 |
| 25 | log2_max_frame_num_minus4 | `1` | ue=0 |
| 26 | pic_order_cnt_type | `011` | ue=2 |
| 29 | max_num_ref_frames | `00100` | ue=3 |
| 34 | gaps_in_frame_num_value_allowed_flag | `0` | 0 |
| 35 | pic_width_in_mbs_minus1 | `00000101000` | ue=39 |
| 46 | pic_height_in_map_units_minus1 | `000010111` | ue=22 |
| 55 | frame_mbs_only_flag | `1` | 1 |
| 56 | direct_8x8_inference_flag | `1` | 1 |
| 57 | frame_cropping_flag | `1` | 1 |
| 58、59、60 | 左、右、上裁剪偏移 | 各为 `1` | 三个 ue 值均为 0 |
| 61 | 下裁剪偏移 | `00101` | ue=4 |
| 66 | vui_parameters_present_flag | `1` | 1 |

例如，从第 35 位开始，先读到 5 个零，再读到 1，随后读取 5 位后缀 01000，得到 31+8=39。这个字段共占 11 位，所以下一个字段从第 46 位开始，而不是从下一个字节开始。

### 汇总字段并计算结果

| SPS 字段 | 本例结果 |
|---|---|
| `profile_idc` | 66，即 Baseline Profile 编码家族 |
| 约束字节 | 0xC0，应继续按位解释约束标志 |
| `level_idc` | 30，即 Level 3.0 |
| `seq_parameter_set_id` | 0 |
| `log2_max_frame_num_minus4` | 0 |
| `pic_order_cnt_type` | 2 |
| `max_num_ref_frames` | 3 |
| `pic_width_in_mbs_minus1` | 39 |
| `pic_height_in_map_units_minus1` | 22 |
| `frame_mbs_only_flag` | 1 |
| 裁剪左、右、上、下 | 0、0、0、4 |
| `num_units_in_tick` | 1 |
| `time_scale` | 60 |
| `fixed_frame_rate_flag` | 0 |

尺寸推导：

```text
coded_width  = (39 + 1) × 16 = 640
coded_height = (22 + 1) × 16 × (2 − 1) = 368

逐行 4:2:0：cropUnitX = 2，cropUnitY = 2

width  = 640 − (0 + 0) × 2 = 640
height = 368 − (0 + 4) × 2 = 360
```

本例时序比值为 60 ÷ (2×1) = 30，但 fixed_frame_rate_flag 为 0，不能仅靠这个 SPS 字段断言所有实际帧间隔都固定为 1/30 秒。

PPS 前部解析结果为：PPS ID 0、所引用 SPS ID 0、entropy_coding_mode_flag 为 0、bottom_field_pic_order_in_frame_present_flag 为 0。

## 10 PPS 与 Slice 前部的解析

### PPS 的主要路线

| 字段或分支 | 作用 |
|---|---|
| `pic_parameter_set_id` | PPS ID，范围为 0 到 255 |
| `seq_parameter_set_id` | 该 PPS 引用的 SPS |
| `entropy_coding_mode_flag` | 0 为 CAVLC，1 为 CABAC；还需符合 Profile 约束 |
| `bottom_field_pic_order_in_frame_present_flag` | 控制部分 Slice POC 语法 |
| `num_slice_groups_minus1` | 大于 0 时进入 Slice Group 相关分支 |
| 默认参考索引数量 | Slice 未覆盖时使用的默认值 |
| 加权预测控制 | 控制相应预测语法 |
| `pic_init_qp_minus26` 等 se 字段 | 初始量化与偏移配置 |
| 去块滤波、受限帧内、冗余图像控制 | 影响 Slice 语法或解码行为 |
| more_rbsp_data 对应扩展 | 8×8 变换标志、缩放矩阵及第二色度 QP 偏移等 |

遇到不支持的 Slice Group 分支，可以明确拒绝该流；不能只忽略该字段后继续按“无 Slice Group”解析。生产实现也应校验 PPS 与所引用 SPS 的组合。

### 从 Slice 找到 PPS

对这里讨论的普通 Type 1 或 Type 5 Slice，头部前三个语法元素为：

```text
first_mb_in_slice       ue(v)
slice_type              ue(v)
pic_parameter_set_id    ue(v)
```

读取第三个元素后，可先建立 PPS → SPS 的引用关系，再结合配置解释后续位宽与条件字段。这里的简化前缀不能直接推广到所有扩展 NAL 或数据分区语法。

日志中的 non-existing PPS 0 referenced，表示当前 Slice 引用了 ID 为 0 的 PPS，但接收端尚未建立对应配置；这个 0 不是“第 0 帧”。

## 11 RTP 与 WebRTC 中的参数集

### H.264 RTP 的常见封包

非交错 packetization-mode=1 支持单 NAL、STAP-A 和 FU-A。RTP 载荷本身不带 Annex B 起始码。

| 类型 | 载荷组织 | 接收处理 |
|---|---|---|
| 单 NAL | 一个完整 NAL | 直接交给 NAL 层 |
| STAP-A，Type 24 | 聚合头加多个“16 位长度与 NAL” | 对每项做长度与边界检查 |
| FU-A，Type 28 | FU indicator、FU header、片段数据 | 恢复 NAL 头并重组完整 NAL |

一个 STAP-A 可以包含 SPS 与 PPS。一个较大的 NAL 可以被拆成多个 FU-A；不能把 FU header 当作 SPS 字段解析。

FU-A 恢复原始头的计算：

```c
uint8_t nal_header = (fu_indicator & 0xE0) | (fu_header & 0x1F);
```

例如原始 IDR Slice 的 NAL 头为 0x65：

```text
开始片：FU indicator = 7c，FU header = 85
中间片：FU indicator = 7c，FU header = 05
结束片：FU indicator = 7c，FU header = 45

恢复头：(7c & e0) | (85 & 1f) = 65
```

恢复后的 NAL 头只写一次。片段数据中不能混入 FU indicator 或 FU header。应维护片段起止、所属流、时间戳和序列连续性；乱序恢复或 RTX 处理完成后，仍有缺口的 NAL 不能作为完整数据提交解码器。格式见 [RFC 6184 的聚合与分片规则](https://www.rfc-editor.org/rfc/rfc6184#section-5)。

### 时间戳和 Marker

H.264 RTP 时钟频率为 90000。30 fps、无额外时间变化的教学情形中，相邻图像时间戳增量为 90000÷30=3000。同一访问单元的多个 RTP 包通常使用同一图像时间戳。

Marker 按载荷规范标记访问单元的最后一个包，不代表“I 帧”。收到 Marker 也不能单独证明之前所有片段都已收到。

### RTSP 与 WebRTC 的区别

一般 H.264 RTP 会话可在 SDP 的 sprop-parameter-sets 中，用 Base64 传递完整 SPS/PPS NAL。Base64 数据不包含 Annex B 起始码，多个参数集可用逗号分隔。

WebRTC 的规则更具体：[RFC 7742](https://www.rfc-editor.org/rfc/rfc7742#section-6.2)要求 H.264 参数集在带内传递，生成的 SDP 不应包含 sprop-parameter-sets。不能把 RTSP 的 SDP 参数集模式直接套到浏览器 WebRTC。

下面只展示 WebRTC H.264 协商中的几行，不是完整 SDP，也不与本讲样本绑定：

```text
a=rtpmap:102 H264/90000
a=fmtp:102 profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1
```

profile-level-id 用于表达相关能力与约束，不能替代实际 SPS/PPS。接收端仍需要当前视频源对应的参数集。

## 12 RTC 接收端的缓存与配置切换

### 建议保存的数据

```text
每个视频源或解码上下文
├── SPS：按 sps_id 索引
│   ├── 原始 NAL 字节
│   ├── 解析后的配置
│   └── 内容版本或摘要
├── PPS：按 pps_id 索引
│   ├── 原始 NAL 字节
│   ├── 引用的 sps_id
│   └── 内容版本或摘要
└── 当前访问单元
    ├── 引用的 PPS 与 SPS 版本
    ├── 完整性与随机接入状态
    └── 时间戳和图像信息
```

缓存按媒体源或解码上下文隔离。不同 SSRC、Simulcast 层或 SFU 上游源可能复用同一个参数 ID；不能把所有流的 SPS 0 放进一个全局槽位。发生 SSRC 重写或上游切换时，还需要业务层维护源的对应关系。

### 收到数据后的处理路线

```text
RTP 接收、去重与乱序处理
        ↓
Depacketize，恢复完整 NAL
        ↓
识别参数集并校验，按源与 ID 缓存
        ↓
读取 Slice 引用，取得匹配的 PPS 与 SPS
        ↓
检查当前访问单元完整性与参考依赖
        ↓
交给解码器，处理解码器报告的格式变化
        ↓
更新渲染、缓冲区或硬件表面配置
```

同一个 ID 可以被新内容更新，不能只比较 ID 是否变化。分辨率没变，也可能存在其他配置变化。新参数集何时成为当前解码配置，应结合后续引用它的图像判断。

可以用下面三个应用状态检查流程是否完整。这是接收端设计示意，不是 H.264 或 FFmpeg 规定的枚举：

| 状态 | 已具备什么 | 接下来做什么 |
|---|---|---|
| 等待配置 | 还缺当前 Slice 引用的 PPS 或 SPS | 继续收集匹配参数；当前图像按有界缓存或丢弃策略处理 |
| 等待接入点 | 参数集完整，但尚未建立可用参考状态 | 等待适当的随机接入图像，必要时请求发送端刷新 |
| 正常解码 | 配置匹配、图像完整、参考依赖满足 | 持续提交访问单元，监测丢包、切流和配置变化 |

收到 SPS/PPS 只完成配置收集，并不自动表示已能解码任意 P/B 图像。参数内容改变后，重新检查配置与参考状态；缓存参数集时，还要保证原始 NAL 的内存在网络接收缓冲被复用后仍然有效。

解码器可能自动处理部分参数变化，也可能要求应用重新配置；按具体软件或硬件 API 处理，不应每收到一个重复 SPS 就无条件销毁解码器。

### 中途加入与切流

常见接入策略是：先取得匹配的参数集，再从可以正确建立参考状态的随机接入点开始解码。仅有 SPS/PPS 仍不足以解码任意一个依赖旧参考图像的 P/B 图像。

SFU 切换编码源时，应同时考虑参数集匹配、随机接入点、RTP 时间戳和序列映射。不能把旧源的 SPS/PPS 无条件补在新源关键帧前。

PLI 请求发送端进行图像恢复；NACK 请求重传特定丢包。二者的职责不同。参数集缺失时，要结合缓存、丢包情况与发送端刷新策略处理，不应对每个错误包无限发送 PLI。[RFC 4585](https://www.rfc-editor.org/rfc/rfc4585)描述了这些反馈机制。

## 13 FFmpeg 与项目代码的连接

### 从容器取得配置

FFmpeg 解复用后，通常通过 AVStream.codecpar 取得解码参数。将其复制到解码上下文，再打开解码器：

```c
/* 位于返回 int 错误码的初始化函数内。 */
int ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
if (ret < 0)
    return ret;

dec_ctx->pkt_timebase = st->time_base;
ret = avcodec_open2(dec_ctx, decoder, NULL);
if (ret < 0)
    return ret;
```

这里是初始化顺序示意。extradata 的格式取决于输入来源，不应假定一定是 Annex B；FFmpeg 对常见容器配置已有处理逻辑。

自建 RTP 解包链路则需要明确解码器期望的输入格式，按要求提供参数集和完整访问单元，避免把每个网络片段都当成一个可独立解码的 packet。

### 对照项目中的简化实现

[项目 h264encoder.cpp](D:/CodeRepositories/0VoiceAV/voice_av/09-ffmpeg-encoding-and-muxing/10-1-rtmp-publish/src/h264encoder.cpp)包含从 extradata 提取 SPS/PPS 的教学实现。

其中的“跳过 4 字节，然后寻找下一个四字节起始码”依赖特定输出布局。扩展到流媒体通用解析时，应改为：

1. 先识别配置记录或 Annex B。
2. 逐个枚举 NAL，再按 Type 7、8 分类。
3. 支持多个参数集，并按真实语法读取 ID。
4. 检查长度、截断和配置引用，不假定参数集顺序永远固定。

使用 GLOBAL_HEADER 后，参数集可能保存在内存中的 extradata。直接 fwrite packet 不会自动把这些配置写入裸流；同时也不能不判断 extradata 格式就直接前置。写 MP4、FLV 等容器时，应把配置交给相应复用器。

## 14 验证实验与排错顺序

### 生成测试流

以下命令使用合成图像，输出一个 H.264 访问单元，适合查看参数集：

```powershell
ffmpeg -f lavfi -i color=c=black:s=640x360:r=30 -frames:v 1 -an -c:v libx264 -profile:v baseline -level:v 3.0 -x264-params "keyint=30:bframes=0:scenecut=0" -f h264 sample_640x360.h264
```

### 按字段查看头部

```powershell
ffmpeg -hide_banner -i sample_640x360.h264 -c:v copy -bsf:v trace_headers -f null - 2>&1 | Select-String -Pattern "profile_idc|level_idc|parameter_set_id|pic_width|pic_height|frame_crop|num_units_in_tick|time_scale|fixed_frame_rate"
```

trace_headers 的字段可重复打印，例如分别处理 extradata 和访问单元时；重复日志不等于原始流一定有同样数量的独立配置。

从 MP4 转出常见 Annex B 字节流可使用：

```powershell
ffmpeg -i input.mp4 -map 0:v:0 -c:v copy -bsf:v h264_mp4toannexb output.h264
```

相关过滤器见 [FFmpeg Bitstream Filters](https://ffmpeg.org/ffmpeg-bitstream-filters.html)。

### 由外到内定位问题

| 现象 | 优先检查 |
|---|---|
| 找不到 SPS/PPS | 是否把 avcC 或 RTP 载荷误当作起始码裸流；是否漏了带内参数集 |
| 尺寸巨大或不合理 | ue 读取是否错位、High/POC 分支是否遗漏、裁剪单位是否错误 |
| non-existing PPS | Slice 引用的 PPS ID 是否缓存；是否使用了其他源的缓存 |
| 解出了参数集但仍 no frame | AU 是否完整、参考状态是否建立、是否从适当接入点开始 |
| 切流后持续花屏 | 源与配置版本是否匹配、旧参考图像和时间戳是否正确切换 |
| extradata 在调试器里看似空串 | 检查指针、长度和十六进制数据，开头 0x00 不代表长度为零 |

建议日志至少包含：流标识、NAL Type、NAL 长度、SPS/PPS ID、PPS 引用的 SPS ID、配置摘要、图像时间戳及解析错误原因。

位读取必须有边界，尺寸与长度运算必须避免溢出；对未实现的语法分支应明确拒绝或交给成熟解码库，不应用猜测值继续处理。

## 15 H.265 迁移时的差异

H.265 使用 2 字节 NAL 头，并增加 VPS。常见参数集 Type 为 VPS 32、SPS 33、PPS 34；类型提取为：

```c
uint8_t hevc_type = (nal[0] >> 1) & 0x3F;
```

H.265 的 SPS/PPS 语法与 H.264 不相同，RTP 聚合和分片格式也使用 [RFC 7798](https://www.rfc-editor.org/rfc/rfc7798)的定义。不能只修改 Type 数字后继续用 H.264 字段解析器。

可以复用的思想是：明确封装边界、恢复完整 NAL、校验位读取、按源与 ID 管理配置，以及在访问单元引用时应用对应配置。

## 16 自测题与参考答案

### 自测题

1. SPS NAL 以 00 00 01 67 开头，应该从哪个字节开始识别 NAL Type？
2. ue 位串 00101 的值是多少？se 对应的值是多少？
3. 逐行 4:2:0 视频编码尺寸为 1920×1088，裁剪下偏移为 4，其余为 0，结果尺寸是多少？
4. 新收到 SPS 1 后，一个 Slice 仍引用 PPS 0，而 PPS 0 引用 SPS 0，应该使用哪个 SPS？
5. RTP payload 首字节低五位为 28，能否直接当 SPS 解析？
6. RTSP SDP 中的 sprop-parameter-sets 能否原样作为 WebRTC SDP 生成规则？
7. FU-A 中间片缺失，但结束片带 Marker，能否提交完整 NAL？
8. 为什么切换上游源后，同样的 PPS ID 0 也可能导致解码异常？

### 参考答案

1. 去掉三字节起始码，从 0x67 读取 NAL 头；Type 为 7。SPS 的 RBSP 字段还需去掉该头并处理防竞争字节。
2. ue 为 4；se 对 codeNum 4 的映射为 −2。
3. cropUnitY 为 2，高度为 1088−4×2=1080，结果为 1920×1080。
4. 使用 SPS 0。由 Slice → PPS → SPS 的引用关系决定。
5. 不能。它表示 FU-A，先按分片语法恢复完整 NAL，再检查恢复后的真实 Type。
6. 不能。WebRTC 的 RFC 7742 要求参数集带内传递，生成的 SDP 不包含该参数。
7. 不能。Marker 不能填补片段缺口；应按丢包恢复策略处理。
8. 参数 ID 只在对应编码配置上下文中有意义，不同源可以复用相同 ID，但字节内容与语义不同。

## 17 参考资料

- [H.264 标准](https://www.itu.int/rec/T-REC-H.264-201610-S/en)：SPS、PPS、Slice、RBSP 和尺寸相关语法。
- [RFC 6184](https://www.rfc-editor.org/rfc/rfc6184)：H.264 RTP 封包、参数集与 SDP。
- [RFC 7742](https://www.rfc-editor.org/rfc/rfc7742)：WebRTC H.264 参数要求。
- [RFC 4585](https://www.rfc-editor.org/rfc/rfc4585)：NACK、PLI 等 RTP 反馈。
- [RFC 7798](https://www.rfc-editor.org/rfc/rfc7798)：H.265 RTP 与 NAL 组织。
- [FFmpeg SPS/PPS 解析](https://ffmpeg.org/doxygen/9.0/h264__ps_8c_source.html)：可对照的完整工程实现。
- [FFmpeg AVC 配置处理](https://ffmpeg.org/doxygen/trunk/avc_8c_source.html)：avcC 与 Annex B 的连接。
- [FFmpeg Bitstream Filters](https://ffmpeg.org/ffmpeg-bitstream-filters.html)：trace_headers、h264_mp4toannexb 等工具。

