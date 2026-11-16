#pragma once

#include "th01/video.h"

#include "core/text/font.h"

typedef struct {
    uint32_t codepoint;
    uint16_t rows[16];
    uint8_t width;
} GAME_TEXT_GLYPH;

// Holds immutable glyphs sorted by Unicode scalar value. Halfwidth glyphs
// occupy bits 15 through 8; fullwidth glyphs occupy the whole row. Storage
// belongs to the allocator used when loading and must outlive drawing.
typedef struct {
    const GAME_TEXT_GLYPH *glyphs;
    int32_t count;
} GAME_TEXT_FONT;

typedef enum {
    GAME_TEXT_NORMAL,
    GAME_TEXT_HEAVY,
    GAME_TEXT_BOLD,
    GAME_TEXT_BLACK,
} GAME_TEXT_WEIGHT;

typedef struct {
    GAME_TEXT_WEIGHT weight;
    int32_t spacing;
    bool clear_background, underline, reverse;
} GAME_TEXT_STYLE;

// Copies and sorts supplied bitmap glyphs. Requires unique Unicode scalar
// values, the character's 8- or 16-pixel cell width, and zero low bytes in
// halfwidth rows. Uses 8 for ASCII and halfwidth katakana, 16 otherwise. Leaves
// out empty on failure and allocates final storage only after validation.
RESULT GameText_LoadGlyphs(
    const GAME_TEXT_GLYPH *glyphs, int32_t count, const ALLOCATOR *allocator,
    GAME_TEXT_FONT *out);

// Caches the distinct characters in a valid UTF-8 string through a borrowed
// core font. Initializes no font subsystem and retains no font handle.
// Rasterizes with the font's normal face, scales the complete RGBA cell to
// 8x16 or 16x16 by nearest sampling, and keeps alpha values of at least 128.
// Samples source coordinates floor(x*image_width/cell_width) and
// floor(y*image_height/16), including the image's cell margins.
// ASCII and halfwidth katakana use 8 pixels; other characters use 16.
// Controls use the halfwidth space glyph. The bundled font is a portable
// substitute, not the original PC-98 ROM shapes. Releases temporary images
// before returning; final glyph storage uses allocator. Leaves out empty
// on failure, without allocating from allocator.
RESULT GameText_Load(
    FONT *font, const char *characters, const ALLOCATOR *allocator,
    GAME_TEXT_FONT *out);

// Finds a cached Unicode scalar, or returns nullptr. C0/C1 controls map to
// the halfwidth space. The returned rows can also supply enlarged overlays.
const GAME_TEXT_GLYPH *GameText_Find(
    const GAME_TEXT_FONT *font, uint32_t codepoint);

// Measures one line with 0 through 7 pixels between cells, excluding the
// final gap. Uses cached widths; uncached characters retain their default
// cell width and draw blank. Empty text has zero width. Rejects malformed
// UTF-8, invalid styles and widths beyond INT32_MAX, leaving width zero.
RESULT GameText_Measure(
    const GAME_TEXT_FONT *font, const char *text, const GAME_TEXT_STYLE *style,
    int32_t *width);

// Draws glyph coverage in color masked to four bits on the drawing page.
// Applies 16-bit source weights before reverse; overflow to the left is
// discarded. Clear-background fills the text extent with index 0, through
// row 17 when underlined, and underline uses index 7 at top+17.
// Stops before a cell that extends beyond the right edge, as the source
// does. Clips left and vertical writes without reproducing VRAM aliasing.
// Empty text draws nothing; malformed input leaves video unchanged.
// Successful drawing and measurement allocate nothing. Failure results own
// an error message; validate strings and styles before simulation. Neither
// operation reads a font, renderer, clock, file or device. Page selectors
// and the DAC remain unchanged.
RESULT GameText_Draw(
    const GAME_TEXT_FONT *font, VIDEO *video, const char *text, int32_t left,
    int32_t top, uint8_t color, const GAME_TEXT_STYLE *style);
