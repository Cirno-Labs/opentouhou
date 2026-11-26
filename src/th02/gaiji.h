#pragma once

// Gaiji: the 256 user-defined 16x16 glyphs that the game loads into the
// text RAM font and shows like characters.

#include "pc98/zun/bfnt.h"

// Gaiji masks by ID, bit 15 leftmost.
typedef struct {
    uint16_t rows[256][16];
} GAIJI;

// Registers all 256 16x16 two-color glyphs of font.
RESULT Gaiji_Register(GAIJI *bank, const BFNT *font);
