#pragma once

#include "core/base/types.h"
#include "core/image/pixels.h"

// Scales image to width x height with a bilinear filter.
void Image_Resize(IMAGE *image, int32_t width, int32_t height);

// Downsamples quantized pixels with a triangle filter and wrapped edges,
// rounding directly to the destination's channel depth.
void Image_DownsampleTriangle(
    IMAGE *image, int32_t width, int32_t height, IMAGE_DEPTH source_depth,
    IMAGE_DEPTH destination_depth);
