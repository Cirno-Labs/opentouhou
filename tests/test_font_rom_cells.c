#include "core/base/memory.h"
#include "core/text/font_rom.h"

#include "test.h"

static uint16_t M_CellBits(const int first, const int second, const int row)
{
    return (uint16_t)(0xa501u ^ (unsigned)first * 257u ^ (unsigned)second * 37u
                      ^ (unsigned)row * 0x1c37u);
}

TEST(raw_rom_cells_keep_all_ank_and_jis_addresses_and_unicode_aliases)
{
    uint8_t *const bytes = Mem_AllocZ(288768);
    for (int code = 0; code < 256; code++) {
        for (int row = 0; row < 16; row++) {
            bytes[0x800 + code * 16 + row] = (uint8_t)(code ^ row * 7);
        }
    }
    for (int first = 0x21; first <= 0x7c; first++) {
        for (int second = 0x20; second <= 0x7f; second++) {
            const size_t offset =
                0x1800 + ((first - 0x21) * 96 + second - 0x20) * 32;
            for (int row = 0; row < 16; row++) {
                const uint16_t bits = M_CellBits(first, second, row);
                bytes[offset + row] = (uint8_t)(bits >> 8);
                bytes[offset + 16 + row] = (uint8_t)bits;
            }
        }
    }
    READER reader;
    Reader_OpenBuffer(&reader, bytes, 288768, nullptr);
    FONT_ROM *rom;
    CHECK(Result_Ignore(FontRom_ReadT98(&reader, &rom)));
    for (uint32_t code = 0; code <= UINT16_MAX; code++) {
        const uint16_t *const half = FontRom_Cell(rom, (uint16_t)code, false);
        CHECK((half != nullptr) == (code < 256));
        if (half != nullptr) {
            for (int row = 0; row < 16; row++) {
                CHECK_EQ_INT(half[row], ((code ^ row * 7) & 255) << 8);
            }
        }
        const int first = (int)(code >> 8), second = (int)(code & 255);
        const bool valid =
            first >= 0x21 && first <= 0x7e && second >= 0x21 && second <= 0x7e;
        const uint16_t *const full = FontRom_Cell(rom, (uint16_t)code, true);
        CHECK((full != nullptr) == valid);
        if (full != nullptr) {
            for (int row = 0; row < 16; row++) {
                CHECK_EQ_INT(
                    full[row],
                    first <= 0x7c ? M_CellBits(first, second, row) : 0);
            }
        }
    }
    uint16_t rows[16];
    uint8_t width;
    CHECK(FontRom_Glyph(rom, 0x2252, rows, &width));
    CHECK_EQ_INT(width, 16);
    CHECK_EQ_INT(rows[0], FontRom_Cell(rom, 0x2262, true)[0]);
    CHECK(
        FontRom_Cell(rom, 0x2262, true)[0]
        != FontRom_Cell(rom, 0x2d70, true)[0]);
    CHECK(FontRom_Glyph(rom, 'A', rows, &width));
    CHECK_EQ_INT(rows[0], FontRom_Cell(rom, 'A', false)[0]);
    CHECK(rows[0] != FontRom_Cell(rom, 0x2941, true)[0]);
    FontRom_Free(rom);
    Reader_Close(&reader);
    Mem_Free(bytes);
}

TEST(bdf_graphics_aliases_borrow_halfwidth_cells)
{
    static const char full[] =
        "STARTFONT 2.1\nSTARTPROPERTIES 4\nPIXEL_SIZE 16\n"
        "FONT_ASCENT 14\nFONT_DESCENT 2\n"
        "CHARSET_REGISTRY \"JISX0208.1990\"\n"
        "ENDPROPERTIES\nCHARS 0\nENDFONT\n";
    static const char half[] =
        "STARTFONT 2.1\nSTARTPROPERTIES 4\nPIXEL_SIZE 16\n"
        "FONT_ASCENT 14\nFONT_DESCENT 2\n"
        "CHARSET_REGISTRY \"JISX0201.1976\"\nENDPROPERTIES\n"
        "CHARS 1\nSTARTCHAR A\nENCODING 65\nBBX 8 1 0 13\n"
        "BITMAP\nA5\nENDCHAR\nENDFONT\n";
    READER full_reader, half_reader;
    Reader_OpenBuffer(&full_reader, full, sizeof(full) - 1, nullptr);
    Reader_OpenBuffer(&half_reader, half, sizeof(half) - 1, nullptr);
    FONT_ROM *rom;
    CHECK(Result_Ignore(FontRom_ReadBdf(&full_reader, &half_reader, &rom)));
    for (uint16_t code = 0x21; code <= 0x7e; code++) {
        CHECK(
            FontRom_Cell(rom, 0x2900 | code, true)
            == FontRom_Cell(rom, code, false));
    }
    for (uint16_t code = 0xa1; code <= 0xdf; code++) {
        CHECK(
            FontRom_Cell(rom, 0x2a00 | (code - 0x80), true)
            == FontRom_Cell(rom, code, false));
    }
    CHECK(FontRom_Cell(rom, 0x2b21, true) == FontRom_Cell(rom, 0x20, false));
    CHECK_EQ_INT(FontRom_Cell(rom, 0x2941, true)[0], 0xa500);
    uint16_t rows[16];
    uint8_t width;
    CHECK(FontRom_Glyph(rom, 'A', rows, &width));
    CHECK_EQ_INT(width, 8);
    CHECK_EQ_INT(rows[0], 0xa500);
    FontRom_Free(rom);
    Reader_Close(&full_reader);
    Reader_Close(&half_reader);
}
