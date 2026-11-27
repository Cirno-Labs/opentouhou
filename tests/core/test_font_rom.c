#include "tests/test.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/text/font_registry.h"
#include "core/text/font_rom.h"

#include <string.h>

#define M_T98_SIZE 288768
#define M_BMP_PIXELS 62
#define M_BMP_SIZE (M_BMP_PIXELS + 2048 * 256)

static const char m_BdfFullHeader[] =
    "STARTFONT 2.1\n"
    "FONT -test-fixed-medium-r-normal--16-150-75-75-c-160-jisx0208.1990-0\n"
    "SIZE 16 75 75\n"
    "FONTBOUNDINGBOX 16 16 0 -2\n"
    "STARTPROPERTIES 4\n"
    "PIXEL_SIZE 16\n"
    "FONT_ASCENT 14\n"
    "FONT_DESCENT 2\n"
    "CHARSET_REGISTRY \"JISX0208.1990\"\n"
    "ENDPROPERTIES\n";

// あ, 0x2422, and 亜, 0x3021, a 4x2 glyph whose BBX puts it at rows 9 and
// 10 and columns 14 through 17.
static const char m_BdfFullGlyphs[] =
    "CHARS 2\n"
    "STARTCHAR 2422\n"
    "ENCODING 9250\n"
    "SWIDTH 960 0\n"
    "DWIDTH 16 0\n"
    "BBX 16 16 0 -2\n"
    "BITMAP\n"
    "8001\n0000\n0000\n0000\n0000\n0000\n0000\n1234\n"
    "0000\n0000\n0000\n0000\n0000\n0000\n0000\nFFFF\n"
    "ENDCHAR\n"
    "STARTCHAR 3021\n"
    "ENCODING 12321\n"
    "BBX 4 2 14 3\n"
    "BITMAP\n"
    "F0\n"
    "90\n"
    "ENDCHAR\n"
    "ENDFONT\n";

// '\', 0x5C, and ｱ, 0xB1, an 8x4 glyph that its BBX puts 3 lines above the
// cell.
static const char m_BdfHalf[] =
    "STARTFONT 2.1\r\n"
    "STARTPROPERTIES 4\r\n"
    "PIXEL_SIZE 16\r\n"
    "FONT_ASCENT 14\r\n"
    "FONT_DESCENT 2\r\n"
    "CHARSET_REGISTRY \"JISX0201.1976\"\r\n"
    "ENDPROPERTIES\r\n"
    "CHARS 2\r\n"
    "STARTCHAR yen\r\n"
    "ENCODING 92\r\n"
    "BBX 8 16 0 -2\r\n"
    "BITMAP\r\n"
    "81\r\n00\r\n00\r\n00\r\n00\r\n00\r\n00\r\n00\r\n"
    "00\r\n00\r\n00\r\n00\r\n00\r\n00\r\n00\r\n18\r\n"
    "ENDCHAR\r\n"
    "STARTCHAR a\r\n"
    "ENCODING 177\r\n"
    "BBX 8 4 0 13\r\n"
    "BITMAP\r\n"
    "11\r\n22\r\n44\r\n3C\r\n"
    "ENDCHAR\r\n"
    "ENDFONT\r\n";

static void M_T98Put(
    uint8_t *const rom, const int32_t first, const int32_t second,
    const int32_t row, const uint16_t bits)
{
    uint8_t *const cell =
        rom + 0x1800 + ((first - 0x21) * 96 + second - 0x20) * 32;
    cell[row] = (uint8_t)(bits >> 8);
    cell[16 + row] = (uint8_t)bits;
}

// Builds a FONT.ROM with marked cells: 'A', '\', '~', ｱ, あ, ①, 亜 and ～.
static uint8_t *M_T98(void)
{
    uint8_t *const rom = Mem_AllocZ(M_T98_SIZE);
    rom[0x800 + 'A' * 16 + 3] = 0x5A;
    rom[0x800 + '\\' * 16 + 0] = 0x81;
    rom[0x800 + '~' * 16 + 1] = 0x7E;
    rom[0x800 + 0xB1 * 16 + 5] = 0x3C;
    M_T98Put(rom, 0x24, 0x22, 7, 0x1234);
    M_T98Put(rom, 0x2D, 0x21, 0, 0xFF01);
    M_T98Put(rom, 0x30, 0x21, 15, 0xABCD);
    M_T98Put(rom, 0x21, 0x41, 2, 0x0FF0);
    // Second byte 0x20 has no JIS code and must not land anywhere.
    M_T98Put(rom, 0x24, 0x20, 0, 0xFFFF);
    return rom;
}

