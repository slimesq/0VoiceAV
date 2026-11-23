#include "FFmpegPlayer.h"

void *aync_stop(void *args) {

    FFmpegPlayer *ffmpeg = static_cast<FFmpegPlayer *>(args);
    ffmpeg->is_playing = false;
    //   等待prepare结束
    pthread_join(ffmpeg->pid_prepare, NULL);
    // 保证 start线程结束
    pthread_join(ffmpeg->pid_read, NULL);       // 数据不能再写入
                                                // 停止解码线程
                                                // 停止输出线程
    DELETE(ffmpeg->p_video_channel);
    DELETE(ffmpeg->p_audio_channel);
    LOG_D("ffmpeg->p_fmt_ctx");
    // 这时候释放就不会出现问题了
    if (ffmpeg->p_fmt_ctx) {
        //先关闭读取 (关闭fileintputstream)
        avformat_close_input(&ffmpeg->p_fmt_ctx);
        ffmpeg->p_fmt_ctx = NULL;
    }

    LOG_D("DELETE(ffmpeg)");
//    DELETE(ffmpeg);
    LOG_D("aync_stop done");
    return NULL;
}

/**
 * 被子线程调用的初始化FFmpeg，需要定义为全局函数
 */
void *prepare_thread(void *args) {
    auto *pFFmpeg = static_cast<FFmpegPlayer *>(args);
    pFFmpeg->prepareFFmpeg();
    return nullptr;
}

/**
 * 被子线程调用的获取数据包，需要定义为全局函数
 */
void *read_thread(void *args) {
    auto *pFFmpeg = static_cast<FFmpegPlayer *>(args);
    pFFmpeg->readPacket();
    return nullptr;
}

FFmpegPlayer::FFmpegPlayer(JavaCallHelper *javaCallHelper, const char *dataSource) {
    p_java_call_helper = javaCallHelper;
    // strlen函数返回字符串长度，计算规则是直到遇到字符串中末尾的'\0'
    url = new char[strlen(dataSource) + 1];// 这里 +1 也是因为最后以为是'\0'
    strcpy(url, dataSource);
}

FFmpegPlayer::~FFmpegPlayer() {
    LOG_D("析构MyFFmpeg");
}

/**
 * 子线程初始化FFmpeg
 */
void FFmpegPlayer::prepare() {
    // 参数1：线程id，pthread_t型指针
    // 参数2：线程的属性，nullptr默认属性
    // 参数3：线程创建之后执行的函数，函数指针，可以写&prepareFFmpeg，由于函数名就是函数指针，所以可以省略&
    // 参数4：prepareFFmpeg函数接受的参数，void型指针
    pthread_create(&pid_prepare, nullptr, prepare_thread, this);
    LOG_D("线程pid_prepare:%lu", pid_prepare);
}

/**
 * 初始化Fmpeg，运行在子线程
 */
void FFmpegPlayer::prepareFFmpeg() {
    // 1.初始化网络模块
//    avformat_network_init();
    memset(st_index, -1, sizeof(st_index));
    audio_stream = -1;
    video_stream = -1;
    // 总上下文，包含了视频、音频的各种信息
    p_fmt_ctx = avformat_alloc_context();

    AVDictionary *opts = nullptr;// 字典可以理解为HashMap
    av_dict_set(&opts, "timeout", "3000000", 0);// 设置超时时间为3秒

    int ret;// 返回结果

    // 2.打开输入视频文件
    // 第三个参数是输入文件的封装格式，可以强制指定AVFormatContext中AVInputFormat的。
    // 一般设置为nullptr，可以自动检测AVInputFormat
    LOG_D("avformat_open_input %s", url);
    ret = avformat_open_input(&p_fmt_ctx, url, nullptr, NULL);// 需要释放 avformat_close_input
    if (ret < 0) {
        char str_error[512] = {0};
        av_strerror(ret, str_error, sizeof(str_error) -1);
        LOG_E("无法打开输入视频文件%s, err:%s", url, str_error);
        if (p_java_call_helper != nullptr) {
            p_java_call_helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);
        }
        return;
    }
    LOG_D("avformat_find_stream_info");
    // 3.获取视频文件信息
    if (avformat_find_stream_info(p_fmt_ctx, nullptr) < 0) {
        if (p_java_call_helper != nullptr) {
            p_java_call_helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        return;
    }
    LOG_D("avcodec_find_decoder");
    st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(p_fmt_ctx, AVMEDIA_TYPE_VIDEO,
                                st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(p_fmt_ctx, AVMEDIA_TYPE_AUDIO,
                                st_index[AVMEDIA_TYPE_AUDIO], st_index[AVMEDIA_TYPE_VIDEO],  NULL, 0);
    audio_stream = st_index[AVMEDIA_TYPE_AUDIO];
    video_stream = st_index[AVMEDIA_TYPE_VIDEO];
    for (int i = 0; i < p_fmt_ctx->nb_streams; ++i) {
        if(i == audio_stream || i == video_stream) {
            // 创建解码器上下文
            AVCodecContext *avctx = avcodec_alloc_context3(NULL);
            ret = avcodec_parameters_to_context(avctx, p_fmt_ctx->streams[i]->codecpar);
            // 找到解码器
            AVCodec *dec = avcodec_find_decoder(avctx->codec_id);
            if (!dec) {
                if (p_java_call_helper != nullptr) {
                    p_java_call_helper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
                }
                LOG_D("FFMPEG_FIND_DECODER_FAIL codec_id:%d", avctx->codec_id);
                return;
            }

            if (!avctx) {
                if (p_java_call_helper != nullptr) {
                    p_java_call_helper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
                }
                LOG_D("FFMPEG_ALLOC_CODEC_CONTEXT_FAIL");
                return;
            }
            // 打开解码器
            if (avcodec_open2(avctx, dec, nullptr) < 0) {
                if (p_java_call_helper != nullptr) {
                    p_java_call_helper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
                }
                LOG_D("FFMPEG_OPEN_DECODER_FAIL");
                return;
            }

            if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                // 音频对应的AVStream
                AVStream *pStream = p_fmt_ctx->streams[i];
                p_audio_channel = new AudioChannel(i, p_java_call_helper, avctx, pStream->time_base);
                LOG_D("解析出来音频");
            } else if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                // 视频对应的AVStream
                AVStream *pStream = p_fmt_ctx->streams[i];
                // 视频帧率，每秒多少帧
                double fps = av_q2d(pStream->avg_frame_rate);
                LOG_D("帧率 = %f", fps);
                p_video_channel = new VideoChannel(i, p_java_call_helper, avctx, pStream->time_base);
                p_video_channel->setFPS(fps);// 设置帧率，目的是播放视频时，确定延迟时间 弱
                p_video_channel->setRenderCallback(render_frame);// 设置回调
            }
        }
    }

    if (p_audio_channel == nullptr && p_video_channel == nullptr) {
        // 音视频都没有
        if (p_java_call_helper != nullptr) {
            p_java_call_helper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        }
        LOG_D("FFMPEG_NOMEDIA");
        return;
    } else if (p_audio_channel != nullptr && p_video_channel != nullptr) {
        // 音视频都存在，需要赋值，进行音视频同步
        p_video_channel->p_audio_channel = p_audio_channel;
    }
    LOG_D("prepareFFmpeg done");
    if (p_java_call_helper != nullptr) {
        LOG_D("p_java_call_helper->onPrepare");
        p_java_call_helper->onPrepare(THREAD_CHILD);
    }
}

