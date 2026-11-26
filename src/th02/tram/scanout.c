#include "th02/tram/scanout.h"

#include "core/base/assert.h"

typedef struct {
    uint16_t code;
    uint8_t attribute;
    bool wide;
    bool right;
} TRAM_CELL;

static uint16_t M_Row(
    const GAIJI *const bank, const TRAM_FONT *const font,
    const TRAM_CELL *const cell, const uint8_t y)
{
    const uint8_t column = cell->code & 127;
    if (cell->wide && (column == 0x56 || column == 0x57)) {
        const size_t glyph = (cell->code >> 8) + (column - 0x56) * 128;
        return bank->rows[glyph][y];
    }
    return font->row != nullptr
        ? font->row(font->context, cell->code, cell->wide, y)
        : 0;
}

void Tram_Scanout(
    const GAIJI *const bank, const TRAM_FONT *const font,
    const uint16_t first_cell, const bool blink_visible, uint8_t *const pixels,
    const size_t stride)
{
    ASSERT(stride >= 640);
    for (size_t row = 0; row < TRAM_ROWS; row++) {
        TRAM_CELL cells[TRAM_COLUMNS];
        bool gaiji_right = false;
        bool kanji_right = false;
        uint16_t previous = 0;
        for (size_t x = 0; x < TRAM_COLUMNS; x++) {
            const size_t index =
                (first_cell + row * TRAM_COLUMNS + x) % TRAM_PLANE_CELLS;
            const uint16_t code = g_Tram->codes[index];
            TRAM_CELL *const cell = &cells[x];
            *cell = (TRAM_CELL) {
                .code = code & 0x7f7f,
                .attribute = (uint8_t)g_Tram->attributes[index],
            };
            if (kanji_right) {
                cell->code = previous;
                cell->wide = true;
                cell->right = true;
                kanji_right = false;
            } else if ((code >> 8) == 0) {
                cell->code = code;
                gaiji_right = false;
            } else {
                cell->wide = true;
                const uint8_t column = cell->code & 127;
                if (column == 0x56 || column == 0x57) {
                    cell->right = gaiji_right;
                    gaiji_right = !gaiji_right;
                } else {
                    gaiji_right = false;
                    kanji_right = column < 9 || column >= 12;
                }
            }
            previous = cell->code;
        }
        for (uint8_t y = 0; y < 16; y++) {
            uint8_t *const line = pixels + (row * 16 + y) * stride;
            for (size_t x = 0; x < TRAM_COLUMNS; x++) {
                const TRAM_CELL *const cell = &cells[x];
                const uint8_t attribute = cell->attribute;
                const bool shown = (attribute & 1) != 0
                    && ((attribute & 2) == 0 || blink_visible);
                const uint16_t bits = shown ? M_Row(bank, font, cell, y) : 0;
                const uint8_t mask = cell->right ? bits : bits >> 8;
                for (size_t dot = 0; dot < 8; dot++) {
                    const bool covered = (mask & (0x80u >> dot)) != 0;
                    line[x * 8 + dot] = covered != ((attribute & 4) != 0)
                        ? (attribute >> 5) + 1
                        : 0;
                }
            }
            if (y == 15) {
                for (size_t x = 0; x < TRAM_COLUMNS; x++) {
                    if ((cells[x].attribute & 8) != 0) {
                        for (size_t dot = x * 8 + 4;
                             dot < x * 8 + 12 && dot < 640; dot++) {
                            line[dot] = (cells[dot / 8].attribute >> 5) + 1;
                        }
                    }
                }
            }
            for (size_t x = 0; x < TRAM_COLUMNS; x++) {
                if ((cells[x].attribute & 16) != 0) {
                    line[x * 8 + 4] = (cells[x].attribute >> 5) + 1;
                }
            }
        }
    }
}
