#include "core/base/log.h"
#include "core/image.h"
#include "core/image/codec.h"

#include <libswscale/swscale.h>
#include <math.h>

static int32_t M_ChannelBits(const IMAGE_DEPTH depth, const int32_t channel)
{
    switch (depth) {
    case IMAGE_DEPTH_A1R5G5B5:
        return channel == 3 ? 1 : 5;
    case IMAGE_DEPTH_A4R4G4B4:
        return 4;
    case IMAGE_DEPTH_R5G6B5:
        return channel == 3 ? 0 : channel == 1 ? 6 : 5;
    case IMAGE_DEPTH_RGB8:
        return channel == 3 ? 0 : 8;
    case IMAGE_DEPTH_RGBA8:
        return 8;
    }
    return 8;
}

static double M_TriangleIntegral(const double x)
{
    if (x <= -1.0) {
        return 0.0;
    }
    if (x < 0.0) {
        return (x + 1.0) * (x + 1.0) * 0.5;
    }
    if (x < 1.0) {
        return 1.0 - (1.0 - x) * (1.0 - x) * 0.5;
    }
    return 1.0;
}

static float M_TriangleWeight(
    const double left, const double right, const int32_t pixel)
{
    return (float)((M_TriangleIntegral(right - pixel - 0.5)
                    - M_TriangleIntegral(left - pixel - 0.5))
                   / (right - left));
}

static int32_t M_WrapPixel(const int32_t pixel, const int32_t size)
{
    const int32_t wrapped = pixel % size;
    return wrapped < 0 ? wrapped + size : wrapped;
}

static float M_UnpackChannel(const uint8_t value, const int32_t bits)
{
    return bits == 0
        ? 1.0f
        : (float)(value >> (8 - bits)) / (float)((1u << bits) - 1u);
}

static uint8_t M_PackChannel(const float value, const int32_t bits)
{
    if (bits == 0) {
        return 255;
    }
    const uint32_t maximum = (1u << bits) - 1u;
    const uint32_t packed = (uint32_t)floorf(
        fminf(1.0f, fmaxf(0.0f, value)) * (float)maximum + 0.5f);
    uint32_t expanded = packed;
    int32_t filled = bits;
    while (filled < 8) {
        expanded = (expanded << bits) | packed;
        filled += bits;
    }
    return (uint8_t)(expanded >> (filled - 8));
}

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
    const uint8_t *const src_planes[4] = { image->pixels };
    const int src_strides[4] = { image->width * 4 };
    uint8_t *const dst_planes[4] = { result.pixels };
    const int dst_strides[4] = { width * 4 };
    sws_scale(
        sws, src_planes, src_strides, 0, image->height, dst_planes,
        dst_strides);
    sws_freeContext(sws);

    Image_Free(image);
    *image = result;
}

void Image_DownsampleTriangle(
    IMAGE *const image, const int32_t width, const int32_t height,
    const IMAGE_DEPTH source_depth, const IMAGE_DEPTH destination_depth)
{
    if (width < 1 || height < 1 || width > image->width
        || height > image->height) {
        LOG_FATAL(
            "Invalid triangle downsampling size: %dx%d from %dx%d", width,
            height, image->width, image->height);
    }
    if (width == image->width && height == image->height) {
        Image_Quantize(image, destination_depth);
        return;
    }
    IMAGE result = Image_Create(width, height, image->allocator);
    float *const rows =
        Mem_AllocArr((size_t)width * image->height * 4, sizeof(float));
    const double scale_x = (double)image->width / width;
    const double scale_y = (double)image->height / height;
    for (int32_t y = 0; y < image->height; y++) {
        for (int32_t x = 0; x < width; x++) {
            float sum[4] = {};
            const double left = x * scale_x;
            const double right = (x + 1) * scale_x;
            for (int32_t i = (int32_t)floor(left - 0.5);
                 i < (int32_t)ceil(right + 0.5); i++) {
                const float weight = M_TriangleWeight(left, right, i);
                const uint8_t *const pixel = image->pixels
                    + ((size_t)y * image->width + M_WrapPixel(i, image->width))
                        * 4;
                for (int32_t c = 0; c < 4; c++) {
                    sum[c] += M_UnpackChannel(
                                  pixel[c], M_ChannelBits(source_depth, c))
                        * weight;
                }
            }
            for (int32_t c = 0; c < 4; c++) {
                rows[((size_t)y * width + x) * 4 + c] = sum[c];
            }
        }
    }
    for (int32_t y = 0; y < height; y++) {
        const double top = y * scale_y;
        const double bottom = (y + 1) * scale_y;
        for (int32_t x = 0; x < width; x++) {
            float sum[4] = {};
            for (int32_t i = (int32_t)floor(top - 0.5);
                 i < (int32_t)ceil(bottom + 0.5); i++) {
                const float weight = M_TriangleWeight(top, bottom, i);
                const float *const pixel = rows
                    + ((size_t)M_WrapPixel(i, image->height) * width + x) * 4;
                for (int32_t c = 0; c < 4; c++) {
                    sum[c] += pixel[c] * weight;
                }
            }
            for (int32_t c = 0; c < 4; c++) {
                result.pixels[((size_t)y * width + x) * 4 + c] =
                    M_PackChannel(sum[c], M_ChannelBits(destination_depth, c));
            }
        }
    }
    Mem_Free(rows);
    Image_Free(image);
    *image = result;
}
