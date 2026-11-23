#include "ffmpeg_channel_mask.h"
#include "pushwork.h"
#include "avtimebase.h"

namespace LQF {
PushWork::PushWork()
{

}

PushWork::~PushWork()
{
    if(audio_capturer_)
        delete audio_capturer_;
    if(audio_encoder_)
        delete audio_encoder_;
    if(video_capturer)
        delete video_capturer;
    if(audio_resampler_)
        delete audio_resampler_;
    if(video_encoder_)
        delete video_encoder_;
    if(rtsp_pusher)
        delete rtsp_pusher;

    if(video_out_sdl)
        delete video_out_sdl;

    if(h264_fp_)
        fclose(h264_fp_);
    if(aac_fp_)
        fclose(aac_fp_);
    if(pcm_flt_fp_)
        fclose(pcm_flt_fp_);
    if(pcm_s16le_fp_)
        fclose(pcm_s16le_fp_);
}
/**
 * @brief PushWork::Init
 * @param properties
 * @return
 */
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

    // 视频编码属性
    video_width_  = properties.GetProperty("video_width", desktop_width_);     // 宽
    video_height_ = properties.GetProperty("video_height", desktop_height_);   // 高
    video_fps_ = properties.GetProperty("video_fps", desktop_fps_);             // 帧率
    video_gop_ = properties.GetProperty("video_gop", video_fps_);
    video_bitrate_ = properties.GetProperty("video_bitrate", 1024*1024);   // 先默认1M fixedme
    video_b_frames_ = properties.GetProperty("video_b_frames", 0);   // b帧数量

    // rtmp推流
    rtsp_url_ = properties.GetProperty("rtsp_url", "");
    if(rtsp_url_ == "")
    {
        LogError("rtsp url is null");
        return RET_FAIL;
    }
    rtsp_debug_ = properties.GetProperty("rtsp_debug", 0);

    if(1 == rtsp_debug_)
    {
        video_out_sdl = new VideoOutSDL();  // 显示用的
        if(!video_out_sdl)
        {
            LogError("new VideoOutSDL() failed");
            return RET_FAIL;
        }
        Properties yuv_out_properties;
        yuv_out_properties.SetProperty("video_width", desktop_width_);
        yuv_out_properties.SetProperty("video_height", desktop_height_);
        yuv_out_properties.SetProperty("win_x", 200);
        yuv_out_properties.SetProperty("win_title", "push video display");
        if(video_out_sdl->Init(yuv_out_properties) != RET_OK)
        {
            LogError("video_out_sdl Init failed");
            return RET_FAIL;
        }
    }



    // 初始化publish time
    AVPublishTime::GetInstance()->Rest();   // 推流打时间戳的问题

    // 设置音频编码器，先音频捕获初始化
    audio_encoder_ = new AACEncoder();
    if(!rtsp_pusher)
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

    aac_fp_ = fopen("push_dump.aac", "wb");
    if(!aac_fp_)
    {
        LogError("fopen push_dump.aac failed");
        return RET_FAIL;
    }
    // 音频重采样，捕获的PCM数据 s16交错模式, 做编码的时候float棋盘格格式的
    // 1-快速掌握音视频开发基础知识.pdf
    audio_resampler_ = new AudioResampler();
    AudioResampleParams aud_params;
    aud_params.logtag = "[audio-resample]";
    aud_params.src_sample_fmt = (AVSampleFormat)mic_sample_fmt_;
    aud_params.dst_sample_fmt = (AVSampleFormat)audio_encoder_->get_sample_format();    // 从编码器读取
    aud_params.src_sample_rate = mic_sample_rate_;
    aud_params.dst_sample_rate = audio_encoder_->get_sample_rate();
    aud_params.src_channel_layout = default_channel_mask(mic_channels_);
    aud_params.dst_channel_layout = audio_encoder_->get_channel_layout();
    aud_params.logtag = "audio-resample-encode";
    audio_resampler_->InitResampler(aud_params);


