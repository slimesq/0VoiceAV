#include "audiodecodeloop.h"
#include "avtimebase.h"
namespace LQF {
AudioDecodeLoop::AudioDecodeLoop(PacketQueue *packet_queue ,
                                 FrameQueue *frame_queue):
    packet_queue_(packet_queue), frame_queue_(frame_queue)
{

}

AudioDecodeLoop::~AudioDecodeLoop()
{
    request_exit_ = true;
    Stop();
    if(aac_decoder_)
        delete aac_decoder_;
}

RET_CODE AudioDecodeLoop::Init(const Properties &properties)
{
    if(!packet_queue_ || !frame_queue_) {
        LogError("packet_queue_ or frame_queue_ is null");
        return RET_FAIL;
    }
    max_cache_duration_ = properties.GetProperty("max_cache_duration", 500);

    aac_decoder_ = new AACDecoder();
    if(!aac_decoder_)
    {
        LogError("new AACDecoder() failed");
        return RET_ERR_OUTOFMEMORY;
    }
    Properties properties2;
    if(aac_decoder_->Init(properties2) != RET_OK)
    {
        LogError("aac_decoder_ Init failed");
        return RET_FAIL;
    }

    return RET_OK;
}

void AudioDecodeLoop::Loop()
{

    AVFrame *frame = av_frame_alloc();
    Frame *af = NULL;
    RET_CODE ret = RET_OK;
    while(true)
    {
        if(!cache_enough_) {
            if(packet_queue_->packet_wait_cache(1, max_cache_duration_) < 0) {
                request_exit_ = 1;
            }
            LogInfo("cache_enough duration:%lld", packet_queue_->duration());
            cache_enough_ = true;
        }
        if(request_exit_)
            break;
        do
        {
            if(request_exit_)
                break;
            ret = aac_decoder_->ReceiveFrame(frame);
            if (ret >= 0) {
                frame->pts = frame->best_effort_timestamp;
            }

            if (ret == RET_OK)           // 解到一帧数据
            {
                if(decode_frames_++ < PRINT_MAX_FRAME_DECODE_TIME) {
                    AVPlayTime *play_time = AVPlayTime::GetInstance();
                    LogInfo("%s:c:%u:s:%d:t:%u", play_time->getAcodecTag(),
                            decode_frames_, packet_queue_->get_nb_packets(),
                            play_time->getCurrenTime());
                }
                // 2. 获取可写Frame
                if (!(af = frame_queue_->frame_queue_peek_writable()))  // 获取可写帧
                {
                    request_exit_ = 1;
                    break;
                }
                LogDebug("decoder apts:%u", frame->pts);
                // 3. 设置Frame并放入FrameQueue
                af->pts = frame->pts / 1000.0;
                af->pos = frame->pkt_pos;
                af->serial = pkt_serial_;
                af->duration = frame->duration/1000;
                af->int_duration = aac_decoder_->GetFrameDuration();         // 只针对flv的时间戳

                av_frame_move_ref(af->frame, frame);
                frame_queue_->frame_queue_push(af->int_duration);
            }
        } while (ret == RET_OK);
        if(request_exit_)
            break;
        AVPacket pkt;
        if (packet_queue_->packet_queue_get(&pkt, 1, &pkt_serial_) < 0)
        {
            LogError("packet_queue_get failed");
            break;
        }
        if (pkt.data == flush_pkt.data) {//
            // when seeking or when switching to a different stream
            aac_decoder_->FlushBuffers(); //清空里面的缓存帧
            finished_ = 0;        // 重置为0
        } else if (pkt.data != NULL && pkt.size!=0)
        {
            if(aac_decoder_->SendPacket(&pkt) != RET_OK)
                LogError("SendPacket failed, which is an API violation.\n");
            av_packet_unref(&pkt);
        } else {
            LogWarn("pkt null");
        }
    }
    av_frame_free(&frame);
    LogWarn("Loop leave");
}

int AudioDecodeLoop::Post(void *pkt)
{
    int64_t duration = packet_queue_->duration();
    auto size = packet_queue_->get_nb_packets();
//    if(size > 50)
    {
        if(packet_cache_delay_++ > 3) {
            packet_cache_delay_ = 0; // 只是为了降低打印的频率
            LogInfo("audio cache %d packet lead to delay:%lldms \n", size, duration);
        }
    }
    return packet_queue_->packet_queue_put((AVPacket *)pkt);
}

void AudioDecodeLoop::setCache_enough(bool cache_enough)
{
    cache_enough_ = cache_enough;
}

void AudioDecodeLoop::setMax_cache_duration(int max_cache_duration)
{
    max_cache_duration_ = max_cache_duration;
}

}
