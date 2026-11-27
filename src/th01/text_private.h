#pragma once

#include "th01/text.h"

// Shares the glyph validation th01/text.c's GameText_Load and tests'
// GameText_LoadGlyphs both rely on.

// Whether value is a valid Unicode scalar value (not a surrogate, at most
// U+10FFFF).
bool Text_Scalar(uint32_t value);

// Maps a C0/C1 control code point to the halfwidth space, else value as is.
uint32_t Text_Normalize(uint32_t value);

// The cell width of a code point: 8 pixels for ASCII and halfwidth kana,
// else 16.
uint8_t Text_Width(uint32_t value);

// Orders two GAME_TEXT_GLYPHs by code point, for qsort.
int Text_Compare(const void *first, const void *second);
