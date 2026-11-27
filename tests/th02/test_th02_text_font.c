#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "th02/grcg.h"
#include "th02/text_font.h"

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/io/reader.h"
#include "core/text/font_registry.h"

static void M_SetCell(
    uint8_t *const bytes, const uint16_t address, const uint16_t bits)
{
    const size_t offset =
        0x1800 + (((address >> 8) - 0x21) * 96 + (address & 255) - 0x20) * 32;
    bytes[offset] = bits >> 8;
    bytes[offset + 16] = bits & 255;
}

static RESULT M_ReadText(
    TEXT_FONT *const font, const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, DRAW_TEXT *const out)
{
    const DRAW_TEXT_FONT view = TextFont_Graphics(font);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    const RESULT result = DrawText_Read(&reader, size, &view, allocator, out);
    Reader_Close(&reader);
    return result;
}

TEST(registered_rom_drives_both_planes_without_merging_unicode_aliases)
{
    FontRegistry_Reset(false);
    uint8_t *const bytes = Mem_AllocZ(288768);
    bytes[0x800 + 'A' * 16] = 0x80;
    M_SetCell(bytes, 0x2941, 0x4000);
    M_SetCell(bytes, 0x2262, 0x2001);
    M_SetCell(bytes, 0x2d70, 0x1002);
    FontRegistry_AddFile("font.rom", "test", bytes, 288768);
    Mem_Free(bytes);
    TEXT_FONT font;
    CHECK(Result_Ignore(TextFont_Open(&font)));
    ARENA arena;
    Arena_Init(&arena, "text font integration", 1024 * 1024);
    const uint8_t field[] = { 'A', 0x81, 0xe0, 0x87, 0x90 };
    DRAW_TEXT text;
    CHECK(Result_Ignore(M_ReadText(
        &font, field, sizeof(field), Arena_Allocator(&arena), &text)));
    CHECK_EQ_INT(text.count, 3);
    CHECK_EQ_INT(text.glyphs[0].rows[0], 0x4000);
    CHECK_EQ_INT(text.glyphs[1].rows[0], 0x2001);
    CHECK_EQ_INT(text.glyphs[2].rows[0], 0x1002);
    const TRAM_FONT view = TextFont_Tram(&font);
    CHECK_EQ_INT(view.row(view.context, 'A', false, 0), 0x8000);
    CHECK_EQ_INT(view.row(view.context, 0x6202, true, 0), 0x2001);
    CHECK_EQ_INT(view.row(view.context, 0x700d, true, 0), 0x1002);
    CHECK_EQ_INT(view.row(view.context, 0, true, 0), 0);
    CHECK_EQ_INT(view.row(view.context, 256, false, 0), 0);

    Tram_Init();
    Tram_PutText(0, 0, &text.cells, UINT16_MAX, 0xe1);
    GAIJI gaiji = {};
    uint8_t *const pixels = Mem_Alloc(VIDEO_VISIBLE_SIZE);
    Tram_Scanout(&gaiji, &view, 0, true, pixels, VIDEO_WIDTH);
    CHECK_EQ_INT(pixels[0], 8);
    CHECK_EQ_INT(pixels[1], 0);
    CHECK_EQ_INT(pixels[10], 8);
    CHECK_EQ_INT(pixels[23], 8);
    CHECK_EQ_INT(pixels[27], 8);
    CHECK_EQ_INT(pixels[38], 8);
    Mem_Free(pixels);
    TextFont_Close(&font);
    CHECK(font.rom == nullptr);
    FontRegistry_Reset(false);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();

    Draw_Text(text.glyphs, text.count, 0, 0, 7);
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    CHECK_EQ_INT(g_Video.pages[0][1], 7);
    CHECK_EQ_INT(g_Video.pages[0][10], 7);
    CHECK_EQ_INT(g_Video.pages[0][23], 7);
    CHECK_EQ_INT(g_Video.pages[0][27], 7);
    CHECK_EQ_INT(g_Video.pages[0][38], 7);
    Arena_Free(&arena);
}

TEST(bundled_bdf_names_supply_graphics_ascii_after_a_bad_preferred_font)
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
    FontRegistry_Reset(false);
    FontRegistry_AddFile("FONT.ROM", "test", "bad", 3);
    FontRegistry_AddFile("shnmk16.bdf", "test", full, sizeof(full) - 1);
    FontRegistry_AddFile("shnm8x16r.bdf", "test", half, sizeof(half) - 1);
    TEXT_FONT font;
    CHECK(Result_Ignore(TextFont_Open(&font)));
    const TRAM_FONT font_view = TextFont_Tram(&font);
    CHECK_EQ_INT(font_view.row(font_view.context, 'A', false, 0), 0xa500);
    const DRAW_TEXT_FONT graphics = TextFont_Graphics(&font);
    uint16_t rows[16];
    const DRAW_TEXT_CHARACTER character = { 0x0941, false, "A", 1 };
    CHECK(Result_Ignore(graphics.load(graphics.context, &character, rows)));
    CHECK_EQ_INT(rows[0], 0xa500);
    TextFont_Close(&font);
    FontRegistry_Reset(false);
}

TEST(missing_fonts_and_unsupported_graphics_selectors_fail_explicitly)
{
    FontRegistry_Reset(false);
    TEXT_FONT font;
    CHECK(!Result_Ignore(TextFont_Open(&font)));
    CHECK(font.rom == nullptr);
    TextFont_Close(&font);
    uint8_t *const bytes = Mem_AllocZ(288768);
    FontRegistry_AddFile("FONT.ROM", "test", bytes, 288768);
    Mem_Free(bytes);
    CHECK(Result_Ignore(TextFont_Open(&font)));
    ARENA arena;
    Arena_Init(&arena, "invalid text font selector", 8192);
    const uint8_t field[] = { 0xf0, 0x40 };
    DRAW_TEXT text;
    CHECK(!Result_Ignore(M_ReadText(
        &font, field, sizeof(field), Arena_Allocator(&arena), &text)));
    CHECK_EQ_INT(arena.used, 0);
    CHECK(text.glyphs == nullptr && text.cells.text == nullptr);
    TextFont_Close(&font);
    FontRegistry_Reset(false);
    Arena_Free(&arena);
}
