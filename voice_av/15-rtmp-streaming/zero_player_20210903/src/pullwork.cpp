#include "pullwork.h"
#include "timeutil.h"
#include "avtimebase.h"
namespace LQF
{
PullWork::PullWork()
{

}

PullWork::~PullWork()
{
    LogWarn("audioq.packet_queue_abort");
    if(audioq.is_init()) {
        audioq.packet_queue_abort();
    }
    LogWarn("videoq.packet_queue_abort");
    if(videoq.is_init()) {
        videoq.packet_queue_abort();
    }
    pictq.frame_queue_abort();
    sampq.frame_queue_abort();
    LogWarn("delete rtmp_player");
    if(rtmp_player_)
        delete rtmp_player_;

    LogWarn("delete audio_out_sdl");
    if(audio_out_sdl_)
        delete audio_out_sdl_;

    LogWarn("delete audio_decode_loop");
    if(audio_decode_loop_)
        delete audio_decode_loop_;

    LogWarn("delete video_decode_loop");
    if(video_decode_loop_)
        delete video_decode_loop_;

    LogWarn("delete video_output_loop");
    if(video_output_loop_)
        delete video_output_loop_;

    LogWarn("delete avsync");
    if(avsync_)
        delete avsync_;
}

RET_CODE PullWork::Init(const Properties &properties)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond();

    PacketQueue::InitFlushPacket();

    memset(&audioq, 0, sizeof(audioq));
    memset(&videoq, 0, sizeof(videoq));
    memset(&sampq, 0, sizeof(sampq));
    memset(&pictq, 0, sizeof(pictq));

    // 音频队列
    if(audioq.packet_queue_init() != 0) {
        LogError("audioq.packet_queue_init failed");
        return RET_FAIL;
    }

    // 视频队列
    if(videoq.packet_queue_init() != 0) {
        LogError("videoq.packet_queue_init failed");
        return RET_FAIL;
    }

    // 音频采样帧队列
    if(sampq.frame_queue_init(&audioq, SAMPLE_QUEUE_SIZE, 1)!= 0) {
        LogError("sampq.frame_queue_init failed");
        return RET_FAIL;
    }
    // 视频图像帧队列
    if(pictq.frame_queue_init(&videoq, VIDEO_PICTURE_QUEUE_SIZE, 1)!= 0) {
        LogError("pictq.frame_queue_init failed");
        return RET_FAIL;
    }
    audioq.packet_queue_start();
    videoq.packet_queue_start();

    // rtmp拉流
    rtmp_url_ = properties.GetProperty("rtmp_url", "");
    if(rtmp_url_ == "")
    {
        LogError("rtmp url is null");
        return RET_FAIL;
    }

    memset(&src_video_frame_, 0, sizeof(VideoFrame));

    video_out_width_ = properties.GetProperty("video_out_width", 480);
    video_out_height_ = properties.GetProperty("video_out_height", 270);
    audio_out_sample_rate_ = properties.GetProperty("audio_out_sample_rate", 48000);
    max_cache_duration_ = properties.GetProperty("max_cache_duration", 500);

    network_jitter_duration_ = properties.GetProperty("network_jitter_duration", 100);
    accelerate_speed_factor_ = properties.GetProperty("accelerate_speed_factor", (float)1.2);

    src_video_frame_.format = AV_PIX_FMT_YUV420P;
    src_video_frame_.width =  video_out_width_;
    src_video_frame_.height =  video_out_height_;


    avsync_ = new AVSync();
    if(avsync_->Init(AV_SYNC_AUDIO_MASTER, &audioq.serial_, &videoq.serial_) != RET_OK)
    {
        LogError("AVSync Init failed");
        return RET_FAIL;
    }
    audio_decode_loop_ = new AudioDecodeLoop(&audioq, &sampq);
    if(!audio_decode_loop_)
    {
        LogError("new AudioDecodeLoop() failed");
        return RET_FAIL;
    }

