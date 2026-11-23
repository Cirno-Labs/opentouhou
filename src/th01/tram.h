#pragma once

#include "th01/text.h"

typedef enum {
    TRAM_TRANSPARENT = -1,
    TRAM_BLACK,
    TRAM_BLUE,
    TRAM_RED,
    TRAM_MAGENTA,
    TRAM_GREEN,
    TRAM_CYAN,
    TRAM_YELLOW,
    TRAM_WHITE,
} TRAM_COLOR;

// Holds a 640x400 indexed text layer over graphics. Index 0 is transparent;
// indices 1 through 8 are the hardware colors in B, R, G bit order. Only
// Tram_SetVisible changes visible.
typedef struct {
    uint8_t *pixels;
    bool visible;
} TRAM;

// Supplies the overlay palette. Entry 0 has zero alpha; every other color,
// including black, has full alpha.
extern const GFX_COLOR g_TramPalette[9];

// Allocates a clear, visible layer.
void Tram_Init(TRAM *tram, const ALLOCATOR *allocator);
void Tram_SetVisible(TRAM *tram, bool visible);

// Clears the layer to transparency.
void Tram_Clear(TRAM *tram);

// Fills a half-open rectangle, clipped to the screen.
void Tram_Fill(TRAM *tram, GFX_RECT rectangle, TRAM_COLOR color);

// Centers up to five glyphs of at most 40 source pixels in total at y=80,
// filling the rest of rows 0 through 335 with background. An empty line
// only clears. Leaves rows 336 through 399 unchanged. Five halfwidth glyphs
// fill the screen; two halfwidth glyphs start at x=192.
void Tram_PutLine16(
    TRAM *tram, const GAME_TEXT_GLYPH *const *glyphs, int32_t count,
    TRAM_COLOR foreground, TRAM_COLOR background);

// Fills the layer with black and shows graphics through a fullwidth glyph
// at (192,80).
void Tram_PutKanji16Reverse(TRAM *tram, const GAME_TEXT_GLYPH *glyph);
