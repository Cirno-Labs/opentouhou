#pragma once

#include "th01/text.h"

// Copies and sorts bitmap glyphs exactly as src/th01/text.c's GameText_Load
// does for a FONT_ROM, but from glyphs tests already have as bitmaps. No
// caller outside tests needs it.
RESULT GameText_LoadGlyphs(
    const GAME_TEXT_GLYPH *glyphs, int32_t count, const ALLOCATOR *allocator,
    GAME_TEXT_FONT *out);
