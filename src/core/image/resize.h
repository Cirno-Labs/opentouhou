#pragma once

#include "core/base/types.h"
#include "core/image/pixels.h"

// Scales image to width x height with a bilinear filter.
void Image_Resize(IMAGE *image, int32_t width, int32_t height);
