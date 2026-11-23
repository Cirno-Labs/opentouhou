#pragma once

#include "th01/video.h"

#include "core/text/font_rom.h"

typedef struct {
    uint32_t codepoint;
    uint16_t rows[16];
    uint8_t width;
} GAME_TEXT_GLYPH;

// Holds glyphs sorted by Unicode scalar value. Halfwidth glyphs occupy bits
// 15 through 8 of each row; fullwidth glyphs occupy the whole row.
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
// values, the character's cell width and zero low bytes in halfwidth rows.
// ASCII and halfwidth katakana are 8 pixels wide, other characters 16.
RESULT GameText_LoadGlyphs(
    const GAME_TEXT_GLYPH *glyphs, int32_t count, const ALLOCATOR *allocator,
    GAME_TEXT_FONT *out);

// Caches the distinct characters of a UTF-8 string with their cells from
// rom, as the original draws them. Controls use the halfwidth space glyph.
// A character without a cell is cached blank, with a warning.
RESULT GameText_Load(
    const FONT_ROM *rom, const char *characters, const ALLOCATOR *allocator,
    GAME_TEXT_FONT *out);

// Finds a cached Unicode scalar, or returns nullptr. C0/C1 controls map to
// the halfwidth space.
const GAME_TEXT_GLYPH *GameText_Find(
    const GAME_TEXT_FONT *font, uint32_t codepoint);

// Draws text in color masked to four bits on the drawing page, with
// spacing pixels between cells. Applies the 16-bit weights before reverse,
// discarding overflow to the left; uncached characters draw blank.
// clear_background fills the text extent with index 0, through row 17 when
// underlined, and underline draws index 7 at top+17. Stops before a cell that
// extends beyond the right edge, as the original does. Clips left and vertical
// writes without the VRAM's aliasing. Fails without drawing on malformed input.
RESULT GameText_Draw(
    const GAME_TEXT_FONT *font, VIDEO *video, const char *text, int32_t left,
    int32_t top, uint8_t color, const GAME_TEXT_STYLE *style);
