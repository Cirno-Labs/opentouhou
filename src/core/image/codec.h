#pragma once

#include "core/base/types.h"

// Sets the largest width or height Image_Decode accepts. A larger image is
// refused before any pixel buffer is allocated for it.
#define IMAGE_MAX_SIZE 16384

// Sets how libswscale converts and scales: bilinear, with chroma read and
// written at full resolution and accurate rounding, so that JPEG colour
// edges stay smooth.
#define IMAGE_SWS_FLAGS                                                        \
    (SWS_BILINEAR | SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP | SWS_ACCURATE_RND)
