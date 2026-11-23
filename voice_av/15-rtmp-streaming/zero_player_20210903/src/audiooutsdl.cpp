#include "audiooutsdl.h"
#include "dlog.h"
#include "timeutil.h"
#include "avsync.h"
#include "avtimebase.h"

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30


static int64_t audio_callback_time;
static int audio_decode_frame(AudioOutSDL *is)
{
    int data_size, resampled_data_size;
    AVChannelLayout dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;

    if (is->paused)
        return -1;

    do {
#if defined(_WIN32)
        while (is->frame_queue_->frame_queue_nb_remaining() == 0) {
            if ((av_gettime_relative() - audio_callback_time) > 1000000LL *
                    is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2)
                return -1;
            av_usleep (1000);
            //            LogInfo("av_usleep (1000);");
        }
#endif
        if (!(af = is->frame_queue_->frame_queue_peek_readable()))
            return -1;
        is->frame_queue_->frame_queue_next();
    } while (af->serial != is->packet_queue_->serial());

    data_size = av_samples_get_buffer_size(NULL, af->frame->ch_layout.nb_channels,
                                           af->frame->nb_samples,
                                           (enum AVSampleFormat)af->frame->format, 1);

    // Keep a native layout for SDL's channel order. Use the channel count
    // to choose the default layout for unordered or custom decoder layouts.
    if (af->frame->ch_layout.order == AV_CHANNEL_ORDER_NATIVE)
        dec_channel_layout = af->frame->ch_layout;
    else
        av_channel_layout_default(&dec_channel_layout, af->frame->ch_layout.nb_channels);
    wanted_nb_samples =  af->frame->nb_samples;

    if (af->frame->format        != is->audio_src.fmt            ||
            av_channel_layout_compare(&dec_channel_layout, &is->audio_src.channel_layout) ||
            af->frame->sample_rate   != is->audio_src.freq           ||
            (wanted_nb_samples       != af->frame->nb_samples && !is->swr_ctx)) {
        swr_free((SwrContext **)(&is->swr_ctx));
        int ret = swr_alloc_set_opts2(&is->swr_ctx,
                                      &is->audio_tgt.channel_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
                                      &dec_channel_layout, (AVSampleFormat)af->frame->format, af->frame->sample_rate,
                                      0, NULL);
        if (ret < 0 || swr_init(is->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                   af->frame->sample_rate,
                   av_get_sample_fmt_name((enum AVSampleFormat)af->frame->format),
                   af->frame->ch_layout.nb_channels,
                   is->audio_tgt.freq,
                   av_get_sample_fmt_name((enum AVSampleFormat)is->audio_tgt.fmt),
                   is->audio_tgt.channels);
            swr_free((SwrContext **)(&is->swr_ctx));
            return -1;
        }
        is->audio_src.channel_layout = dec_channel_layout;
        is->audio_src.channels       = af->frame->ch_layout.nb_channels;
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = (enum AVSampleFormat)af->frame->format;
    }

    if (is->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &is->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
        int out_size  = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count, is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
                                     wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1)
            return AVERROR(ENOMEM);
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            if (swr_init(is->swr_ctx) < 0)
                swr_free(&is->swr_ctx);
        }
        is->audio_buf = is->audio_buf1;
        resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
    } else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    audio_clock0 = is->audio_clock;
    /* update the audio clock with the pts */
    if (!isnan(af->pts))
        is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
    else
        is->audio_clock = NAN;
    is->audio_clock_serial = af->serial;

    return resampled_data_size;
}

