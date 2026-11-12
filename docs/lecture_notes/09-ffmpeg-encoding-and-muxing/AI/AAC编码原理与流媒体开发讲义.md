# AAC 编码原理与流媒体开发讲义

> 项目章节 09 · Markdown 讲义 · 2026-11-01
>
> 面向音视频编码、直播推流、录制、音视频同步及 RTC 网关开发。
>
> 学习主线：PCM 如何变成 AAC，损失发生在哪里，一个 AAC 访问单元代表多长时间，以及同一编码内容如何分别进入 ADTS、MP4、FLV 和 RTP。
>
> 正文以常见的 AAC-LC、每声道每访问单元 1024 个采样点为主要教学配置。960 点配置、HE-AAC、AAC-LD 和 AAC-ELD 需按实际配置分别处理，不能直接套用全部数字。

## 01 学习目标与概念边界

学完本讲，应能：

1. 区分 PCM、AAC 编码、AAC 访问单元与传输封装。
2. 解释 MDCT、心理声学、量化、比特分配和熵编码的职责。
3. 计算音频码率、访问单元时长、采样计数和时间戳。
4. 识别 AudioSpecificConfig 与 ADTS 头，并完成简单手算。
5. 将设备采集块正确整理成编码器需要的 PCM 帧。
6. 理解编码延迟、priming、封包和抖动缓冲各自的影响。
7. 识别 AAC 在直播链路与浏览器 WebRTC 链路中的不同角色。

AAC 是 Advanced Audio Coding，高级音频编码。常见 AAC 编码为有损压缩。线性 PCM 则直接表示数字音频采样值，通常未经压缩。AAC 解码为 PCM，只能重建编码中保留的信息，不能恢复已丢失的细节。

本讲描述常见编码器的基本工作路线。标准规定码流与解码语义，具体心理声学分析和编码决策可以因实现而不同。

| 阅读目的 | 章节 | 本阶段要回答的问题 |
|---|---|---|
| 理解压缩原理 | 02 至 07 | 声音怎样变成系数，为什么量化会有损 |
| 处理时长与封装 | 08 至 11 | 每个 AU 推进多少采样，ASC、ADTS 与 RTP 怎样组织 |
| 落地流媒体开发 | 12 至 15 | 延迟来自哪里，怎样凑帧、编码、封装与排错 |

如果当前目标只是理解“AAC 为什么能压缩”，先完成 02 至 07 节；ASC、ADTS 与 RTP 的位布局属于后面的工程应用。

## 02 从 PCM 理解数据量

PCM 的三个基本量是采样率、每个采样值的位数和声道数。

| 参数 | 含义 | 教学配置 |
|---|---|---|
| 采样率 | 每声道每秒的采样点数 | 48000 Hz |
| 采样位数 | 每个采样值的存储位数 | 16 bit 整数 |
| 声道数 | 独立音频声道数量 | 2，即立体声 |

忽略额外对齐和容器信息：

```text
PCM 码率 = 采样率 × 每个采样值位数 × 声道数
         = 48000 × 16 × 2
         = 1536000 bit/s
         = 1536 kb/s
```

若 AAC 的平均码率为 128 kb/s，二者码率之比为 1536÷128=12。它表示此教学配置的数据量关系，不表示所有 AAC 都固定压缩 12 倍。

在 16 bit 立体声 PCM 中，每声道 1024 个采样点包含 2048 个采样值，占 4096 字节，持续时间仍是 1024÷48000 秒。声道数影响数据量，不会把这段声音的时长翻倍。

### Packed 与 Planar

```text
交错立体声 PCM：L0 R0 L1 R1 L2 R2 ……
平面立体声 PCM：左平面 L0 L1 L2 ……；右平面 R0 R1 R2 ……
```

例如 s16 通常表示交错的 16 bit 整数样本，fltp 表示平面浮点样本。它们属于 PCM 数据表示，不能把“浮点 PCM”理解为已经压缩的 AAC。

FFmpeg 的 AVFrame.nb_samples 表示每声道采样点数。平面数据应按各平面和实际步长访问，不能假定 AVFrame 的所有有效样本都无间隙地串在一个地址后面。

## 03 AAC 编码的整体流程

