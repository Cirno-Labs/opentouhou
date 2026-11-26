#pragma once

// Graphics text: the game's CP932 text fields read into the glyph masks
// that Draw_Text draws. It touches no VRAM; it sits in draw because draw
// owns the glyph format it fills, as tram owns its text fields.

#include "th02/draw/glyphs.h"
#include "th02/tram.h"

typedef struct {
    uint16_t selector;
    bool wide;
    const char *text;
    size_t size;
} DRAW_TEXT_CHARACTER;

// Renders one character into sixteen mask rows, bit 15 leftmost.
typedef struct {
    RESULT (*load)(
        void *context, const DRAW_TEXT_CHARACTER *character, uint16_t rows[16]);
    void *context;
} DRAW_TEXT_FONT;

typedef struct {
    TRAM_TEXT cells;
    const DRAW_TEXT_GLYPH *glyphs;
    int32_t count;
} DRAW_TEXT;

// Reads a CP932 text field, all of it even past zero, rendering each
// character with font.
RESULT DrawText_Read(
    READER *reader, size_t byte_size, const DRAW_TEXT_FONT *font,
    const ALLOCATOR *allocator, DRAW_TEXT *out);
