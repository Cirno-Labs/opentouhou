#pragma once

#include "th01/bos.h"
#include "th01/ptn.h"
#include "th01/video.h"

// Draws a full PTN image (quarter -1) or a quarter numbered 0..3 in reading
// order. Requires a valid image index. Rounds left down to eight pixels.
// Uses the drawing page for put, page 0 for erase; erase restores page 1.
// Horizontal overshoot wraps across visible rows as in linear planar VRAM.
// Discards accesses outside the visible page, excluding cross-plane aliasing.
// Allocates nothing. Does not apply the optional restore-before-put flag.
void Sprite_PtnPut(
    VIDEO *video, const PTN *sprites, int32_t image, int32_t quarter,
    int16_t left, int16_t top);
void Sprite_PtnErase(
    VIDEO *video, const PTN *sprites, int32_t image, int32_t quarter,
    int16_t left, int16_t top);

// Restores full word groups from page 1 to page 0. Rounds left down and
// left+width up to 16 pixels. Preserves linear row wrapping and excludes
// outside-page accesses as above. Requires positive width and height.
void Sprite_Restore16(
    VIDEO *video, int16_t left, int16_t top, int16_t width, int16_t height);

// Draws player BOS coverage, masking away colors outside coverage. Uses
// the source's (left+640)/8-80 byte address and intended-row clipping.
// Requires valid image indices and dimensions; selects drawing page 0.
// Discards outside-page accesses. Allocates nothing.
void Sprite_PlayerPut(
    VIDEO *video, const BOS *sprites, int32_t image, int16_t left, int16_t top);
void Sprite_PlayerErase(
    VIDEO *video, const BOS *sprites, int32_t image, int16_t left, int16_t top);

// Restores only old coverage not overlapped by the new coverage, then puts
// the new image, row by row. Preserves the original optimizer's mask and
// left-edge quirks, including its independent erase_top. Requires width
// at most 64 and a byte-column displacement of -1, 0 or 1. The intended-row
// put clipping differs from the erase's linear addressing at screen edges.
void Sprite_PlayerOverlap(
    VIDEO *video, const BOS *sprites, int32_t put_image, int32_t erase_image,
    int16_t put_left, int16_t put_top, int16_t erase_left, int16_t erase_top);
