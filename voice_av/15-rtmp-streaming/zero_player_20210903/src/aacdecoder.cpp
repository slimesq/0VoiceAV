#include "aacdecoder.h"
#include "dlog.h"
namespace LQF {
AACDecoder::AACDecoder()
{

}

AACDecoder::~AACDecoder()
{
    avcodec_free_context(&ctx);

    if (packet)
        //Release pacekt
        av_packet_free(&packet);
    if (frame)
        //Release frame
        av_frame_free(&frame);
}

RET_CODE AACDecoder::Init(const Properties &properties)
{
    //NO ctx yet
    ctx = NULL;


    // Get encoder
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);

    // Check codec
    if(!codec)
    {
        LogError("No codec found\n");
        return RET_ERR_MISMATCH_CODE;
    }
    //Alocamos el conto y el picture
    ctx = avcodec_alloc_context3(codec);
    if (!ctx)
        return RET_ERR_OUTOFMEMORY;

    //Set params
//    ctx->request_sample_fmt		= AV_SAMPLE_FMT_S16;
    // 初始化codecCtx
    ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    ctx->sample_rate = 48000;
//    ctx->bit_rate = bit;
    av_channel_layout_default(&ctx->ch_layout, 2);
    //OPEN it
    if (avcodec_open2(ctx, codec, NULL) < 0)
    {
        LogError("could not open codec\n");
        return RET_FAIL;
    }

    //Create packet
    packet = av_packet_alloc();
    //Allocate frame
    frame = av_frame_alloc();

    //Get the number of samples
    numFrameSamples = 1024;
    return RET_OK;
}

RET_CODE AACDecoder::Decode(const uint8_t *in, int inLen, uint8_t *out, int &outLen)
{
    //If we have input
    if (inLen<=0)
        return RET_FAIL;

    //Set data
    packet->data = (uint8_t *)in;
    packet->size = inLen;

    //Decode it
    if (avcodec_send_packet(ctx, packet)<0)
        //nothing
    {
        LogError("-AACDecoder::Decode() Error decoding AAC packet");
        return RET_FAIL;
    }
    //Release side data
    av_packet_free_side_data(packet);

    //If we got a frame
    if (avcodec_receive_frame(ctx, frame)<0)
        //Nothing yet
    {
        outLen = 0;
        return RET_FAIL;
    }
    //Get data
    float *buffer1 = (float *) frame->data[0];
    float *buffer2 = (float *) frame->data[1];
    auto len = frame->nb_samples;
    int16_t *sample = (int16_t *)out;
    //Convert to SWORD
    for (size_t i=0; i<len; ++i)
    {
        sample[i*2] =  (int16_t)(buffer1[i] * 0x7fff);
        sample[i*2 + 1] =  (int16_t)(buffer2[i] * 0x7fff);
    }
    outLen = 4096;
    static FILE *dump_pcm = NULL;
    if(!dump_pcm)
    {
        dump_pcm = fopen("aac_dump.pcm", "wb");
        if(!dump_pcm)
        {
            LogError("fopen aac_dump.pcm failed");
        }
    }
    if(dump_pcm)
    {//ffplay -ar 48000 -ac 2 -f s16le -i aac_dump.pcm
        fwrite(out, 1,outLen,  dump_pcm);
        fflush(dump_pcm);
    }
    //Return number of samples
    return RET_OK;
}
void AACDecoder::FlushBuffers()
{
    avcodec_flush_buffers(ctx); //清空里面的缓存帧
}
RET_CODE AACDecoder::SendPacket(const AVPacket *avpkt)
{
    int ret = avcodec_send_packet(ctx, avpkt);
    if(0 == ret)
        return RET_OK;
    if (AVERROR(EAGAIN) == ret) {
        return RET_ERR_EAGAIN;
    } else if(ret == AVERROR_EOF) {
        LogWarn("avcodec_send_packet failed, RET_ERR_EOF.\n");
        return RET_ERR_EOF;
    } else {
        LogError("avcodec_send_packet failed, AVERROR(EINVAL) or AVERROR(ENOMEM) or other...\n");
        return RET_FAIL;
    }
}
RET_CODE AACDecoder::ReceiveFrame(AVFrame *frame)
{
    int ret = avcodec_receive_frame(ctx, frame);
    if(0 == ret)
        return RET_OK;
    if (AVERROR(EAGAIN) == ret) {
        return RET_ERR_EAGAIN;
    } else if(ret == AVERROR_EOF) {
        LogWarn("avcodec_receive_frame failed, RET_ERR_EOF.\n");
        return RET_ERR_EOF;
    } else {
        LogError("avcodec_receive_frame failed, AVERROR(EINVAL) or AVERROR(ENOMEM) or other...\n");
        return RET_FAIL;
    }
}
bool AACDecoder::SetConfig(const uint8_t *data, const size_t size)
{
    return true;
}
}
