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

// Holds a 640x400 text layer over graphics: index 0 is transparent and
// 1 through 8 are the hardware colors in B, R, G bit order.
typedef struct {
    uint8_t *pixels;
    bool visible;
} TRAM;

extern const TRAM *const g_Tram;

// Supplies the overlay palette. Entry 0 has zero alpha; every other color,
// including black, has full alpha.
extern const GFX_COLOR g_TramPalette[9];

// Allocates a clear, visible layer.
void Tram_Init(const ALLOCATOR *allocator);
void Tram_SetVisible(bool visible);

// Clears the layer to transparency.
void Tram_Clear(void);

// Fills a half-open rectangle, clipped to the screen.
void Tram_Fill(GFX_RECT rectangle, TRAM_COLOR color);

// Centers up to five glyphs at y=80 and fills rows 0..335 with background.
// An empty line only clears.
void Tram_PutLine16(
    const GAME_TEXT_GLYPH *const *glyphs, int32_t count, TRAM_COLOR foreground,
    TRAM_COLOR background);

// Fills the layer with black and shows graphics through a fullwidth glyph
// at (192,80).
void Tram_PutKanji16Reverse(const GAME_TEXT_GLYPH *glyph);
