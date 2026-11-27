#pragma once

#include "th02/background/background.h"

typedef enum {
    TILE_COPY_ALL,
    TILE_COPY_INTENSITY,
} TILE_COPY_MODE;

// Writable view of g_Background for the module's parts.
extern BACKGROUND *const p_Background;

// Copies height>0 rows at 80-byte pitch in order, overlap included,
// swapping eight-pixel halves when the parities differ.
void Render_CopyRows(
    uint16_t destination, uint16_t source, int16_t height, TILE_COPY_MODE mode);

// Copies height lines of an image from line top, without clamping height to
// sixteen.
void Render_CopyLines(
    uint16_t destination, uint8_t image, int16_t top, int16_t height,
    TILE_COPY_MODE mode);

// Fills one tile black; requires black RMW charging.
void Render_Black(uint16_t destination);
