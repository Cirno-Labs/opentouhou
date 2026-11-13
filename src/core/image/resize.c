// SPDX-License-Identifier: GPL-3.0-only

#include "core/image.h"

#include "core/base/log.h"
#include "core/image/codec.h"

#include <libswscale/swscale.h>

void Image_Resize(IMAGE *const image, const int32_t width, const int32_t height)
{
    struct SwsContext *const sws = sws_getContext(
        image->width, image->height, AV_PIX_FMT_RGBA, width, height,
        AV_PIX_FMT_RGBA, IMAGE_SWS_FLAGS, nullptr, nullptr, nullptr);
    if (sws == nullptr) {
        LOG_FATAL(
            "failed to scale a %dx%d image to %dx%d", image->width,
            image->height, width, height);
    }

    IMAGE result = Image_Create(width, height, image->allocator);
    const uint8_t *const src_planes[1] = { image->pixels };
    const int src_strides[1] = { image->width * 4 };
    uint8_t *const dst_planes[1] = { result.pixels };
    const int dst_strides[1] = { width * 4 };
    sws_scale(
        sws, src_planes, src_strides, 0, image->height, dst_planes,
        dst_strides);
    sws_freeContext(sws);

    Image_Free(image);
    *image = result;
}
