#pragma once

#include "th01/video.h"

#include "core/base/assert.h"

// Returns the index at (x, y) of page, or zero outside the screen.
static inline uint8_t Th01Test_Pixel(
    const VIDEO *const video, const int32_t page, const int32_t x,
    const int32_t y)
{
    ASSERT(page >= 0 && page < 2);
    if (x < 0 || x >= VIDEO_WIDTH || y < 0 || y >= VIDEO_HEIGHT) {
        return 0;
    }
    return video->pages[page][y * VIDEO_WIDTH + x];
}
