#pragma once

// Text RAM: the 80x25 text layer over the graphics, each cell a character
// code and an attribute, and the writes that fill it.

#include "th02/tram/text.h"

#define TRAM_COLUMNS 80
#define TRAM_ROWS 25
#define TRAM_PLANE_CELLS 4096

// Text attributes: a color in bits 5..7 (the digital color minus one), and
// flags. A cell shows only with TRAM_VISIBLE, which every color includes.
// clang-format off
#define TRAM_VISIBLE       0x01
#define TRAM_BLINK         0x02
#define TRAM_REVERSE       0x04
#define TRAM_UNDERLINE     0x08
#define TRAM_VERTICAL_LINE 0x10
#define TRAM_BLACK         0x01
#define TRAM_BLUE          0x21
#define TRAM_RED           0x41
#define TRAM_MAGENTA       0x61
#define TRAM_GREEN         0x81
#define TRAM_CYAN          0xa1
#define TRAM_YELLOW        0xc1
#define TRAM_WHITE         0xe1
// clang-format on

// Text RAM, hidden tail and full attribute words included.
typedef struct {
    uint16_t codes[TRAM_PLANE_CELLS];
    uint16_t attributes[TRAM_PLANE_CELLS];
} TRAM;

extern const TRAM *const g_Tram;

// Fill touches only the visible 80x25 cells.
void Tram_Init(void);
void Tram_Fill(uint16_t code, uint16_t attribute);

// Writes a two-cell gaiji; addresses wrap at 16 bits, and stores outside
// text RAM are dropped.
void Tram_PutGaiji(uint16_t x, uint16_t y, uint16_t glyph, uint16_t attribute);

// Writes a gaiji string, the cursor wrapping at 16 bits.
void Tram_PutGaijiString(
    uint16_t x, uint16_t y, const uint8_t *glyphs, uint16_t attribute);

// Writes the first bytes of text, cutting a wide character into its
// partial code.
void Tram_PutText(
    uint16_t x, uint16_t y, const TRAM_TEXT *text, uint16_t source_bytes,
    uint16_t attribute);

// Writes one character, wide ones by their single code.
void Tram_PutCharacter(
    uint16_t x, uint16_t y, const TRAM_CHARACTER *character,
    uint16_t attribute);

// Sets the attributes of ordered visible cells.
void Tram_FillAttributes(
    uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t attribute);
