#pragma once

#include "core/text.h"

#include <SDL3_ttf/SDL_ttf.h>

struct FONT {
    TTF_Font *font;
    uint8_t *data;
    // The em size for each pixel of cell height.
    float cell_scale;
    float ascent_scale;
    void *raster_face;
    bool native_bold;
    int32_t bold_advance_adjust;
    // The font that gives the characters font lacks, or nullptr.
    FONT *fallback;
};

RESULT TextGlyphs_Measure(
    const FONT *font, const char *text, size_t size, bool gdi, int32_t *width,
    int32_t *height);
RESULT TextGlyphs_Render(
    const FONT *font, const char *text, size_t size, bool gdi,
    SDL_Surface **out);

RESULT TextRaster_Init(void);
void TextRaster_Shutdown(void);
RESULT TextRaster_Open(const uint8_t *data, size_t size, float em, void **out);
void TextRaster_Close(void *face);
RESULT TextRaster_SetSize(void *face, float em);
RESULT TextRaster_Glyph(
    const FONT *font, uint32_t index, SDL_Surface **out, int32_t *offset_x,
    int32_t *offset_y);

bool TextRaster_IsSimple(const FONT *font, const char *text, size_t size);
RESULT TextRaster_MeasureSimple(
    const FONT *font, const char *text, size_t size, int32_t *width,
    int32_t *height);
RESULT TextRaster_RenderSimple(
    const FONT *font, const char *text, size_t size, SDL_Surface **out);
