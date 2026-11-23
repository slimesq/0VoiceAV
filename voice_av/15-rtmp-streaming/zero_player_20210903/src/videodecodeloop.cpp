#include "VideoDecodeLoop.h"
#include "avtimebase.h"
namespace LQF
{
VideoDecodeLoop::VideoDecodeLoop(PacketQueue *packetq, FrameQueue *frameq)
    :pkt_queue_(packetq), frame_queue_(frameq)
{

}

VideoDecodeLoop::~VideoDecodeLoop()
{
    request_exit_ = true;
    Stop();
    if(h264_decoder_)
        delete h264_decoder_;
    if(yuv_buf_)
        delete [] yuv_buf_;
}

RET_CODE VideoDecodeLoop::Init(const Properties &properties)
{
    if(!pkt_queue_ || !frame_queue_) {
        LogError("pkt_queue_ or frame_queue_ is null");
        return RET_FAIL;
    }
    h264_decoder_ = new H264Decoder();
    if(!h264_decoder_)
    {
        LogError("new H264Decoder() failed");
        return RET_ERR_OUTOFMEMORY;
    }
    Properties properties2;
    if(h264_decoder_->Init(properties2) != RET_OK)
    {
        LogError("aac_decoder_ Init failed");
        return RET_FAIL;
    }
    yuv_buf_ = new uint8_t[YUV_BUF_MAX_SIZE];
    if(!yuv_buf_)
    {
        LogError("yuv_buf_ new failed");
        return RET_ERR_OUTOFMEMORY;
    }
    if(!pkt_queue_)
    {
        LogError("PacketQueue new failed");
        return RET_ERR_OUTOFMEMORY;
    }
    if(pkt_queue_->packet_queue_init() == 0) {
        pkt_queue_->packet_queue_start();
        return RET_OK;
    }
    else {
        return RET_FAIL;
    }
}
int VideoDecodeLoop::queue_picture(AVFrame *src_frame, double pts,
                                   double duration, int64_t pos, int serial)
{
    Frame *vp;

    if (!(vp = frame_queue_->frame_queue_peek_writable())) // 检测队列是否有可写空间
        return -1;      // 请求退出则返回-1
    // 执行到这步说已经获取到了可写入的Frame
    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;
    if(fabs(pts - start_pts_) < 0.5) {      // 目的是计算帧间隔
        vp->int_duration = fabs(pts - start_pts_) * 1000; // 转成ms
         start_pts_ = pts;
    } else {
        vp->int_duration = duration*1000;         // 只针对flv的时间戳
    }

    av_frame_move_ref(vp->frame, src_frame); // 将src中所有数据转移到dst中，并复位src。
    frame_queue_->frame_queue_push(vp->int_duration);   // 更新写索引位置
    return 0;
}

void VideoDecodeLoop::Loop()
{
    AVFrame *frame = av_frame_alloc();
    RET_CODE ret = RET_OK;
    static FILE *decode_dump_h264 = NULL;
    while(true)
    {
        if(request_exit_)
            break;

        do
        {
            if(request_exit_)
                break;
            ret = h264_decoder_->ReceiveFrame(frame);
            if (ret == 0) {
                frame->pts = frame->best_effort_timestamp;
            }

            if (ret == RET_OK)           // 解到一帧数据
            {
                //                LogWarn("decoder key:%d  vpts:%u",frame->key_frame, frame->pts);
                if(queue_picture(frame, frame->pts / 1000.0, 0.040,
                                 frame->pkt_pos, pkt_serial) < 0) {
                    request_exit_ = 1;  // 返回-1请求退出
                }
            }
        } while (ret == RET_OK);

        if(request_exit_)
            break;
        AVPacket pkt;
        if (pkt_queue_->packet_queue_get(&pkt, 1, &pkt_serial) < 0)
        {
            LogError("packet_queue_get failed");
            break;
        }

        if(!decode_dump_h264) {
            decode_dump_h264 = fopen("decode.h264","wb+");
        }
        fwrite(pkt.data, pkt.size, 1, decode_dump_h264);
        fflush(decode_dump_h264);

        if (pkt.data == flush_pkt.data) {//
            // when seeking or when switching to a different stream
            h264_decoder_->FlushBuffers(); //清空里面的缓存帧
            finished_ = 0;        // 重置为0
        } else if (pkt.data != NULL && pkt.size!=0) {
            //            LogInfo("nalu type:%d, size:%d", 0x1f & pkt.data[4], pkt.size);
            if(h264_decoder_->SendPacket(&pkt) != RET_OK) {
                LogError("SendPacket failed, which is an API violation.\n");
            }
            av_packet_unref(&pkt);
        } else {
            LogWarn("pkt null");
        }
    }
    av_frame_free(&frame);
    LogWarn("Loop leave");
}

int VideoDecodeLoop::Post(void *pkt)
{
    int64_t duration = pkt_queue_->duration();
    auto size = pkt_queue_->get_nb_packets();
    if(size > 25) {
        if(packet_cache_delay_++ > 3) {
            packet_cache_delay_ = 0; // 只是为了降低打印的频率
            LogInfo("video cache %d packet lead to delay:%lldms\n", size, duration);
        }
    }

    return pkt_queue_->packet_queue_put((AVPacket *)pkt);
}
}