static void M_PutU16(uint8_t *const out, const uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void M_PutU32(uint8_t *const out, const uint32_t value)
{
    M_PutU16(out, value & 0xFFFF);
    M_PutU16(out + 2, value >> 16);
}

// Inks width pixels of bits from x at line y, counted from the top.
static void M_BmpInk(
    uint8_t *const bmp, const int32_t x, const int32_t y, const uint16_t bits,
    const int32_t width)
{
    uint8_t *const line = bmp + M_BMP_PIXELS + (2047 - y) * 256;
    for (int32_t i = 0; i < width; i++) {
        if ((bits & (0x8000 >> i)) != 0) {
            line[(x + i) / 8] &= (uint8_t)~(0x80 >> ((x + i) % 8));
        }
    }
}

// Builds an Anex86 BMP with the same marked cells as M_T98.
static uint8_t *M_Anex86(void)
{
    uint8_t *const bmp = Mem_Alloc(M_BMP_SIZE);
    memset(bmp, 0xFF, M_BMP_SIZE);
    memset(bmp, 0, M_BMP_PIXELS);
    bmp[0] = 'B';
    bmp[1] = 'M';
    M_PutU32(bmp + 2, M_BMP_SIZE);
    M_PutU32(bmp + 10, M_BMP_PIXELS);
    M_PutU32(bmp + 14, 40);
    M_PutU32(bmp + 18, 2048);
    M_PutU32(bmp + 22, 2048);
    M_PutU16(bmp + 26, 1);
    M_PutU16(bmp + 28, 1);
    M_PutU32(bmp + 58, 0xFFFFFF);
    M_BmpInk(bmp, 'A' * 8, 3, 0x5A00, 8);
    M_BmpInk(bmp, '\\' * 8, 0, 0x8100, 8);
    M_BmpInk(bmp, '~' * 8, 1, 0x7E00, 8);
    M_BmpInk(bmp, 0xB1 * 8, 5, 0x3C00, 8);
    M_BmpInk(bmp, (0x24 - 0x20) * 16, 0x22 * 16 + 7, 0x1234, 16);
    M_BmpInk(bmp, (0x2D - 0x20) * 16, 0x21 * 16 + 0, 0xFF01, 16);
    M_BmpInk(bmp, (0x30 - 0x20) * 16, 0x21 * 16 + 15, 0xABCD, 16);
    M_BmpInk(bmp, (0x21 - 0x20) * 16, 0x41 * 16 + 2, 0x0FF0, 16);
    return bmp;
}

// Returns row of the cell of code_point, which must have one.
static uint16_t M_Row(
    const FONT_ROM *const rom, const uint32_t code_point, const int32_t row,
    const uint8_t width)
{
    uint16_t rows[16];
    uint8_t got = 0;
    CHECK(FontRom_Glyph(rom, code_point, rows, &got));
    CHECK_EQ_INT(got, width);
    return rows[row];
}

static bool M_Blank(const FONT_ROM *const rom, const uint32_t code_point)
{
    uint16_t rows[16];
    uint8_t width;
    CHECK(FontRom_Glyph(rom, code_point, rows, &width));
    for (int32_t i = 0; i < 16; i++) {
        if (rows[i] != 0) {
            return false;
        }
    }
    return true;
}

static void M_CheckMarkedCells(const FONT_ROM *const rom)
{
    CHECK_EQ_INT(M_Row(rom, 'A', 3, 8), 0x5A00);
    CHECK_EQ_INT(M_Row(rom, 'A', 2, 8), 0);
    CHECK_EQ_INT(M_Row(rom, '\\', 0, 8), 0x8100);
    CHECK_EQ_INT(M_Row(rom, '~', 1, 8), 0x7E00);
    CHECK_EQ_INT(M_Row(rom, U'ｱ', 5, 8), 0x3C00);
    CHECK_EQ_INT(M_Row(rom, U'あ', 7, 16), 0x1234);
    CHECK_EQ_INT(M_Row(rom, U'①', 0, 16), 0xFF01);
    CHECK_EQ_INT(M_Row(rom, U'亜', 15, 16), 0xABCD);
    CHECK_EQ_INT(M_Row(rom, U'～', 2, 16), 0x0FF0);
    CHECK(M_Blank(rom, U'い'));
}

static RESULT M_ReadT98(const uint8_t *const data, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size, "FONT.ROM");
    FONT_ROM *rom = nullptr;
    const RESULT result = FontRom_ReadT98(&reader, &rom);
    Reader_Close(&reader);
    CHECK(IS_OK(result) == (rom != nullptr));
    FontRom_Free(rom);
    return result;
}