void FFmpegPlayer::start() {
    is_playing = true;
    LOG_D("FFmpegPlayer::start:%s p_audio_channel:%p, p_video_channel:%p", url, p_audio_channel, p_video_channel);
    if (p_audio_channel != nullptr) {
        LOG_D("p_audio_channel::start");
        p_audio_channel->start();
    }
    if (p_video_channel != nullptr) {
        LOG_D("p_video_channel::start");
        p_video_channel->start();
    }
    if (p_audio_channel != nullptr || p_video_channel != nullptr) {
        // 开启获取packet线程
        pthread_create(&pid_read, nullptr, read_thread, this);
        LOG_D("线程pid_read:%lu", pid_read);
    }
}

void FFmpegPlayer::stop() {
    // formatContext
//    pthread_create(&pid_stop, 0, aync_stop, this);
    aync_stop(this);
    LOG_D("pid_stop pid %ld", pid_stop);
//    pthread_join(pid_stop, NULL);
};

void FFmpegPlayer::readPacket() {
    int ret = 0;
    LOG_D("readPacket into");
    while (is_playing) {
        if ((p_audio_channel != nullptr && p_audio_channel->pkt_queue.size() > QUEUE_MAX)
            || (p_video_channel != nullptr && p_video_channel->pkt_queue.size() > QUEUE_MAX)) {
            if (ALL_LOG) {
                LOG_E("Packet queue sleep 10ms, Audio = %d, Video = %d",
                      p_audio_channel->pkt_queue.size(), p_video_channel->pkt_queue.size());
            }
            // 音频或视频Packet队列超过100个，需要减缓生产
            av_usleep(10 * 1000);// 睡眠10ms
            continue;
        }

        // 因为使用了队列，所以需要这里必须要多次开辟内存空间
        AVPacket *packet = av_packet_alloc();// 需要使用av_packet_free释放，在BaseChannel中
        // 从媒体中读取音频包、视频包
        ret = av_read_frame(p_fmt_ctx, packet);
//        LOG_D("av_read_frame ->>>>>>>");
        if (ret == 0) {
            // 将数据包加入队列
            if (p_audio_channel != nullptr && packet->stream_index == p_audio_channel->channelId) {
                p_audio_channel->pkt_queue.enQueue(packet);
            } else if (p_video_channel != nullptr &&
                       packet->stream_index == p_video_channel->channelId) {
                p_video_channel->pkt_queue.enQueue(packet);
            }
        } else if (ret == AVERROR_EOF) {
            // 读取完毕 但是不一定播放完毕
            if (p_video_channel->pkt_queue.empty() && p_video_channel->frame_queue.empty()
                && p_audio_channel->pkt_queue.empty() && p_audio_channel->frame_queue.empty()) {
                LOG_D("播放完毕");
                // TODO 需要停止队列？
                break;
            }
            // 因为seek的存在，就算读取完毕，依然要循环，去执行av_read_frame(否则seek了没用)
        } else {
            break;
        }
    }

    is_playing = false;
    if(p_audio_channel)
        p_audio_channel->stop();
    if(p_video_channel)
        p_video_channel->stop();
}

void FFmpegPlayer::setRenderCallback(RenderFrame renderFrame) {
    this->render_frame = renderFrame;
}
