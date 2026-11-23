#include "ffmpeg_channel_mask.h"
#include <functional>
#include "pushwork.h"
#include "dlog.h"
#include "avpublishtime.h"

PushWork::PushWork()
{

}

RET_CODE PushWork::Init(const Properties &properties)
{
    // 音频test模式
    audio_test_ = properties.GetProperty("audio_test", 0);
    input_pcm_name_ = properties.GetProperty("input_pcm_name", "input_48k_2ch_s16.pcm");

    // 麦克风采样属性
    mic_sample_rate_ = properties.GetProperty("mic_sample_rate", 48000);
    mic_sample_fmt_ = properties.GetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
    mic_channels_ = properties.GetProperty("mic_channels", 2);

    // 音频编码参数
    audio_sample_rate_ = properties.GetProperty("audio_sample_rate", mic_sample_rate_);
    audio_bitrate_ = properties.GetProperty("audio_bitrate", 128*1024);
    audio_channels_ = properties.GetProperty("audio_channels", mic_channels_);
    audio_ch_layout_ = default_channel_mask(audio_channels_);    // 由audio_channels_决定



    // 视频test模式
    video_test_ = properties.GetProperty("video_test", 0);
    input_yuv_name_ = properties.GetProperty("input_yuv_name", "input_1280_720_420p.yuv");

    // 桌面录制属性
    desktop_x_ = properties.GetProperty("desktop_x", 0);
    desktop_y_ = properties.GetProperty("desktop_y", 0);
    desktop_width_  = properties.GetProperty("desktop_width", 1920);
    desktop_height_ = properties.GetProperty("desktop_height", 1080);
    desktop_format_ = properties.GetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
    desktop_fps_ = properties.GetProperty("desktop_fps", 25);


    // 初始化publish time
    AVPublishTime::GetInstance()->Rest();   // 推流打时间戳的问题

    // 设置音频编码器，先音频捕获初始化
    audio_encoder_ = new AACEncoder();
    if(!audio_encoder_)
    {
        LogError("new AACEncoder() failed");
        return RET_FAIL;
    }
    Properties  aud_codec_properties;
    aud_codec_properties.SetProperty("sample_rate", audio_sample_rate_);
    aud_codec_properties.SetProperty("channels", audio_channels_);
    aud_codec_properties.SetProperty("bitrate", audio_bitrate_);        // 这里没有去设置采样格式
    // 需要什么样的采样格式是从编码器读取出来的
    if(audio_encoder_->Init(aud_codec_properties) != RET_OK)
    {
        LogError("AACEncoder Init failed");
        return RET_FAIL;
    }

    int frame_bytes1 = 0;
    int frame_bytes2 = 0;
    // 默认读取出来的数据是s16的，编码器需要的是fltp, 需要做重采样
    // 手动把s16转成fltp
    frame_bytes1 = av_samples_get_buffer_size(NULL, audio_encoder_->GetChannels(),
                                              audio_encoder_->GetFrameSamples(),
                                              (enum AVSampleFormat)audio_encoder_->GetFormat(), 1);
    fltp_buf_ = (uint8_t *)av_malloc(frame_bytes1);

    audio_frame = av_frame_alloc();
    audio_frame->format = audio_encoder_->GetFormat();
    audio_frame->format = AV_SAMPLE_FMT_FLTP;
    audio_frame->nb_samples = audio_encoder_->GetFrameSamples();
    av_channel_layout_from_mask(&audio_frame->ch_layout, audio_encoder_->GetChannelLayout());
    frame_bytes2  = audio_encoder_->GetFrameBytes();
    if(frame_bytes1 != frame_bytes2) {
        LogError("frame_bytes1:%d != frame_bytes2:%d", frame_bytes1, frame_bytes2);
        return RET_FAIL;
    }
    if(avcodec_fill_audio_frame(audio_frame, audio_encoder_->GetChannels(),
                                (enum AVSampleFormat) audio_encoder_->GetFormat(),fltp_buf_, frame_bytes1, 0) < 0)
    {
        LogError("avcodec_fill_audio_frame failed");
        return RET_FAIL;
    }



    // 设置音频捕获
    audio_capturer_ = new AudioCapturer();
    Properties aud_cap_properties;
    aud_cap_properties.SetProperty("audio_test", 1);
    aud_cap_properties.SetProperty("input_pcm_name", input_pcm_name_);
    aud_cap_properties.SetProperty("channels", mic_channels_);
    aud_cap_properties.SetProperty("nb_samples", 1024);     // 由编码器提供 // fix me
    aud_cap_properties.SetProperty("format", mic_sample_fmt_);
    aud_cap_properties.SetProperty("byte_per_sample", 2);   // fix me
    if(audio_capturer_->Init(aud_cap_properties) != RET_OK)
    {
        LogError("AudioCapturer Init failed");
        return RET_FAIL;
    }

    audio_capturer_->AddCallback(std::bind(&PushWork::PcmCallback, this, std::placeholders::_1,
                                           std::placeholders::_2));

    if(audio_capturer_->Start()!= RET_OK) {
        LogError("AudioCapturer Start failed");
        return RET_FAIL;
    }

    video_capturer_ = new VideoCapturer();
    Properties  vid_cap_properties;
    vid_cap_properties.SetProperty("video_test", 1);
    vid_cap_properties.SetProperty("input_yuv_name", input_yuv_name_);
    vid_cap_properties.SetProperty("width", desktop_width_);
    vid_cap_properties.SetProperty("height", desktop_height_);
    if(video_capturer_->Init(vid_cap_properties) != RET_OK)
    {
        LogError("VideoCapturer Init failed");
        return RET_FAIL;
    }
    //    video_nalu_buf = new uint8_t[VIDEO_NALU_BUF_MAX_SIZE];

    video_capturer_->AddCallback(std::bind(&PushWork::YuvCallback, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));
    //    if(video_capturer_->Start()!= RET_OK) {
    //        LogError("VideoCapturer Start failed");
    //        return RET_FAIL;
    //    }

    return RET_OK;
}