static RESULT M_ReadAnex86(const uint8_t *const data, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size, "NEC.BMP");
    FONT_ROM *rom = nullptr;
    const RESULT result = FontRom_ReadAnex86(&reader, &rom);
    Reader_Close(&reader);
    CHECK(IS_OK(result) == (rom != nullptr));
    FontRom_Free(rom);
    return result;
}

static FONT_ROM *M_ReadBdf(const char *const full, const char *const half)
{
    READER full_reader;
    READER half_reader;
    Reader_OpenBuffer(&full_reader, full, strlen(full), "full.bdf");
    Reader_OpenBuffer(
        &half_reader, half, half != nullptr ? strlen(half) : 0, "half.bdf");
    FONT_ROM *rom = nullptr;
    const RESULT result = FontRom_ReadBdf(
        &full_reader, half != nullptr ? &half_reader : nullptr, &rom);
    Reader_Close(&full_reader);
    Reader_Close(&half_reader);
    CHECK(IS_OK(result) == (rom != nullptr));
    Result_Ignore(result);
    return rom;
}

// Returns whether a full BDF made of header and glyphs reads.
static bool M_BdfReads(const char *const header, const char *const glyphs)
{
    char *const text = Str_Format("%s%s", header, glyphs);
    FONT_ROM *const rom = M_ReadBdf(text, nullptr);
    Mem_Free(text);
    FontRom_Free(rom);
    return rom != nullptr;
}

// Returns m_BdfFullHeader with from replaced by to.
static char *M_Header(const char *const from, const char *const to)
{
    const char *const at = strstr(m_BdfFullHeader, from);
    CHECK(at != nullptr);
    return Str_Format(
        "%.*s%s%s", (int)(at - m_BdfFullHeader), m_BdfFullHeader, to,
        at + strlen(from));
}

static bool M_HeaderReads(const char *const from, const char *const to)
{
    char *const header = M_Header(from, to);
    const bool reads = M_BdfReads(header, m_BdfFullGlyphs);
    Mem_Free(header);
    return reads;
}

static bool M_GlyphReads(const char *const glyph)
{
    char *const glyphs = Str_Format(
        "CHARS 1\nSTARTCHAR x\nENCODING 9250\n%sENDCHAR\nENDFONT\n", glyph);
    const bool reads = M_BdfReads(m_BdfFullHeader, glyphs);
    Mem_Free(glyphs);
    return reads;
}

TEST(t98_reads_both_cell_sizes)
{
    uint8_t *const data = M_T98();
    READER reader;
    Reader_OpenBuffer(&reader, data, M_T98_SIZE, "FONT.ROM");
    FONT_ROM *rom = nullptr;
    CHECK(IS_OK(FontRom_ReadT98(&reader, &rom)));
    Reader_Close(&reader);
    M_CheckMarkedCells(rom);
    FontRom_Free(rom);
    Mem_Free(data);
}

TEST(t98_refuses_other_sizes)
{
    uint8_t *const data = M_T98();
    CHECK(!Result_Ignore(M_ReadT98(data, M_T98_SIZE - 1)));
    CHECK(!Result_Ignore(M_ReadT98(data, 0)));
    Mem_Free(data);
    uint8_t *const longer = Mem_AllocZ(M_T98_SIZE + 1);
    CHECK(!Result_Ignore(M_ReadT98(longer, M_T98_SIZE + 1)));
    Mem_Free(longer);
}

TEST(anex86_reads_both_cell_sizes_with_clear_bits_as_ink)
{
    uint8_t *const data = M_Anex86();
    READER reader;
    Reader_OpenBuffer(&reader, data, M_BMP_SIZE, "NEC.BMP");
    FONT_ROM *rom = nullptr;
    CHECK(IS_OK(FontRom_ReadAnex86(&reader, &rom)));
    Reader_Close(&reader);
    M_CheckMarkedCells(rom);
    FontRom_Free(rom);
    Mem_Free(data);
}