    Properties aud_loop_properties;
    aud_loop_properties.SetProperty("max_cache_duration", max_cache_duration_);
    if(audio_decode_loop_->Init(aud_loop_properties)!= RET_OK)
    {
        LogError("audio_decode_loop_ Init failed");
        return RET_FAIL;
    }
    if(audio_decode_loop_->Start()!= RET_OK)
    {
        LogError("audio_decode_loop_ Start failed");
        return RET_FAIL;
    }

    // 初始化audio out相关
    audio_out_sdl_ = new AudioOutSDL(avsync_, &audioq, &sampq);
    if(!audio_out_sdl_)
    {
        LogError("new AudioOutSDL() failed");
        return RET_FAIL;
    }
    audio_out_sdl_->AddAudioCacheInfoCallback(std::bind(&PullWork::AudioCacheInfoCallback, this,
                                                        std::placeholders::_1,
                                                        std::placeholders::_2,
                                                        std::placeholders::_3));
    audio_out_sdl_->setFrame_enough__callback(std::bind(&PullWork::cacheEnoughCallback, this,
                                                        std::placeholders::_1));
    Properties aud_out_properties;
    aud_out_properties.SetProperty("sample_rate", audio_out_sample_rate_);  // 可以不设置，只是初始值，具体要看sdl是否支持
    aud_out_properties.SetProperty("channels", audio_out_sample_channels_);
    if(audio_out_sdl_->Init(aud_out_properties) != RET_OK)
    {
        LogError("audio_out_sdl Init failed");
        return RET_OK;
    }


    video_output_loop_ = new VideoOutputLoop(avsync_, &videoq, &pictq);
    Properties video_out_loop_properties;
    if(video_output_loop_->Init(video_out_loop_properties)!= RET_OK)
    {
        LogError("video_output_loop_ Init failed");
        return RET_FAIL;
    }
    //        video_output_loop_->AddAVSyncCallback(std::bind(&PullWork::avSyncCallback, this,
    //                                                        std::placeholders::_1,
    //                                                        std::placeholders::_2,
    //                                                        std::placeholders::_3));
    video_output_loop_->AddVideoRefreshCallback(std::bind(&PullWork::outVideoPicture, this,
                                                          std::placeholders::_1));
    if(video_output_loop_->Start() != RET_OK) {
        LogError("video_output_loop_ Start   failed");
        return RET_FAIL;
    }

    cur_time = TimesUtil::GetTimeMillisecond();


    // 初始化非常耗时，所以需要提前初始化好 有耗时到1秒
    LogInfo("%s:video_out_init:t:%lld",AVPlayTime::GetInstance()->getKeyTimeTag(),
            TimesUtil::GetTimeMillisecond() - cur_time);


    video_decode_loop_ = new VideoDecodeLoop(&videoq, &pictq);

    if(!video_decode_loop_)
    {
        LogError("new VideoDecodeLoop() failed");
        return RET_FAIL;
    }
    Properties vid_loop_properties;
    if(video_decode_loop_->Init(vid_loop_properties)!= RET_OK)
    {
        LogError("video_decode_loop_ Init failed");
        return RET_FAIL;
    }

    if(video_decode_loop_->Start() != RET_OK) {
        LogError("video_decode_loop_ Start   failed");
        return RET_FAIL;
    }
    AVPlayTime::GetInstance()->Rest();

    rtmp_player_ = new RTMPPlayer();
    if(!rtmp_player_)
    {
        LogError("new RTMPPlayer() failed");
        return RET_FAIL;
    }

    rtmp_player_->AddAudioInfoCallback(std::bind(&PullWork::AudioInfoCallback, this,
                                                 std::placeholders::_1,
                                                 std::placeholders::_2,
                                                 std::placeholders::_3));
    rtmp_player_->AddVideoInfoCallback(std::bind(&PullWork::VideoInfoCallback, this,
                                                 std::placeholders::_1,
                                                 std::placeholders::_2,
                                                 std::placeholders::_3));

