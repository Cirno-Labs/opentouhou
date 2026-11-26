#pragma once

// The font choice: the first PC-98 font ROM dump that the user supplies,
// else the built-in BDF pair, for both text RAM and graphics text.

#include "th02/draw/text.h"

#include "core/text/font_rom.h"

typedef struct {
    FONT_ROM *rom;
} TEXT_FONT;

// Opens the first of FONT.ROM, NEC.BMP, ANEX86.BMP, FREECG98.BMP and the
// BDF pair.
RESULT TextFont_Open(TEXT_FONT *font);

void TextFont_Close(TEXT_FONT *font);

// Returns the text RAM font, valid until Close; invalid cells are blank.
TRAM_FONT TextFont_Tram(const TEXT_FONT *font);

// Returns the graphics font, which fails on unsupported selectors instead
// of drawing a Unicode substitute.
DRAW_TEXT_FONT TextFont_Graphics(TEXT_FONT *font);