    // 视频编码
    video_encoder_ = new H264Encoder();
    Properties  vid_codec_properties;
    vid_codec_properties.SetProperty("width", video_width_);
    vid_codec_properties.SetProperty("height", video_height_);
    vid_codec_properties.SetProperty("fps", video_fps_);            // 帧率
    vid_codec_properties.SetProperty("b_frames", video_b_frames_);
    vid_codec_properties.SetProperty("bitrate", video_bitrate_);    // 码率
    vid_codec_properties.SetProperty("gop", video_gop_);            // gop
    if(video_encoder_->Init(vid_codec_properties) != RET_OK)
    {
        LogError("H264Encoder Init failed");
        return RET_FAIL;
    }
    h264_fp_ = fopen("push_dump.h264", "wb");
    if(!h264_fp_)
    {
        LogError("fopen push_dump.h264 failed");
        return RET_FAIL;
    }


    // 设置音频pts的间隔
    double audio_frame_duration = 1000.0/audio_encoder_->get_sample_rate() *audio_encoder_->GetFrameSampleSize();
    LogInfo("audio_frame_duration:%lf", audio_frame_duration);
    AVPublishTime::GetInstance()->set_audio_frame_duration(audio_frame_duration);
    AVPublishTime::GetInstance()->set_audio_pts_strategy(AVPublishTime::PTS_RECTIFY);//帧间隔矫正
    // 设置视频pts的间隔
    double video_frame_duration = 1000.0 / video_encoder_->get_framerate();
    LogInfo("video_frame_duration:%lf", video_frame_duration);
    AVPublishTime::GetInstance()->set_video_pts_strategy(AVPublishTime::PTS_RECTIFY);//帧间隔矫正

    // 先启动RTSPPusher
    rtsp_pusher = new RTSPPusher();     // 启动后线程推流的线程就启动了，带了线程
    if(!rtsp_pusher)
    {
        LogError("new RTSPPusher() failed");
        return RET_FAIL;
    }

    if(!rtsp_pusher->Init(rtsp_url_))
    {
        LogError("rtsp_pusher->Init failed");
        return RET_FAIL;
    }
    // 推流的时候要设置对应的音视频编码器参数，我们直接从 编码器读取
    if(!rtsp_pusher->ConfigVideoSteam(video_encoder_->GetCodecContext()))
    {
        LogError("rtsp_pusher->ConfigVideoSteam failed");
        return RET_FAIL;
    }

    if(!rtsp_pusher->ConfigAudioSteam(audio_encoder_->GetCodecContext()))
    {
        LogError("rtsp_pusher->ConfigAudioSteam failed");
        return RET_FAIL;
    }

    if(!rtsp_pusher->Connect(0))
    {
        LogError("rtsp_pusher connect() failed");
        return RET_FAIL;
    }

    // 设置音频捕获
    audio_capturer_ = new AudioCapturer();
    Properties  aud_cap_properties;
    aud_cap_properties.SetProperty("audio_test", 1);
    aud_cap_properties.SetProperty("input_pcm_name", input_pcm_name_);
    if(audio_capturer_->Init(aud_cap_properties) != RET_OK)
    {
        LogError("AACEncoder Init failed");
        return RET_FAIL;
    }
    audio_capturer_->AddCallback(std::bind(&PushWork::PcmCallback, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));
    if(audio_capturer_->Start() != RET_OK)
    {
        LogError("AudioCapturer Start failed");
        return RET_FAIL;
    }


    video_capturer = new VideoCapturer();
    Properties  vid_cap_properties;
    vid_cap_properties.SetProperty("video_test", 1);
    vid_cap_properties.SetProperty("input_yuv_name", input_yuv_name_);
    vid_cap_properties.SetProperty("width", desktop_width_);
    vid_cap_properties.SetProperty("height", desktop_height_);
    if(video_capturer->Init(vid_cap_properties) != RET_OK)
    {
        LogError("VideoCapturer Init failed");
        return RET_FAIL;
    }
    video_nalu_buf = new uint8_t[VIDEO_NALU_BUF_MAX_SIZE];