static void fill_audio_pcm(void *udata, Uint8 *stream, int len)
{
    AudioOutSDL *is = (AudioOutSDL *)udata;
    int audio_size, len1;


    audio_callback_time = av_gettime_relative();
    uint8_t print_out_cout = is->GetPrintFrameOutputTimeCount();
    if(print_out_cout< is->GetMaxPrintFrameOutputTime()) {
        AVPlayTime *play_time = AVPlayTime::GetInstance();
        LogInfo("sdlout:%s:c:%u:t:%u", play_time->getAcodecTag(),
                print_out_cout,
                play_time->getCurrenTime());
    }


    while (len > 0) {   // 循环读取，直到读取到足够的数据
        /* (1)如果is->audio_buf_index < is->audio_buf_size则说明上次拷贝还剩余一些数据，
         * 先拷贝到stream再调用audio_decode_frame
         * (2)如果audio_buf消耗完了，则调用audio_decode_frame重新填充audio_buf
         */
        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_size = audio_decode_frame(is);
            if (audio_size < 0) {
                /* if error, just output silence */
                is->audio_buf = NULL;
                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size
                        * is->audio_tgt.frame_size;
                LogWarn("audio sdl out no data .................");
                is->frame_enough__callback_(false);
            } else {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;

            // 2 是否需要做变速
            if(is->playback_rate_change_)
            {
                is->playback_rate_change_ = false;
                // 初始化
                if(is->audio_speed_convert_)
                {
                    // 先释放
                    sonicDestroyStream(is->audio_speed_convert_);

                }
                // 再创建
                is->audio_speed_convert_ = sonicCreateStream(is->audio_tgt.freq,
                                                             is->audio_tgt.channels);

                // 设置变速系数
                sonicSetSpeed(is->audio_speed_convert_, is->cur_speed_factor);
                sonicSetPitch(is->audio_speed_convert_, 1.0);
                sonicSetRate(is->audio_speed_convert_, 1.0);
            }
            if(!is->IsNormalPlaybackRate() && is->audio_buf)
            {
                // 不是正常播放则需要修改
                // 需要修改  is->audio_buf_index is->audio_buf_size is->audio_buf
                int actual_out_samples = is->audio_buf_size/
                        (is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt));
                // 计算处理后的点数
                int out_ret= 0;
                int out_size = 0;
                int num_samples = 0;
                int sonic_samples = 0;
                if(is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT)  // sonic只支持2种格式
                {
                    out_ret = sonicWriteFloatToStream(is->audio_speed_convert_,
                                                      (float *)is->audio_buf,
                                                      actual_out_samples);
                }
                else  if(is->audio_tgt.fmt == AV_SAMPLE_FMT_S16)
                {
                    out_ret = sonicWriteShortToStream(is->audio_speed_convert_,
                                                      (short *)is->audio_buf,
                                                      actual_out_samples);
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "sonic unspport ......\n");
                }
                num_samples =  sonicSamplesAvailable(is->audio_speed_convert_);
                out_size = (num_samples) * av_get_bytes_per_sample(is->audio_tgt.fmt) *is->audio_tgt.channels;

                av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
                if(out_ret)
                {
                    // 从流中读取处理好的数据
                    if(is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT) {
                        sonic_samples = sonicReadFloatFromStream(is->audio_speed_convert_,
                                                                 (float *)is->audio_buf1,
                                                                 num_samples);
                    }
                    else  if(is->audio_tgt.fmt == AV_SAMPLE_FMT_S16)
                    {
                        sonic_samples = sonicReadShortFromStream(is->audio_speed_convert_,
                                                                 (short *)is->audio_buf1,
                                                                 num_samples);
                    }
                    else
                    {
                        LogError("sonic unspport ......");
                    }
                    is->audio_buf = is->audio_buf1;
                    //                    qDebug() << "mdy num_samples: " << num_samples;
                    //                    qDebug() << "orig audio_buf_size: " << is->audio_buf_size;
                    is->audio_buf_size = sonic_samples * is->audio_tgt.channels *av_get_bytes_per_sample(is->audio_tgt.fmt);
                    //                    qDebug() << "mdy audio_buf_size: " << is->audio_buf_size;
                    is->audio_buf_index = 0;
                }
            }
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        /* 判断是否为静音，以及当前音量的大小，如果音量不是最大则需要处理pcm数据 */
        if (!is->muted && is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
            memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        else {
            memset(stream, 0, len1);
            // 3.调整音量
            /* 如果处于mute状态则直接使用stream填0数据, 暂停时is->audio_buf = NULL */
            if (!is->muted && is->audio_buf)
                SDL_MixAudioFormat(stream, (uint8_t *)is->audio_buf + is->audio_buf_index,
                                   AUDIO_S16SYS, len1, is->audio_volume);
        }
        len -= len1;
        stream += len1;
        /* 更新is->audio_buf_index，指向audio_buf中未被拷贝到stream的数据（剩余数据）的起始位置 */
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    float new_speed_factor = is->audio_cache_info_callback_(is->cur_speed_factor,
                                                            is->packet_queue_->duration(),
                                                            is->frame_queue_->duration());
    if (fabs( new_speed_factor - is->cur_speed_factor) > 0.00001) {
        // 说明前后变速系数不一致
        if(fabs( new_speed_factor - 1.0) < 0.00001) {
            // 说明变速系数为正常情况
            is->cur_speed_factor = 1.0;
        } else {
            // 说明变速系数已经改变且不为1则触发变速
            is->cur_speed_factor = new_speed_factor;
            is->playback_rate_change_ = true;
        }
    }

    /* Let's assume the audio driver that is used by SDL has two periods. */
    if (!isnan(is->audio_clock)) {
        is->avsync_->audclk.set_clock_at(is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec,
                                         is->audio_clock_serial,
                                         audio_callback_time / 1000000.0);

        is->avsync_->extclk.sync_clock_to_slave(&is->avsync_->audclk);
    }
}

static int audio_open(void *opaque, AVChannelLayout wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
    AudioOutSDL *is = (AudioOutSDL *)opaque;
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        av_channel_layout_default(&wanted_channel_layout, wanted_nb_channels);
    }
    if (wanted_channel_layout.order != AV_CHANNEL_ORDER_NATIVE || wanted_nb_channels != wanted_channel_layout.nb_channels)
        av_channel_layout_default(&wanted_channel_layout, wanted_nb_channels);
    wanted_nb_channels = wanted_channel_layout.order == AV_CHANNEL_ORDER_NATIVE ? wanted_channel_layout.nb_channels : 0;
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        return -1;
    }
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    /*
     * 一次读取多长的数据
     * SDL_AUDIO_MAX_CALLBACKS_PER_SEC一秒最多回调次数，避免频繁的回调
     *  Audio buffer size in samples (power of 2)
     */
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = fill_audio_pcm;
    wanted_spec.userdata = opaque;
    while (!(is->audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
               wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR,
                       "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        av_channel_layout_default(&wanted_channel_layout, wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
               "SDL advised audio format %d is not supported!\n", spec.format);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        av_channel_layout_default(&wanted_channel_layout, spec.channels);
        if (wanted_channel_layout.order != AV_CHANNEL_ORDER_NATIVE) {
            av_log(NULL, AV_LOG_ERROR,
                   "SDL advised channel count %d is not supported!\n", spec.channels);
            return -1;
        }
    }
    // audio_hw_params保存的参数，就是在做重采样的时候要转成的格式。
    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels =  spec.channels;
    /* audio_hw_params->frame_size这里只是计算一个采样点占用的字节数 */
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    // 比如2帧数据，一帧就是1024个采样点， 1024*2*2 * 2 = 8096字节
    return spec.size;	/* 硬件内部缓存的数据字节, samples * channels *byte_per_sample */
}

