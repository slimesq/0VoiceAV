#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#ifdef __cplusplus
}
#endif

static inline uint64_t default_channel_mask(int channels)
{
    AVChannelLayout layout = {AV_CHANNEL_ORDER_UNSPEC};
    av_channel_layout_default(&layout, channels);
    const uint64_t mask = layout.order == AV_CHANNEL_ORDER_NATIVE ? layout.u.mask : 0;
    av_channel_layout_uninit(&layout);
    return mask;
}
