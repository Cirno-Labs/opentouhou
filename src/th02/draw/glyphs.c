#include "th02/draw/glyphs.h"

#include "th02/clip.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/assert.h"

static void M_Gaiji(
    const uint16_t *const rows, const uint16_t offset, const uint16_t shift,
    const uint8_t color)
{
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t y = 0; y < 16; y++) {
        const uint16_t start = (uint16_t)(offset + y * 80);
        for (size_t x = 0; x < 16; x++) {
            if ((rows[y] & (0x8000u >> x)) != 0) {
                const size_t pixel =
                    ((start + (x + shift) / 8) % VIDEO_PLANE_SIZE) * 8
                    + (x + shift) % 8;
                page[pixel] = color & 15;
            }
        }
    }
}

void Draw_Gaiji(
    const GAIJI *const bank, const int16_t left, const int16_t top,
    const uint8_t glyph, const uint8_t color, const bool carry)
{
    const uint16_t selected = glyph + (carry ? 1 : 0);
    const uint16_t offset =
        (uint16_t)((uint16_t)top * 80u + ((uint16_t)left >> 3));
    M_Gaiji(
        bank->rows[selected == 256 ? 128 : selected], offset,
        (uint16_t)left & 7, color);
    Grcg_SetColor(color);
    Grcg_Off();
}

void Draw_GaijiString(
    const GAIJI *const bank, const int16_t left, const int16_t top,
    const int16_t step, const uint8_t *glyphs, const uint8_t color)
{
    uint16_t offset = (uint16_t)((uint16_t)top * 80u);
    uint16_t x = (uint16_t)left;
    while (*glyphs != 0) {
        offset = (uint16_t)(offset + (x >> 3));
        const uint16_t shift = x & 7;
        M_Gaiji(bank->rows[*glyphs++], offset, shift, color);
        x = (uint16_t)(shift + (uint16_t)step);
    }
    Grcg_SetColor(color);
    Grcg_Off();
}

void Draw_Text(
    const DRAW_TEXT_GLYPH *const glyphs, const int32_t count,
    const int16_t left, const int16_t top, const uint16_t col_and_fx)
{
    ASSERT(count >= 0 && left >= 0);
    ASSERT(count == 0 || glyphs != nullptr);
    const uint16_t weight = (col_and_fx >> 4) & 3;
    const uint16_t spacing = (col_and_fx >> 6) & 7;
    int32_t x = left;
    for (int32_t i = 0; i < count; i++) {
        const DRAW_TEXT_GLYPH *const glyph = &glyphs[i];
        const int32_t width = glyph->wide ? 16 : 8;
        if (x > VIDEO_WIDTH - width) {
            break;
        }
        uint16_t rows[16];
        for (size_t y = 0; y < 16; y++) {
            uint16_t row = glyph->rows[y];
            ASSERT(glyph->wide || (row & 255) == 0);
            if (weight == 1 || weight == 3) {
                row |= (uint16_t)(row << 1);
            }
            if (weight >= 2) {
                const uint16_t before = row;
                row |= (uint16_t)(row << 1);
                row &= (uint16_t)~((uint16_t)(before ^ row) << 1);
            }
            rows[y] = row;
        }
        M_Gaiji(
            rows, (uint16_t)((uint16_t)top * 80u + (uint32_t)x / 8),
            (uint16_t)x & 7, col_and_fx);
        x += width + spacing;
    }
    Grcg_SetColor(col_and_fx);
    Grcg_Off();
}