AudioOutSDL::AudioOutSDL(AVSync *avsync, PacketQueue *packet_queue,
                         FrameQueue *frame_queue)
    :avsync_(avsync),
      packet_queue_(packet_queue),
      frame_queue_(frame_queue)
{
}

AudioOutSDL::~AudioOutSDL()
{
    LogInfo("~AudioOutSDL()");
    if(audio_buf1)
    {
        SDL_PauseAudio(1);
        // 关闭清理
        // 关闭音频设备
        SDL_CloseAudio();
        SDL_Quit();
        delete [] audio_buf1;
    }
}
RET_CODE AudioOutSDL::Init(const Properties &properties)
{
    if(!packet_queue_ || !frame_queue_) {
        LogError("packet_queue_ or frame_queue_ is null");
        return RET_FAIL;
    }
    sample_rate_ = properties.GetProperty("sample_rate", 48000);
    sample_fmt_ = properties.GetProperty("sample_fmt", AUDIO_S16SYS);
    channels_ = properties.GetProperty("channels", 2);

    audio_buf1 = new uint8_t[audio_buf1_size];  // 最大帧buffer
    //SDL initialize
    if(SDL_Init(SDL_INIT_AUDIO))    // 支持AUDIO
    {
        LogError("Could not initialize SDL - %s\n", SDL_GetError());
        return RET_FAIL;
    }

    AVChannelLayout channel_layout;
    av_channel_layout_default(&channel_layout, channels_);

    int ret = 0;
    /* prepare audio output 准备音频输出*/
    if ((ret = audio_open(this, channel_layout, channels_, sample_rate_, &audio_tgt)) < 0) {
        LogError("audio_open failed");
        return RET_FAIL;
    }
    audio_hw_buf_size = ret;
    audio_src = audio_tgt;
    //初始化audio_buf相关参数
    audio_buf_size  = 0;
    audio_buf_index = 0;


    //play audio
    SDL_PauseAudioDevice(audio_dev, 0);
    return RET_OK;
}

void AudioOutSDL::Release()
{

}

uint8_t AudioOutSDL::GetPrintFrameOutputTimeCount()
{
    return frame_ount_time_count++;
}

int AudioOutSDL::GetMaxPrintFrameOutputTime()
{
    return PRINT_MAX_FRAME_OUTPUT_TIME;
}


bool AudioOutSDL::IsNormalPlaybackRate()
{
    if(cur_speed_factor > 0.99 && cur_speed_factor < 1.01)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void AudioOutSDL::setFrame_enough__callback(const std::function<void (bool cache_enough)> &frame_enough__callback)
{
    frame_enough__callback_ = frame_enough__callback;
}

//// 声音输出需要的频率
//int AudioOutSDL::GetTargetFrequency()
//{

//    return audio_tgt.freq;

//}
//// 声音输出需要的通道数量
//int AudioOutSDL::GetTargetChannels()
//{
//    return audio_tgt.channels;
//}


