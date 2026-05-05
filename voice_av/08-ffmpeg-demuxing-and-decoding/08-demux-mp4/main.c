#include <stdio.h>

#include "libavcodec/bsf.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"

const int sampling_frequencies[] = {
    96000,  // 0x0
    88200,  // 0x1
    64000,  // 0x2
    48000,  // 0x3
    44100,  // 0x4
    32000,  // 0x5
    24000,  // 0x6
    22050,  // 0x7
    16000,  // 0x8
    12000,  // 0x9
    11025,  // 0xa
    8000    // 0xb
    // 0xc d e f是保留的
};

int adts_header(char* const p_adts_header, const int data_length, const int profile,
                const int samplerate, const int channels) {
    int sampling_frequency_index = 3;  // 默认使用48000hz
    int adtsLen = data_length + 7;

    int frequencies_size = sizeof(sampling_frequencies) / sizeof(sampling_frequencies[0]);
    int i = 0;
    for (i = 0; i < frequencies_size; i++) {
        if (sampling_frequencies[i] == samplerate) {
            sampling_frequency_index = i;
            break;
        }
    }
    if (i >= frequencies_size) {
        printf("unsupport samplerate:%d\n", samplerate);
        return -1;
    }

    p_adts_header[0] = 0xff;       //syncword:0xfff                          高8bits
    p_adts_header[1] = 0xf0;       //syncword:0xfff                          低4bits
    p_adts_header[1] |= (0 << 3);  //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
    p_adts_header[1] |= (0 << 1);  //Layer:0                                 2bits
    p_adts_header[1] |= 1;         //protection absent:1                     1bit

    p_adts_header[2] = (profile) << 6;  //profile:profile               2bits
    p_adts_header[2] |= (sampling_frequency_index & 0x0f)
                        << 2;      //sampling frequency index:sampling_frequency_index  4bits
    p_adts_header[2] |= (0 << 1);  //private bit:0                   1bit
    p_adts_header[2] |= (channels & 0x04) >> 2;  //channel configuration:channels  高1bit

    p_adts_header[3] = (channels & 0x03) << 6;       //channel configuration:channels 低2bits
    p_adts_header[3] |= (0 << 5);                    //original：0                1bit
    p_adts_header[3] |= (0 << 4);                    //home：0                    1bit
    p_adts_header[3] |= (0 << 3);                    //copyright id bit：0        1bit
    p_adts_header[3] |= (0 << 2);                    //copyright id start：0      1bit
    p_adts_header[3] |= ((adtsLen & 0x1800) >> 11);  //frame length：value   高2bits

    p_adts_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);  //frame length:value    中间8bits
    p_adts_header[5] = (uint8_t)((adtsLen & 0x7) << 5);    //frame length:value    低3bits
    p_adts_header[5] |= 0x1f;                              //buffer fullness:0x7ff 高5bits
    p_adts_header[6] = 0xfc;  //‭11111100‬       //buffer fullness:0x7ff 低6bits
    // number_of_raw_data_blocks_in_frame：
    //    表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧。

    return 0;
}

// 程序本身 input.mp4  out.h264 out.aac
int main(int argc, char** argv) {
    // 判断参数
    if (argc != 4) {
        printf("usage app input.mp4  out.h264 out.aac");
        return -1;
    }

    char* in_filename = argv[1];
    char* h264_filename = argv[2];
    char* aac_filename = argv[3];
    FILE* h264_fp = fopen(h264_filename, "wb");
    if (!h264_fp) {
        printf("open %s failed\n", h264_filename);
        return -1;
    }
    FILE* aac_fp = fopen(aac_filename, "wb");
    if (!aac_fp) {
        printf("open %s failed\n", aac_filename);
        fclose(h264_fp);
        return -1;
    }

    AVFormatContext* fmt_ctx = NULL;
    int ret = avformat_open_input(&fmt_ctx, in_filename, NULL, NULL);
    if (ret < 0) {
        printf("open input file failed\n");
        fclose(h264_fp);
        fclose(aac_fp);
        return -1;
    }

    int video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    int audio_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

    AVBitStreamFilter* bsf_filter = av_bsf_get_by_name("h264_mp4toannexb");
    if (!bsf_filter) {
        printf("failed to get bitstream filter\n");
        fclose(h264_fp);
        fclose(aac_fp);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    AVBSFContext* bsf_ctx = NULL;
    ret = av_bsf_alloc(bsf_filter, &bsf_ctx);
    if (ret < 0) {
        printf("failed to alloc bitstream filter context\n");
        fclose(h264_fp);
        fclose(aac_fp);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    ret = avcodec_parameters_copy(bsf_ctx->par_in, fmt_ctx->streams[video_stream_index]->codecpar);
    if (ret < 0) {
        printf("failed to copy codec parameters to bitstream filter context\n");
        av_bsf_free(&bsf_ctx);
        fclose(h264_fp);
        fclose(aac_fp);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    ret = av_bsf_init(bsf_ctx);
    if (ret < 0) {
        printf("failed to init bitstream filter context\n");
        fclose(h264_fp);
        fclose(aac_fp);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        printf("failed to alloc packet\n");
        fclose(h264_fp);
        fclose(aac_fp);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            ret = av_bsf_send_packet(bsf_ctx, pkt);
            if (ret < 0) {
                printf("failed to send packet to bitstream filter\n");
                av_packet_unref(pkt);
                break;
            }
            while (av_bsf_receive_packet(bsf_ctx, pkt) >= 0) {
                size_t size = fwrite(pkt->data, 1, pkt->size, h264_fp);
                if (size != pkt->size) {
                    printf("failed to write h264 data to file\n");
                    av_packet_unref(pkt);
                    break;
                }
                av_packet_unref(pkt);
            }
        } else if (pkt->stream_index == audio_stream_index) {
            char adts_header_buf[7];
            AVCodecParameters* audio_parameters = fmt_ctx->streams[audio_stream_index]->codecpar;
            adts_header(adts_header_buf, pkt->size, audio_parameters->profile,
                        audio_parameters->sample_rate, audio_parameters->ch_layout.nb_channels);
            fwrite(adts_header_buf, 1, sizeof(adts_header_buf), aac_fp);
            size_t size = fwrite(pkt->data, 1, pkt->size, aac_fp);
            if (size != pkt->size) {
                printf("failed to write aac data to file\n");
                av_packet_unref(pkt);
                break;
            }
            av_packet_unref(pkt);
        }
    }
    printf("while finish\n");

failed:
    av_packet_free(&pkt);
    av_bsf_free(&bsf_ctx);
    fclose(h264_fp);
    fclose(aac_fp);
    avformat_close_input(&fmt_ctx);

    return 0;
}