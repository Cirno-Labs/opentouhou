#pragma once

#include "core/base/types.h"

// Packs and names colors the way the games store them: one 32-bit
// 0xAARRGGBB value.

#define COLOR_ARGB(a, r, g, b)                                                 \
    ((uint32_t)(a) << 24 | (uint32_t)(r) << 16 | (uint32_t)(g) << 8            \
     | (uint32_t)(b))

#define COLOR_WHITE COLOR_ARGB(0xff, 0xff, 0xff, 0xff)
