#pragma once

#include "core/text.h"

#include <SDL3_ttf/SDL_ttf.h>

struct FONT {
    TTF_Font *font;
    // The em size for each pixel of cell height.
    float cell_scale;
    float ascent_scale;
    void *raster_face;
    bool native_bold;
    int32_t bold_advance_adjust;
    // The font that gives the characters font lacks, or nullptr.
    FONT *fallback;
};

// Opens face face_index of the font in data at the cell height
// pixel_height. The font reads data, which must outlive it. Leaves out
// empty on failure.
RESULT Font_Open(
    const void *data, size_t size, int32_t face_index, float pixel_height,
    FONT **out);

// Reads a big-endian number of bytes bytes.
uint32_t Text_ReadBe(const uint8_t *data, int32_t bytes);

// Adds data, the system's font for family, as FontRegistry_AddSystemFont
// does, and takes data, which Mem_Free releases. Reports failure, and adds
// nothing, when no name of a face is family.
RESULT FontRegistry_AddSystemData(
    const char *source, uint8_t *data, size_t size, const char *family);

// Reads the file at path and adds it as FontRegistry_AddSystemData does.
RESULT FontRegistry_AddSystemFile(const char *path, const char *family);

RESULT TextGlyphs_Measure(
    const FONT *font, const char *text, size_t size, bool gdi, int32_t *width,
    int32_t *height);
RESULT TextGlyphs_Render(
    const FONT *font, const char *text, size_t size, bool gdi,
    SDL_Surface **out);

RESULT TextRaster_Init(void);
void TextRaster_Shutdown(void);
RESULT TextRaster_Open(
    const uint8_t *data, size_t size, int32_t face_index, float em, void **out);
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