    rtmp_player_->AddAudioPacketCallback(std::bind(&PullWork::audioPacketCallback, this,
                                                   std::placeholders::_1));
    rtmp_player_->AddVideoPacketCallback(std::bind(&PullWork::videoPacketCallback, this,
                                                   std::placeholders::_1));

    if(!rtmp_player_->Connect(rtmp_url_.c_str()))
    {
        LogError("rtmp_pusher connect() failed");
        return RET_FAIL;
    }
    rtmp_player_->Start();
    return RET_OK;
}
// 目前没有实际的作用
int PullWork::AudioInfoCallback(int what, MsgBaseObj *data, bool flush)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond();
    if(what == RTMP_BODY_AUD_SPEC)
    {

    }
    else
    {
        LogError("can't handle what:%d", what);
        delete data;
    }

    int64_t diff = TimesUtil::GetTimeMillisecond() - cur_time;
    if(diff>5)
        LogDebug("audioCallback t:%ld", diff);

    return 1;
}

int PullWork::VideoInfoCallback(int what, MsgBaseObj *data, bool flush)
{
    //    return;
    int64_t cur_time = TimesUtil::GetTimeMillisecond();

    if(what == RTMP_BODY_METADATA) {

        return 0;
    }


    int64_t diff = TimesUtil::GetTimeMillisecond() - cur_time;
    if(diff>5)
        LogInfo("videoCallback t:%ld", diff);

    return 1;
}

float PullWork::AudioCacheInfoCallback(float speed_factor, int packet_cache, int frame_cache)
{
    float new_speed_factor = 1.0;
    if(packet_cache_delay_ ++ > 3) {
        packet_cache_delay_ = 0;
        event_callback_(EVENT_UPDATE_AUDIO_CACHE_DURATION, packet_cache, frame_cache, 0, NULL);
        event_callback_(EVENT_UPDATE_VIDEO_CACHE_DURATION, videoq.duration(), pictq.duration(), 0, NULL);
        LogInfo("audio-speed_factor:%f, packet_cache:%d, frame_cache:%d", speed_factor, packet_cache, frame_cache);
        LogInfo("video-speed_factor:%f, packet_cache:%d, frame_cache:%d", speed_factor, videoq.duration(), pictq.duration());
    }

    if(packet_cache + frame_cache > max_cache_duration_ + network_jitter_duration_) {
        new_speed_factor = accelerate_speed_factor_;
    } else if (packet_cache + frame_cache < max_cache_duration_){
        new_speed_factor = 1.0;
    } else {
        new_speed_factor = speed_factor;
    }
    video_output_loop_->SetPlaybackRate(new_speed_factor);      // 设置视频的帧率
    return new_speed_factor;
}

int PullWork::audioPacketCallback(void *pkt)
{
    return audio_decode_loop_->Post((AVPacket *)pkt);
}

int PullWork::videoPacketCallback(void *pkt)
{
    // sps和pps一定要发送过去
    return video_decode_loop_->Post((AVPacket *)pkt);
}


int PullWork::outVideoPicture(const Frame *frame)
{
    // 显示视频
    video_refresh_callback_(frame);
    return 1;
}



int PullWork::avSyncCallback(double pts, double duration, double &get_diff)
{
    return avsync_->GetVideoSyncResult(pts, duration, get_diff);
}

void PullWork::setMax_cache_duration(int max_cache_duration)
{
    max_cache_duration_ = max_cache_duration;
    audio_decode_loop_->setMax_cache_duration(max_cache_duration);
}

void PullWork::setNetwork_jitter_duration(int network_jitter_duration)
{
    network_jitter_duration_ = network_jitter_duration;
}

void PullWork::setAccelerate_speed_factor(float accelerate_speed_factor)
{
    accelerate_speed_factor_ = accelerate_speed_factor;
}

void PullWork::cacheEnoughCallback(bool cache_enough)
{
    audio_decode_loop_->setCache_enough(cache_enough);
}
}
