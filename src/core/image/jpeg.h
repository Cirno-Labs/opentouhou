#pragma once

#include "core/image/pixels.h"

// Decodes an eight-bit JPEG, or leaves handled false for other color spaces
// and precisions that the general decoder supports.
RESULT Image_DecodeJpeg(
    const uint8_t *data, size_t size, const ALLOCATOR *allocator, IMAGE *out,
    bool *handled);