TEST(anex86_refuses_other_bmps)
{
    uint8_t *const data = M_Anex86();
    CHECK(IS_OK(M_ReadAnex86(data, M_BMP_SIZE)));
    CHECK(!Result_Ignore(M_ReadAnex86(data, M_BMP_SIZE - 1)));
    CHECK(!Result_Ignore(M_ReadAnex86(data, 20)));
    const struct {
        int32_t offset;
        uint32_t value;
        bool wide;
    } cases[] = {
        { 0, 'X', false },
        { 14, 12, true },
        { 18, 1024, true },
        { 22, 1024, true },
        { 22, (uint32_t)-2048, true },
        { 26, 2, false },
        { 28, 4, false },
        { 30, 1, true },
        { 10, M_BMP_SIZE, true },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t header[M_BMP_PIXELS];
        memcpy(header, data, sizeof(header));
        if (cases[i].wide) {
            M_PutU32(data + cases[i].offset, cases[i].value);
        } else {
            M_PutU16(data + cases[i].offset, cases[i].value);
        }
        CHECK(!Result_Ignore(M_ReadAnex86(data, M_BMP_SIZE)));
        memcpy(data, header, sizeof(header));
    }
    Mem_Free(data);
}

TEST(bdf_places_glyphs_by_bbx_and_clips_them_to_the_cell)
{
    char *const full = Str_Format("%s%s", m_BdfFullHeader, m_BdfFullGlyphs);
    FONT_ROM *const rom = M_ReadBdf(full, m_BdfHalf);
    Mem_Free(full);
    CHECK_EQ_INT(M_Row(rom, U'あ', 0, 16), 0x8001);
    CHECK_EQ_INT(M_Row(rom, U'あ', 7, 16), 0x1234);
    CHECK_EQ_INT(M_Row(rom, U'あ', 15, 16), 0xFFFF);
    CHECK_EQ_INT(M_Row(rom, U'亜', 8, 16), 0);
    CHECK_EQ_INT(M_Row(rom, U'亜', 9, 16), 0x0003);
    CHECK_EQ_INT(M_Row(rom, U'亜', 10, 16), 0x0002);
    CHECK_EQ_INT(M_Row(rom, U'亜', 11, 16), 0);
    CHECK_EQ_INT(M_Row(rom, '\\', 0, 8), 0x8100);
    CHECK_EQ_INT(M_Row(rom, '\\', 15, 8), 0x1800);
    CHECK_EQ_INT(M_Row(rom, U'ｱ', 0, 8), 0x3C00);
    CHECK_EQ_INT(M_Row(rom, U'ｱ', 1, 8), 0);
    CHECK(M_Blank(rom, 'A'));
    FontRom_Free(rom);
}

TEST(bdf_without_half_leaves_the_half_cells_blank)
{
    char *const full = Str_Format("%s%s", m_BdfFullHeader, m_BdfFullGlyphs);
    FONT_ROM *const rom = M_ReadBdf(full, nullptr);
    Mem_Free(full);
    CHECK_EQ_INT(M_Row(rom, U'あ', 7, 16), 0x1234);
    CHECK(M_Blank(rom, '\\'));
    FontRom_Free(rom);
}

TEST(bdf_refuses_other_fonts)
{
    CHECK(M_HeaderReads("JISX0208.1990", "jisx0208.1983"));
    CHECK(!M_HeaderReads("JISX0208.1990", "ISO10646"));
    CHECK(!M_HeaderReads("JISX0208.1990", "JISX0201.1976"));
    CHECK(!M_HeaderReads("CHARSET_REGISTRY", "COMMENT"));
    CHECK(!M_HeaderReads("PIXEL_SIZE 16", "PIXEL_SIZE 14"));
    CHECK(!M_HeaderReads("FONT_DESCENT 2", "FONT_DESCENT 4"));
    CHECK(!M_HeaderReads("STARTFONT", "STARTPROPERTIES"));
    FONT_ROM *const rom = M_ReadBdf(m_BdfHalf, nullptr);
    CHECK(rom == nullptr);
    char *const full = Str_Format("%s%s", m_BdfFullHeader, m_BdfFullGlyphs);
    CHECK(M_ReadBdf(full, full) == nullptr);
    Mem_Free(full);
}