RET_CODE PushWork::DeInit()
{
    if(audio_capturer_) {
        audio_capturer_->Stop();
        delete audio_capturer_;
        audio_capturer_ = NULL;
    }
    if(video_capturer_){
        video_capturer_->Stop();
        delete video_capturer_;
        video_capturer_ = NULL;
    }
    return RET_OK;
}
void PushWork::PcmCallback(uint8_t *pcm, int32_t size)
{
    int ret = 0;
    if(!pcm_s16le_fp_)
    {
        pcm_s16le_fp_ = fopen("push_dump_s16le.pcm", "wb");
    }
    if(pcm_s16le_fp_)
    {
        // ffplay -ar 48000 -channels 2 -f s16le  -i push_dump_s16le.pcm
        fwrite(pcm, 1, size, pcm_s16le_fp_);
        fflush(pcm_s16le_fp_);
    }

//    LogInfo("PcmCallback size:%d", size);
    int16_t *s16 = (int16_t *)pcm;
    float_t *fltp_l = (float_t *)fltp_buf_;
    float_t *fltp_r = (float_t *)(&fltp_buf_[1024*4]);
    memset(fltp_buf_, 0, 8192);
    for(int i = 0; i < 1024; i++) {
        // s16[0],s16[1];s16[2],s16[3];s16[4],s16[5];
        //        fltp[i]      =  s16[i + 0]*1.0 / 32768; // 左通道  *2是因为我们默认现在是2通道
        //        fltp[i] =  s16[i + 1]*1.0 / 32768; // 右通道
//        if(i % 300 == 0)
//            printf("i:%d, s:%d, %d, %f\n", i, (i+1024)*4, s16[i + 0], s16[i + 0]*1.0 / 32768);
        fltp_l[i] = s16[i + 0]*1.0 / 32768;
        fltp_r[i] = s16[i + 1]*1.0 / 32768;
    }
//    audio_frame->data[0] = fltp_buf_;
//    audio_frame->data[1] = fltp_buf_;
//    audio_frame->linesize[0] = 1024*4;
//    audio_frame->linesize[1] = 1024*4;
//    audio_frame->format = audio_encoder_->GetFormat();
//    audio_frame->format = AV_SAMPLE_FMT_FLTP;
//    audio_frame->nb_samples = audio_encoder_->GetFrameSamples();
//    audio_frame->ch_layout.nb_channels = audio_encoder_->GetChannels();
//    audio_frame->channel_layout = audio_encoder_->GetChannelLayout();
//    if((ret = avcodec_fill_audio_frame(audio_frame, audio_encoder_->GetChannels(),
//                                       (enum AVSampleFormat) audio_encoder_->GetFormat(),fltp_buf_, 8192, 0)) < 0)
//    {
//        char buf[1024] = { 0 };
//        av_strerror(ret, buf, sizeof(buf) - 1);
//        LogError("avcodec_fill_audio_frame failed:%s", buf);
////        LogError("avcodec_fill_audio_frame failed");
//        return ;
//    }
    int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_audio_pts();
    int pkt_frame = 0;
    RET_CODE encode_ret = RET_OK;
    AVPacket *packet = audio_encoder_->Encode(audio_frame, pts, 0, &pkt_frame, &encode_ret);
    if(encode_ret == RET_OK && packet) {
        if(!aac_fp_) {
            aac_fp_ = fopen("push_dump.aac", "wb");
            if(!aac_fp_) {
                LogError("fopen push_dump.aac failed");
                return;
            }
        }
        if(aac_fp_) {
            uint8_t adts_header[7];
            if(audio_encoder_->GetAdtsHeader(adts_header, packet->size) != RET_OK) {
                LogError("GetAdtsHeader failed");
                return;
            }
            fwrite(adts_header, 1, 7, aac_fp_);
            fwrite(packet->data, 1, packet->size, aac_fp_);
        }
    }
//    LogInfo("PcmCallback pts:%ld", pts);
//    if(packet) {
////        LogInfo("PcmCallback packet->pts:%ld", packet->pts);
//        av_packet_free(&packet);
//    }
}

void PushWork::YuvCallback(uint8_t *yuv, int32_t size)
{
    LogInfo("YuvCallback size:%d", size);
}
