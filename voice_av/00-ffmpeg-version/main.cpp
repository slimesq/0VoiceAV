#include <iostream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

int main()
{
    avformat_network_init();

    std::cout << "FFmpeg version: " << av_version_info() << '\n';
    std::cout << "libavformat: " << avformat_version() << '\n';
    std::cout << "libavcodec: " << avcodec_version() << '\n';
    std::cout << "libavfilter: " << avfilter_version() << '\n';

    avformat_network_deinit();
    return 0;
}
