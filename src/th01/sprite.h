#pragma once

#include "th01/bos.h"
#include "th01/ptn.h"
#include "th01/video.h"

// Draws a full PTN image (quarter -1) or quarter 0..3 in reading order,
// with left rounded down to eight; erase restores page 1 to page 0.
// Horizontal overshoot wraps across rows as in linear planar VRAM.
void Sprite_PtnPut(
    const PTN *sprites, int32_t image, int32_t quarter, int16_t left,
    int16_t top);
void Sprite_PtnErase(
    const PTN *sprites, int32_t image, int32_t quarter, int16_t left,
    int16_t top);

// Restores page 1 to page 0 in 16-pixel word groups. Requires a positive size.
// Wraps at the 32768-byte plane boundary, as the original's EGC does.
void Sprite_Restore16(int16_t left, int16_t top, int16_t width, int16_t height);

// Draws player BOS coverage on page 0, masking colors outside coverage.
void Sprite_PlayerPut(
    const BOS *sprites, int32_t image, int16_t left, int16_t top);
void Sprite_PlayerErase(
    const BOS *sprites, int32_t image, int16_t left, int16_t top);

// Restores the old coverage the new image misses, then puts the new image.
// Requires width at most 64 and a byte-column move of -1, 0 or 1.
// Keeps the original optimizer's mask and left-edge quirks.
void Sprite_PlayerOverlap(
    const BOS *sprites, int32_t put_image, int32_t erase_image,
    int16_t put_left, int16_t put_top, int16_t erase_left, int16_t erase_top);