    video_capturer->AddCallback(std::bind(&PushWork::YuvCallback, this,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    if(video_capturer->Start() != RET_OK)
    {
        LogError("VideoCapturer Start failed");
        return RET_FAIL;
    }
    return RET_OK;
}

void PushWork::DeInit()
{
    if(audio_capturer_) {
        delete audio_capturer_;
        audio_capturer_ = NULL;
    }
    if(audio_encoder_) {
        delete audio_encoder_;
        audio_encoder_ = NULL;
    }
    if(video_capturer) {
        delete video_capturer;
        video_capturer = NULL;
    }
    if(audio_resampler_) {
        delete audio_resampler_;
        audio_resampler_ = NULL;
    }
    if(video_encoder_) {
        delete video_encoder_;
        video_encoder_ = NULL;
    }
    if(rtsp_pusher) {
        delete rtsp_pusher;
        rtsp_pusher = NULL;
    }

    if(video_out_sdl) {
        delete video_out_sdl;
        video_out_sdl = NULL;
    }

    if(h264_fp_) {
        fclose(h264_fp_);
        h264_fp_ = NULL;
    }
    if(aac_fp_) {
        fclose(aac_fp_);
        aac_fp_ = NULL;
    }
    if(pcm_flt_fp_) {
        fclose(pcm_flt_fp_);
        pcm_flt_fp_ = NULL;
    }
    if(pcm_s16le_fp_) {
        fclose(pcm_s16le_fp_);
        pcm_s16le_fp_ = NULL;
    }
}

void PushWork::AudioCallback(NaluStruct *nalu_data)
{

}

void PushWork::VideoCallback(NaluStruct *nalu_data)
{

}

void PushWork::PcmCallback(uint8_t *pcm, int32_t size)
{
    if(!pcm_s16le_fp_)
    {
        pcm_s16le_fp_ = fopen("push_dump_s16le.pcm", "wb");
    }
    if(pcm_s16le_fp_)
    {
        // ffplay -ar 48000 -channels 2 -f s16le  -i push_dump_s16le.pcm
        //        fwrite(pcm, 1, size, pcm_s16le_fp_);
        //        fflush(pcm_s16le_fp_);
    }

    // 音频重采样
    auto ret = audio_resampler_->SendResampleFrame(pcm, size);
    if(ret <0)
    {   LogError("SendResampleFrame failed ");
        return;
    }
    vector<shared_ptr<AVFrame>> resampled_frames;
    ret = audio_resampler_->ReceiveResampledFrame(
                resampled_frames,
                audio_encoder_->GetFrameSampleSize());

    if(ret !=0)
    {
        LogWarn("ReceiveResampledFrame ret:%d\n",ret);
        return;
    }

    for(int i = 0; i < resampled_frames.size(); i++)
    {
        if(!pcm_flt_fp_)            // ffmpeg 内置的aac编码器，需要的格式都是float的 planar的格式
        {
            pcm_flt_fp_ = fopen("push_dump_flt.pcm", "wb");
        }
        if(pcm_flt_fp_)
        {
            // ffplay -ar 48000 -channels 2 -f f32le  -i push_dump_f32le.pcm
            //            fwrite(resampled_frames[i].get()->data[0], 1,
            //                    resampled_frames[i].get()->linesize[0], pcm_flt_fp_);
            //            fwrite(resampled_frames[i].get()->data[1], 1,
            //                    resampled_frames[i].get()->linesize[1], pcm_flt_fp_);
            //            fflush(pcm_flt_fp_);
        }
        // 封装带参考计数的缓存
        int64_t pts = AVPublishTime::GetInstance()->get_audio_pts();
        AVPacket *packet = audio_encoder_->Encode(resampled_frames[i].get(), pts);
        if(packet)
        {
            if(aac_fp_)
            {
                uint8_t adts_header[7];
                audio_encoder_->GetAdtsHeader(adts_header, packet->size);
                fwrite(adts_header, 1, 7, aac_fp_);
                fwrite(packet->data, 1, packet->size, aac_fp_);
                fflush(aac_fp_);
            }
            rtsp_pusher->Post(RTSP_BODY_AUD_RAW, packet);
            LogDebug("PcmCallback Post");
        }
    }
}

void PushWork::YuvCallback(uint8_t* yuv, int32_t size)
{
    if(video_out_sdl)
        video_out_sdl->Cache(yuv, size);

    // 进行编码
    int64_t pts = AVPublishTime::GetInstance()->get_video_pts();

    AVPacket *packet = video_encoder_->Encode(yuv, pts);
    if(packet){
        // 获取到编码数据
        rtsp_pusher->Post(RTSP_BODY_VID_RAW, packet);

        fwrite(packet->data, packet->size,1, h264_fp_);

        fflush(h264_fp_);
    }
}

void PushWork::Loop()
{
    LogInfo("Loop into");
    if(video_out_sdl)
        video_out_sdl->Loop();          // 目前一定要用debug模式
    LogInfo("Loop leave");
}
}

