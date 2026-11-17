#pragma once

#include "core/text.h"

#include <SDL3_ttf/SDL_ttf.h>

struct TEXT_FONT {
    TTF_Font *font;
    uint8_t *data;
    // The em size for each pixel of cell height.
    float cell_scale;
    bool native_bold;
    int32_t bold_advance_adjust;
    // The font that gives the characters font lacks, or nullptr.
    TEXT_FONT *fallback;
};

RESULT TextGlyphs_Measure(
    const TEXT_FONT *font, const char *text, size_t size, int32_t *width,
    int32_t *height);
RESULT TextGlyphs_Render(
    const TEXT_FONT *font, const char *text, size_t size, SDL_Surface **out);
