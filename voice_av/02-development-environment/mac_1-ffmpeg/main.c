#include <stdio.h>

#include "libavutil/avutil.h"

int main()
{
    printf("Hello FFmpeg %s\n", av_version_info());
    return 0;
}
