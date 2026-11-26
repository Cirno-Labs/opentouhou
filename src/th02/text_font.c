#include "th02/text_font.h"

#include "core/base/assert.h"
#include "core/text/font_registry.h"

#include <string.h>

static uint16_t M_TramRow(
    const void *const context, const uint16_t code, const bool wide,
    const uint8_t y)
{
    ASSERT(y < 16);
    const FONT_ROM *const rom = context;
    const uint16_t address =
        wide ? (uint16_t)(((code & 255) + 0x20) << 8) | (code >> 8) : code;
    const uint16_t *const rows = FontRom_Cell(rom, address, wide);
    return rows != nullptr ? rows[y] : 0;
}

static RESULT M_GraphicsRows(
    void *const context, const DRAW_TEXT_CHARACTER *const character,
    uint16_t rows[16])
{
    const TEXT_FONT *const font = context;
    const uint32_t address = (uint32_t)character->selector + 0x2000;
    FAIL_IF(address > UINT16_MAX, "Unsupported graphics font selector");
    const uint16_t *const cell =
        FontRom_Cell(font->rom, (uint16_t)address, true);
    FAIL_IF(cell == nullptr, "Unsupported graphics font selector");
    memcpy(rows, cell, 16 * sizeof(*rows));
    return OK;
}

RESULT TextFont_Open(TEXT_FONT *const font)
{
    static const char *const names[][2] = {
        { "FONT.ROM", nullptr },
        { "NEC.BMP", nullptr },
        { "ANEX86.BMP", nullptr },
        { "FREECG98.BMP", nullptr },
        { "shnmk16.bdf", "shnm8x16r.bdf" },
    };
    font->rom = nullptr;
    for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
        const void *data;
        size_t size;
        if (FontRegistry_FindFile(names[i][0], &data, &size)
            && Result_Log(
                FontRom_Open(names[i][0], names[i][1], &font->rom),
                LOG_LEVEL_WARNING)) {
            return OK;
        }
    }
    return FAIL(
        "No PC-98 font: put FONT.ROM, NEC.BMP, ANEX86.BMP or FREECG98.BMP, or "
        "shnmk16.bdf with shnm8x16r.bdf, in opentouhou/fonts/");
}

void TextFont_Close(TEXT_FONT *const font)
{
    FontRom_Free(font->rom);
    font->rom = nullptr;
}

TRAM_FONT TextFont_Tram(const TEXT_FONT *const font)
{
    ASSERT(font->rom != nullptr);
    return (TRAM_FONT) { M_TramRow, font->rom };
}

DRAW_TEXT_FONT TextFont_Graphics(TEXT_FONT *const font)
{
    ASSERT(font->rom != nullptr);
    return (DRAW_TEXT_FONT) { M_GraphicsRows, font };
}