```text
PCM
 │
 ├── 心理声学分析 ── 掩蔽阈值、允许误差、窗口选择等信息
 │                              │
 ▼                              ▼
分帧与加窗 → MDCT → 可选频谱及声道工具 → 缩放与量化 → 熵编码
                                                       │
                                                       ▼
                                         码流组织与必要的侧信息
                                                       │
                                                       ▼
                                                AAC 访问单元
```

核心思想是：根据声音内容及人耳特性，把有限比特分配到更容易听出差异的部分，并利用信号的相关性与符号统计继续减少数据。

码流不仅包含量化后的频谱系数，还需要窗口、缩放因子、声道和编码工具等必要信息，解码器才能按对应方式重建声音。[Fraunhofer 的感知音频编码介绍](https://www.iis.fraunhofer.de/content/dam/iis/de/doc/ame/conference/AES-17-Conference_mp3-and-AAC-explained_AES17.pdf)可用于对照这一总体路线。

## 04 分帧 加窗与 MDCT

### 时间域与频率域

PCM 描述声音随时间变化的采样值。频域表示则描述一小段声音中，各种频率成分的贡献。

MDCT 是 Modified Discrete Cosine Transform，修正离散余弦变换。它与视频中的 DCT 都属于变换编码工具，但 MDCT 使用相邻块重叠的处理方式。

变换后，系数的位置对应一类频率成分，系数值表示该成分的贡献。第一个系数不是“第一个 PCM 采样点变换后的新采样值”，而是对整个分析窗口进行计算得到的结果。

对常见的 1024 点 AAC-LC 长窗配置，可以这样理解：

```text
每次处理一个 2048 点的重叠窗口
每次向前推进 1024 个采样点
每个长窗得到 1024 个 MDCT 系数

窗口 0：[采样 0 .................................. 2047]
窗口 1：                  [采样 1024 ............................ 3071]
```

这些采样点数按每声道理解。1024 个新采样点的步进，与 2048 点分析窗口覆盖范围，是两个不同的量。[Apple 的 AAC 编码背景说明](https://developer.apple.com/documentation/quicktime-file-format/background_aac_encoding)解释了这种重叠结构。

### 2048 变成 1024 是否已经丢失一半信息

不能只看单个窗口的输入输出个数。相邻窗口彼此重叠，解码端通过对应的逆变换和重叠合成，在理想条件下抵消时域混叠。

主要不可逆失真来自后续量化及某些感知参数化处理；不能因为单窗系数数目减半，就把 MDCT 当成简单丢掉一半采样。

### 长窗与短窗

突发的鼓声、敲击声等瞬态，对时间分辨率要求较高。长窗量化误差可能在听感上影响突发声之前的区域，形成预回声等失真。

AAC-LC 可采用短窗方式：常见配置在一个访问单元内使用 8 个短窗，每个短窗产生 128 个系数，总计仍为 1024 个系数。窗口切换有相应的过渡方式。

**短窗提高局部时间分辨率，但没有把该访问单元代表的 1024 个新采样点改成 128 个。** 因此，不能据此把其传输时长直接除以 8。

## 05 心理声学与听觉掩蔽

人耳对不同频率、不同时间位置和不同强度的声音，并非具有完全相同的敏感度。一个较强的声音可能使附近的微弱声音难以被听到，这称为掩蔽。

| 现象 | 直观理解 | 对编码的影响 |
|---|---|---|
| 频率相关的掩蔽 | 强声音可能遮蔽邻近频率的弱声音 | 不同频带可允许不同误差 |
| 时间相关的掩蔽 | 瞬态前后，人耳对误差的敏感程度会变化 | 窗口及误差分布需要适应瞬态 |
| 听觉敏感度差异 | 不同频率的可听阈值不同 | 比特分配需要结合频率与能量等因素 |

编码器估计允许误差，再调整量化和比特分配，尽量使量化噪声不容易被听出。这里需要比较的是“量化引入的误差”与相应掩蔽阈值；不能只凭一个系数小，就认定把它置零一定听不出来。

目标码率过低或模型判断不合适时，误差仍可能变得可闻；不能保证所有压缩结果都听不出区别。

它也不是一条“所有高频都不重要”的规则。被掩蔽程度、内容和编码配置需要共同考虑。[Fraunhofer 的感知编码框架](https://www.iis.fraunhofer.de/content/dam/iis/de/doc/ame/conference/AES-106-Convention_MPEG-4_Low-Delay-based-on-AAC_AES4929.pdf)介绍了掩蔽阈值与量化之间的关系。

## 06 缩放 量化与比特分配

### 量化怎样产生损失

先用一个均匀量化的小例子理解误差。它是教学模型，不是 AAC 的实际量化公式：

```text
q = round(x / Δ)
重建系数 x̂ = q × Δ
误差 = x̂ − x

x：原始频谱系数
Δ：教学模型中的量化步长
q：送去进一步编码的量化整数
```

假设心理声学分析判断 A 频带更需要保护，B 频带可以容忍较多误差：

| 频带 | 原始系数 x | 教学步长 Δ | 量化整数 q | 重建系数 | 误差 |
|---|---|---|---|---|---|
| A | 12.14 | 0.1 | 121 | 12.1 | −0.04 |
| B | 0.34 | 1.0 | 0 | 0 | −0.34 |

这个例子展示两个结果：A 使用更细精度，重建误差较小；B 的系数变成了 0，原来的 0.34 无法从 0 中恢复。误差是否可闻，还要结合对应声音和掩蔽判断，本表不提供实际听感结论。

解码端可以恢复量化整数和量化配置，从而算出重建系数；它无法知道同一个量化区间中的原始系数究竟是多少。再做逆变换，得到的就是重建 PCM。

真实 AAC 采用非均匀量化，并用全局增益和各频带缩放因子调节精度。表中的 Δ 只用于解释取舍，不能直接替换 AAC 的量化和反量化算法。量化后产生的零值及常见符号组合，再交给后面的熵编码处理。

### 比特预算怎样影响精度

一个访问单元可使用的比特有限。编码器需要在频带之间分配预算：

- 对容易听出差异的部分分配更多精度。
- 对允许较多误差的部分使用较粗量化。
- 结合实际编码长度调整增益、缩放因子等配置。

普通目标码率或码率控制不意味着每个 AAC packet 必须等长。内容复杂度和比特调配会造成不同访问单元长度不同。

以 48 kHz、每声道每 AU 1024 点、128 kb/s 为例，理论平均负载为：

```text
每 AU 比特数 ≈ 128000 × 1024 ÷ 48000 = 2730.67 bit
每 AU 字节数 ≈ 341.33 byte
```

这只是平均预算估计，不包括所选传输封装的开销，也不能用来把每个包截成固定 341 字节。量化与频带编码可对照 [FFmpeg 的 AAC 系数编码实现](https://ffmpeg.org/doxygen/8.0/aaccoder_8c_source.html)。

## 07 声道与频谱工具

### M/S 立体声

当左右声道相似时，分别独立描述可能重复保存很多信息。Mid/Side 方式可用“共同部分”和“差异部分”进行表示。

下面使用便于理解的归一化约定，实际实现可采用其他等价缩放：

```text
M = (L + R) / 2
S = (L − R) / 2

L = M + S
R = M − S
```

如果 L 与 R 完全相同，S 就为 0，差异信息较少。这个变换本身不等于把立体声变成单声道；实际重建误差还取决于后续量化等处理。

### 工具选读 其他常见工具

| 工具 | 英文名称 | 作用与边界 |
|---|---|---|
| TNS | Temporal Noise Shaping | 通过频谱上的预测处理影响量化噪声的时间分布；不是麦克风回声消除 |
| PNS | Perceptual Noise Substitution | 对适合的类噪声频带使用参数化表示，解码端生成相应噪声 |
| Intensity Stereo | 强度立体声 | 对适合的频带使用共同谱和声道强度等参数，减少数据 |

这些工具的可用性和使用方式与编码类型、配置及编码器决策有关。感知参数化工具可能引入不可逆差异，因此不能把所有有损变化都简单归结为一个量化函数。[Fraunhofer 的 AAC 核心工具示意](https://www.iis.fraunhofer.de/content/dam/iis/de/doc/ame/wp/FraunhoferIIS_Technical-Paper_AAC-ELD-family.pdf)展示了这些环节。

### 霍夫曼编码

量化结果通过霍夫曼码表等机制进一步压缩，让常见符号组合使用较短的表示。该步骤属于无损统计编码，不会再额外改变已经量化的数值。

解码端需要先恢复这些符号，再反量化、处理相应工具并进行逆变换与重叠合成，得到 PCM。

## 08 AAC 家族与应用选择

| 名称 | 主要方向 | 开发时应注意 |
|---|---|---|
| AAC-LC | 常见音乐、直播和视频伴音 | 本讲主要配置；不要把其时长公式无条件推广到其他类型 |
| HE-AAC | AAC 核心结合 SBR 等工具，提高低码率效率 | 核心采样率、输出采样率和扩展信令需要分别解释 |
| HE-AAC v2 | 在 HE-AAC 基础上结合参数立体声等工具 | 声道与输出配置不能仅按最初几个字段猜测 |
| AAC-LD | 低算法延迟方向 | 编解码库和接收端需要支持对应类型 |
| AAC-ELD | 增强低延迟方向 | 不能把普通 AAC-LC 编码器改一个名称就视为支持 ELD |

SBR 是 Spectral Band Replication，频带复制；PS 是 Parametric Stereo，参数立体声。它们使用附加参数与重建工具，不是无损地恢复所有被省略的原始频谱。

低延迟 AAC 家族针对通信场景优化，但实际端到端延迟仍包含采集、编码、封包、网络、抖动缓冲和播放设备等环节。配置及应用方向见 [AAC-ELD 技术介绍](https://www.iis.fraunhofer.de/content/dam/iis/de/doc/ame/wp/FraunhoferIIS_Technical-Paper_AAC-ELD-family.pdf)。

## 09 访问单元 采样数与时间戳

### 1024 表示什么

对本讲主要的 AAC-LC 配置：

```text
每个 AU：每声道推进 1024 个新采样点

48 kHz：1024 ÷ 48000 ≈ 21.333 ms
44.1 kHz：1024 ÷ 44100 ≈ 23.220 ms
```

两个声道不会使时长翻倍。一个 AU 的字节长度可以变化，但在固定配置下，其代表的采样推进量通常固定。

HE-AAC、LD、ELD 或其他帧长配置可能不同。FFmpeg 开发应读取编码器实际 frame_size、解码输出 nb_samples 和相关配置，不应在所有路径写死 1024。

### FFmpeg 的采样时间基

常见编码设置为：

```c
enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
```

若采样率为 48000，每个时间戳刻度表示 1/48000 秒，输入每帧 1024 点时：

```text
输入帧 PTS：0、1024、2048、3072……
对应秒数：0、0.021333、0.042667、0.064……
```

输入 PTS 应累计编码器采样率域中的每声道样本数。声道数不参与该累计。重采样以后，要使用实际输出到编码器的样本数。

封装器可能采用另一个时间基。通常先完成 `avformat_write_header()`，再以复用器最终确定的 `out_stream->time_base` 转换 packet 时间戳，不能只依赖写头之前设置的值：

```c
av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
pkt->stream_index = out_stream->index;
/* 检查返回值并按复用器接口的所有权约定处理 pkt。 */
int ret = av_interleaved_write_frame(out_fmt_ctx, pkt);
```

编码器为 priming 产生的初始 packet 时间戳可能为负值，不能看到负值就无条件清零。音频这种初始偏移也不等于视频 B 帧的双向预测重排。

## 10 AudioSpecificConfig 与 ADTS

### 配置与媒体数据

| 名称 | 作用 |
|---|---|
| AAC AU | 一个可由相应音频配置解释的编码访问单元 |
| ASC | AudioSpecificConfig，描述 MPEG-4 音频对象、采样率、声道及扩展配置 |
| ADTS | Audio Data Transport Stream，为音频数据提供逐帧头部与边界等信息 |

ASC 是配置，ADTS 是一种字节流组织方式，二者不能互相当作同一段头部。

### ASC 的两字节教学例

对普通 AAC-LC、48 kHz、双声道、常见 1024 点配置，一个最小的配置示例是：

```text
ASC：11 90

二进制：00010 | 0011 | 0010 | 0 | 0 | 0
        AOT     频率索引 声道配置  后续三个标志
位数：  5       4       4       1   1   1
```

| 字段 | 结果 |
|---|---|
| audioObjectType | 2，AAC-LC |
| samplingFrequencyIndex | 3，48000 Hz |
| channelConfiguration | 2，双声道 |
| frameLengthFlag | 0，本例为常见 1024 点配置 |
| dependsOnCoreCoder | 0 |
| extensionFlag | 0 |

ASC 不保证永远只有两个字节。AOT 逃逸、显式采样率、SBR/PS 和其他专用配置会改变解析路径。channelConfiguration 为 0 时涉及 PCE 等额外配置；它是配置编号，不能对所有取值直接当作声道数。解析分支可对照 [FFmpeg 的 MPEG-4 Audio 配置解析](https://ffmpeg.org/doxygen/trunk/mpeg4audio_8c_source.html)。

### ADTS 头部

常见的无 CRC、单 raw_data_block ADTS 帧，头部为 7 字节；带相应 CRC 的常见单块形式为 9 字节。多块及错误保护结构还需要按专用语法处理。

下表只列关键字段，不是连续的完整位序表；中间还存在 ID、layer、private_bit 等字段。实现时应按完整语法读取，不能把表中各行直接首尾拼接。

| 重要字段 | 位数 | 含义 |
|---|---|---|
| syncword | 12 | 同步字 0xFFF |
| protection_absent | 1 | 1 表示没有相应 CRC |
| profile | 2 | 本例 AAC-LC 的 ADTS profile 值为 1 |
| sampling_frequency_index | 4 | 采样率索引，本例为 3 |
| channel_configuration | 3 | 分散存储在头部字节中，本例为 2 |
| aac_frame_length | 13 | 整个 ADTS 帧长度，包含头部和对应 CRC 等 |
| number_of_raw_data_blocks_in_frame | 2 | 字段值加 1，得到 raw_data_block 数量 |

对普通 AAC-LC，本例 ASC 的 AOT 为 2，ADTS profile 为 1。写头部时必须按定义映射，不能认为所有库的 profile 枚举、AOT 和 ADTS profile 数字都相同。

### 100 字节 AAC 负载的 ADTS 例子

以下是头部字段教学例，100 字节负载仅是假设长度，不表示任意 100 字节都是合法 AAC：

```text
AAC 负载长度：100 字节
ADTS 无 CRC 头：7 字节
aac_frame_length：107 字节

ADTS 头：ff f1 4c 80 0d 7f fc
```

长度重组：

```c
/* header 指向至少 7 字节已检查的 uint8_t 数据。 */
unsigned frame_length =
    ((header[3] & 0x03u) << 11) |
    (header[4] << 3) |
    (header[5] >> 5);
/* 本例结果为 107，不是 AAC 负载长度 100。 */
```

还需检查同步字、合法采样率索引、最小帧长、头部长度和缓冲区完整性。字节流可能在一个 ADTS 帧中间截断，不能把一次网络 read 或文件 read 的返回边界直接当作帧边界。[FFmpeg 的 ADTS 解析源码](https://ffmpeg.org/doxygen/trunk/adts__header_8c_source.html)可用于核对。

## 11 MP4 FLV 与 RTP 中的 AAC

### 同一编码内容采用不同封装

| 输出方向 | 常见组织 |
|---|---|
| ADTS .aac 字节流 | ADTS 头加 AAC 数据，逐帧排列 |
| MP4 或 M4A | sample 保存编码数据，解码配置中包含 ASC |
| 常见 AAC over FLV | AAC sequence header 传 ASC，媒体包传 AAC 数据 |
| MPEG4-GENERIC RTP | 按 RFC 3640 传输 AAC AU，配置可通过 SDP 表达 |

AAC 的实际采样率和声道应从 ASC 等有效配置解释，不应只凭某个容器的简化或历史字段猜测。

从 ADTS 转入 MP4/M4A 时，可使用 aac_adtstoasc 提取配置并移除 ADTS 头。该步骤属于码流或封装转换，不重新量化声音。FFmpeg 在部分复用场景会自动插入该过滤器。[FFmpeg 过滤器说明](https://ffmpeg.org/ffmpeg-bitstream-filters.html#aac_adtstoasc)

### AAC-hbr RTP 的单 AU 例子

以协商 sizeLength=13、indexLength=3、indexDeltaLength=3 的 AAC-hbr 配置为例，单 AU 载荷包含：

```text
AU-headers-length：16 位，说明后面的 AU header 共占多少“位”
AU header        ：13 位 AU-size 加 3 位 AU-Index
AAC 数据          ：AU-size 指定的字节
```

假设一个完整 AAC AU 长度为 100 字节、AU-Index 为 0：

```text
AU-headers-length = 16 bit → 00 10
AU header = (100 << 3) | 0 → 03 20

RTP payload：
00 10 03 20 | 100 字节 AAC AU
```

这里 00 10 表示 16 位，物理长度为 2 字节；AU-size 为 AAC 数据的字节数，不包含这 4 字节 AU 头部，也不包含额外 ADTS 头。该组织方式不能与 MP4 的 sample 或 ADTS 帧头混用。

其他协商配置、多 AU、分片或交错会改变处理逻辑，应按协商参数和 [RFC 3640](https://www.rfc-editor.org/rfc/rfc3640.html)实现，不能永远固定跳过 4 字节。

### RTP 时钟与音视频同步

对这里讨论的 48 kHz AAC-LC 配置，RTP 时钟使用会话声明的 48000 Hz。每个包携带一个 1024 点 AU 时，相邻 RTP 时间戳通常增加 1024；一个包携带两个连续 AU 时，下一包起点相应推进 2048。

声音的设备采集时钟、FFmpeg 时间基、RTP 时钟和接收端播放时钟需要建立明确映射。不能直接拿音频的 RTP 数字与视频 90000 Hz 的 RTP 数字比较；跨媒体同步还需要公共时间关系，例如 RTCP Sender Report 提供的 RTP 与 NTP 映射。[RFC 3550](https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1)描述了该映射。

## 12 面向流媒体与 RTC 的延迟分析

端到端延迟可以拆成：

```text
设备采集
  + 凑齐编码输入样本的等待
  + 编码器算法与内部缓冲
  + AU 聚合和封包等待
  + 网络传输
  + 接收端抖动缓冲
  + 解码与播放设备缓冲
```

1024÷48000≈21.333 ms 是本例一个 AU 的采样推进时长，不是整个编码延迟，也不是端到端延迟。如果为了省包头把多个 AU 聚成一包，还会增加等待时间。

### Priming 与尾部补齐

重叠变换和编码器实现会产生 priming，即起始准备样本；末尾也可能需要补齐到完整编码帧。这会影响精确播放长度和音视频同步。

不要把所有 AAC 编码器的起始补偿都硬编码为 1024 或 2112。应结合编码器报告、容器编辑信息及 FFmpeg 的 initial_padding、skip-samples 等相关信息处理。ADTS 字节流未必保留与容器相同的精确裁剪信息。[Apple 的编码延迟说明](https://developer.apple.com/documentation/quicktime-file-format/background_aac_encoding)给出了具体例子。

### 浏览器 WebRTC 的音频编码选择

AAC 常用于直播分发、录制和媒体文件。浏览器 WebRTC 的基础互操作要求采用另一套约束：[RFC 7874](https://www.rfc-editor.org/rfc/rfc7874.html#section-3)要求实现 Opus 和 G.711 等基础音频能力，AAC 不在其必选音频编解码器集合中。

因此，不能因为推流链路可以传 AAC，就假定浏览器 WebRTC 会接受 AAC。做 RTC 到直播或录制的网关时，应确认双方实际能力；需要转码时，处理 Opus/AAC 的采样重组、编码延迟与时间戳映射，而不是只改 RTP Payload Type。

## 13 FFmpeg 编码链路与项目代码

### 从采集回调到编码输入

设备每次可能交出 10 ms 的 PCM。48 kHz 下它是每声道 480 点，而本例 AAC 编码输入通常需要每声道 1024 点。

```text
设备 PCM
   ↓
格式转换与重采样，例如 s16 → fltp
   ↓
音频 FIFO，按实际输出样本数累积
   ↓
每声道取 frame_size 个样本
   ↓
AVFrame，设置 nb_samples、声道布局与 PTS
   ↓
avcodec_send_frame
   ↓
avcodec_receive_packet，循环接收当前可用输出
   ↓
按目标容器或协议封装
```

以每次交付 480 点、编码帧长 1024 为例，暂时忽略重采样延迟与其他缓冲：

| 回调次数 | 加入本次 480 点后的可用点数 | 本次送入编码器 | FIFO 剩余 |
|---|---|---|---|
| 1 | 480 | 尚不足一帧 | 480 |
| 2 | 960 | 尚不足一帧 | 960 |
| 3 | 1440 | 取出 1024 点送入 | 416 |
| 4 | 896 | 尚不足一帧 | 896 |
| 5 | 1376 | 再取出 1024 点送入 | 352 |

表中点数都按每声道计。它说明采集块和编码帧可以有不同边界，并不说明编码器需要等待“三个 AAC 帧”。

一次 send 也不保证立即返回一个 packet。输入采样率不同、重采样器有延迟时，应按实际重采样输出数更新 FIFO，不能机械地给每个回调记 480 个输出样本。

### 初始化与时间戳检查

打开编码器前，应设置并确认支持的采样率、声道布局、sample_fmt、码率和时间基。打开成功后读取 frame_size；支持可变帧长的编码器还需要按其能力处理。

采样计数示意：

```text
next_pts 初始为 0
每次从编码器采样率域的 FIFO 取出 N 个每声道样本：
    frame.nb_samples = N
    frame.pts = next_pts
    next_pts += N
```

末尾不足一帧时，按编码器能力和输出格式处理最后样本与补齐，再发送空帧排空编码器。若补了零，还要考虑如何记录真实结束位置，避免把所有补齐样本当成原始内容。

### 对照项目代码

- [音频编码示例 main.c](D:/CodeRepositories/0VoiceAV/voice_av/09-ffmpeg-encoding-and-muxing/01-encode-audio/main.c)：查看 sample_rate、time_base、frame_size、nb_samples 和 ADTS 输出。
- [推流 AACEncoder](D:/CodeRepositories/0VoiceAV/voice_av/09-ffmpeg-encoding-and-muxing/10-1-rtmp-publish/src/aacencoder.cpp)：查看 fltp、声道布局、编码器配置和 ADTS 头生成。
- [RTMP 推流封装](D:/CodeRepositories/0VoiceAV/voice_av/09-ffmpeg-encoding-and-muxing/10-1-rtmp-publish/src/rtmppusher.cpp)：连接编码配置、容器流参数与时间戳。

阅读手写 ADTS 头时，重点核对 Profile/AOT 映射、采样率索引、声道配置和总帧长。GLOBAL_HEADER 是配置方式之一，不能把它当作所有 AAC 编码器“packet 一定带或不带 ADTS”的通用格式检测器。

标准 AAC 编码器与可选第三方编码器支持的格式和选项可能不同，可用 `ffmpeg -h encoder=aac` 检查当前构建。现代 FFmpeg 的常规 AAC 使用也不应无条件照抄旧教程的 experimental 开关。

## 14 可以复现的实验与排错

### 生成 AAC-LC ADTS

```powershell
ffmpeg -f lavfi -i sine=frequency=1000:sample_rate=48000 -t 5 -ac 2 -c:a aac -profile:a aac_low -b:a 128k -f adts demo.aac
```

### 不重新编码地转为 M4A

```powershell
ffmpeg -i demo.aac -c:a copy -bsf:a aac_adtstoasc demo.m4a
```

这会改变数据的封装组织，并从 ADTS 取得配置；不会把 AAC 变成无损音频，也不会修复此前的量化损失。

### 查看采样配置和 packet

```powershell
ffprobe -v error -select_streams a:0 -show_entries stream=codec_name,profile,sample_rate,channels,channel_layout,time_base -of default=noprint_wrappers=1 demo.m4a
ffprobe -v error -select_streams a:0 -read_intervals "%+#5" -show_packets -show_entries packet=pts,pts_time,duration,duration_time,size -of compact demo.m4a
```

在常见 LC 配置下，检查 packet duration 是否对应每声道 1024 个采样点；仍需结合文件实际时间基换算，不能只看一个整数。

本讲的 ASC 11 90、100 字节负载 ADTS 总长 107，以及 AAC-hbr 的 00 10 03 20 均可按前文位布局独立复算。使用 FFmpeg 7.1.1 生成的 48 kHz 双声道样本，其首个 ADTS 头为 ff f1 4c 80 22 1f fc，总长 272 字节；不同编码运行不保证包长相同。

| 现象 | 优先检查 |
|---|---|
| 播放噪声、声道错乱 | PCM 的整数或浮点格式、交错或平面布局、声道顺序 |
| 音调或时长错误 | 实际采样率与声明采样率是否一致 |
| 解码器无法初始化 raw AAC | ASC 是否存在且与 AU 匹配 |
| ADTS 错位或解码失败 | 总长度是否包含头部、CRC 和多块情况是否正确处理 |
| 时间戳推进过快 | 是否误将声道数乘入 nb_samples 或 PTS |
| 音画逐渐漂移 | 采集时钟、重采样样本计数、时间基转换和同步映射 |
| 开头或尾部多静音 | priming、补齐及容器裁剪信息是否正确保留 |
| 接入 WebRTC 后无声 | 双方是否协商该编码，是否误把 AAC 作为必选 WebRTC 音频 |

## 15 自测题与参考答案

### 自测题

1. 48 kHz、16 bit、双声道线性 PCM 的码率是多少？
2. 每声道 1024 点、双声道的 AAC-LC 访问单元，在 48 kHz 下持续多久？
3. 一个 AAC-LC AU 使用 8 个短窗，是否表示它的时长只剩原来的八分之一？
4. ASC 11 90 的 AOT、采样率和声道配置分别是什么？
5. 100 字节 AAC 数据加上无 CRC 的常见 ADTS 头，aac_frame_length 应是多少？
6. AAC-hbr 头部 00 10 表示后续 AU header 占 16 字节还是 16 位？
7. 两声道各 1024 点输入，time_base=1/48000，下一帧 PTS 应增加 1024 还是 2048？
8. 将 AAC 解码为 PCM，再保存成 WAV，是否就恢复了无损原音？
9. 设备一次回调 480 点，能否不做重组就固定每回调发送一个 1024 点 AVFrame？
10. 为什么 AAC 推流可用，不能据此保证浏览器 WebRTC 也能接收？

### 参考答案

1. 48000×16×2=1536000 bit/s，即 1536 kb/s。
2. 1024÷48000≈21.333 ms，声道数不使时间翻倍。
3. 不是。本例 8×128 个系数仍组织在一个推进 1024 点的 AU 中。
4. AOT=2，即 AAC-LC；采样率索引 3，即 48000 Hz；声道配置 2，即双声道。
5. 107 字节，包含 7 字节头。
6. 16 位，即 2 字节；前面的长度字段本身也是 2 字节。
7. 增加 1024，按每声道采样推进量累计。
8. 不能。PCM 保存的是 AAC 解码后的重建结果，原先丢失的信息没有恢复。
9. 不能。需要维护真实样本数量，经格式转换、重采样和 FIFO 整理后再按编码器能力送入。
10. AAC 不属于 RFC 7874 的基础必选音频编码集合，必须确认实际协商能力，必要时转码并重新建立时间关系。

## 16 参考资料

- [Fraunhofer MP3 and AAC Explained](https://www.iis.fraunhofer.de/content/dam/iis/de/doc/ame/conference/AES-17-Conference_mp3-and-AAC-explained_AES17.pdf)：感知编码、量化与熵编码。
- [Fraunhofer AAC-ELD 技术介绍](https://www.iis.fraunhofer.de/content/dam/iis/de/doc/ame/wp/FraunhoferIIS_Technical-Paper_AAC-ELD-family.pdf)：AAC 核心工具与低延迟家族。
- [Apple AAC Encoding Background](https://developer.apple.com/documentation/quicktime-file-format/background_aac_encoding)：重叠变换、priming 和尾部补齐。
- [FFmpeg AAC 系数编码](https://ffmpeg.org/doxygen/8.0/aaccoder_8c_source.html)：量化、码表与编码成本。
- [FFmpeg MPEG-4 Audio 配置](https://ffmpeg.org/doxygen/trunk/mpeg4audio_8c_source.html)：ASC 和扩展配置解析。
- [FFmpeg ADTS 头解析](https://ffmpeg.org/doxygen/trunk/adts__header_8c_source.html)：ADTS 字段与长度。
- [RFC 3640](https://www.rfc-editor.org/rfc/rfc3640.html)：MPEG4-GENERIC RTP 与 AAC-hbr。
- [RFC 3550](https://www.rfc-editor.org/rfc/rfc3550)：RTP、RTCP 与时间映射。
- [RFC 7874](https://www.rfc-editor.org/rfc/rfc7874.html)：WebRTC 音频编码基础要求。
- [FFmpeg Bitstream Filters](https://ffmpeg.org/ffmpeg-bitstream-filters.html)：aac_adtstoasc。
- [FFmpeg AAC 编码器文档](https://ffmpeg.org/ffmpeg-codecs.html#aac)：原生 AAC 编码器选项。