TEST(bdf_refuses_bad_glyphs)
{
    CHECK(M_GlyphReads("BBX 16 1 0 0\nBITMAP\nFFFF\n"));
    CHECK(M_GlyphReads("BBX 0 0 0 0\nBITMAP\n"));
    CHECK(!M_GlyphReads("BBX 16 1 0\nBITMAP\nFFFF\n"));
    CHECK(!M_GlyphReads("BBX 16 1 0 0 0\nBITMAP\nFFFF\n"));
    CHECK(!M_GlyphReads("BBX -1 1 0 0\nBITMAP\nFFFF\n"));
    CHECK(!M_GlyphReads("BBX 16 -1 0 0\nBITMAP\n"));
    CHECK(!M_GlyphReads("BBX 99 1 0 0\nBITMAP\nFFFF\n"));
    CHECK(!M_GlyphReads("BBX x 1 0 0\nBITMAP\nFFFF\n"));
    CHECK(!M_GlyphReads("BITMAP\nFFFF\n"));
    CHECK(!M_GlyphReads("BBX 16 2 0 0\nBITMAP\nFFFF\n"));
    CHECK(!M_GlyphReads("BBX 16 1 0 0\nBITMAP\nFFFF\nFFFF\n"));
    CHECK(!M_GlyphReads("BBX 16 1 0 0\nBITMAP\nFF\n"));
    CHECK(!M_GlyphReads("BBX 16 1 0 0\nBITMAP\nFFXF\n"));
}

TEST(characters_without_a_cell)
{
    uint8_t *const data = M_T98();
    READER reader;
    Reader_OpenBuffer(&reader, data, M_T98_SIZE, "FONT.ROM");
    FONT_ROM *rom = nullptr;
    CHECK(IS_OK(FontRom_ReadT98(&reader, &rom)));
    Reader_Close(&reader);
    Mem_Free(data);
    const uint32_t none[] = {
        0x00, 0x1F, 0x7F, U'é', U'¥', U'ⅰ', U'纊', 0xE000, 0xD800, 0x1F600,
    };
    for (size_t i = 0; i < sizeof(none) / sizeof(none[0]); i++) {
        uint16_t rows[16];
        memset(rows, 0xFF, sizeof(rows));
        uint8_t width = 0;
        CHECK(!FontRom_Glyph(rom, none[i], rows, &width));
        CHECK_EQ_INT(rows[0], 0);
    }
    FontRom_Free(rom);
}

TEST(open_finds_the_file_in_the_registry_and_tells_its_format)
{
    FontRegistry_Reset(false);
    uint8_t *const t98 = M_T98();
    uint8_t *const bmp = M_Anex86();
    char *const full = Str_Format("%s%s", m_BdfFullHeader, m_BdfFullGlyphs);
    FontRegistry_AddFile("FONT.ROM", "test", t98, M_T98_SIZE);
    FontRegistry_AddFile("NEC.BMP", "test", bmp, M_BMP_SIZE);
    FontRegistry_AddFile("full.bdf", "test", full, strlen(full));
    FontRegistry_AddFile("half.bdf", "test", m_BdfHalf, strlen(m_BdfHalf));
    FontRegistry_AddFile("junk.rom", "test", "junk", 4);
    Mem_Free(full);
    Mem_Free(bmp);
    Mem_Free(t98);

    FONT_ROM *rom = nullptr;
    CHECK(IS_OK(FontRom_Open("font.rom", nullptr, &rom)));
    M_CheckMarkedCells(rom);
    FontRom_Free(rom);

    CHECK(IS_OK(FontRom_Open("Nec.Bmp", nullptr, &rom)));
    M_CheckMarkedCells(rom);
    FontRom_Free(rom);

    CHECK(IS_OK(FontRom_Open("FULL.BDF", "HALF.BDF", &rom)));
    CHECK_EQ_INT(M_Row(rom, U'あ', 7, 16), 0x1234);
    CHECK_EQ_INT(M_Row(rom, '\\', 0, 8), 0x8100);
    FontRom_Free(rom);

    CHECK(IS_OK(FontRom_Open("full.bdf", nullptr, &rom)));
    CHECK(M_Blank(rom, '\\'));
    FontRom_Free(rom);

    CHECK(!Result_Ignore(FontRom_Open("ANEX86.BMP", nullptr, &rom)));
    CHECK(rom == nullptr);
    CHECK(!Result_Ignore(FontRom_Open("full.bdf", "missing.bdf", &rom)));
    CHECK(rom == nullptr);
    CHECK(!Result_Ignore(FontRom_Open("junk.rom", nullptr, &rom)));
    CHECK(rom == nullptr);
    CHECK(!Result_Ignore(FontRom_Open("half.bdf", nullptr, &rom)));
    CHECK(rom == nullptr);
    FontRegistry_Reset(false);
}
