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
    bool clear_background;
    bool underline;
    bool reverse;
} GAME_TEXT_STYLE;

// Caches the distinct characters of a UTF-8 string with their cells from rom.
// A character without a cell is cached blank, with a warning.
RESULT GameText_Load(
    const FONT_ROM *rom, const char *characters, const ALLOCATOR *allocator,
    GAME_TEXT_FONT *out);

// Finds a cached Unicode scalar, or returns nullptr. C0/C1 controls map to
// the halfwidth space.
const GAME_TEXT_GLYPH *GameText_Find(
    const GAME_TEXT_FONT *font, uint32_t codepoint);

// Draws UTF-8 text with spacing 0..7 between cells on the drawing page.
// clear_background fills the extent with index 0; underline draws index 7.
// Stops before a cell past the right edge, as the original does.
void GameText_Draw(
    const GAME_TEXT_FONT *font, const char *text, int32_t left, int32_t top,
    uint8_t color, const GAME_TEXT_STYLE *style);
