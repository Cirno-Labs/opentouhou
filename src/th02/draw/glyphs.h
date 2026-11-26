#pragma once

// Gaiji and text glyphs drawn into the graphics pages, as the game draws
// its HUD numbers and menus there.

#include "th02/gaiji.h"

typedef struct {
    const uint16_t *rows;
    bool wide;
} DRAW_TEXT_GLYPH;

// Draws a gaiji at unsigned X; carry selects the next glyph, and ID 255
// draws 128.
void Draw_Gaiji(
    const GAIJI *bank, int16_t left, int16_t top, uint8_t glyph, uint8_t color,
    bool carry);

// Draws glyphs up to ID zero, step pixels apart; negative steps shift
// logically, as in retail.
void Draw_GaijiString(
    const GAIJI *bank, int16_t left, int16_t top, int16_t step,
    const uint8_t *glyphs, uint8_t color);

// Draws text with col_and_fx bits 0..3 color, 4..5 weight, 6..8 spacing,
// without vertical clipping.
void Draw_Text(
    const DRAW_TEXT_GLYPH *glyphs, int32_t count, int16_t left, int16_t top,
    uint16_t col_and_fx);
