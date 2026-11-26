#include "th02_tram_fixture.h"
#include "th02/menu.h"
#include "th02/options_menu/render.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    ARENA arena;

    GAIJI glyphs;
    TH02_SETTINGS settings;
    OPTIONS_MENU_TEXT text[3];
    TRAM_CONSOLE *console;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    Grcg_Init();
    Tram_Init();
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "Options renderer test", 1024 * 1024);
    Video_Init(Arena_Allocator(&fixture->arena));
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    memset(&m_Tram, 0x99, sizeof(m_Tram));
    fixture->settings = (TH02_SETTINGS) { 1, 1, 3, 2, false, true };
}

TEST(options_clear_uses_the_shared_console_cursor_and_live_attribute)
{
    FIXTURE fixture;
    M_Init(&fixture);
    TRAM_CONSOLE console;
    TramConsole_Init(&console, 0x81);
    console.cursor_column = 31;
    console.cursor_row = 7;
    console.cursor_visible = console.system_line_visible = false;
    fixture.console = &console;
    Menu_Clear(fixture.console, 5);
    CHECK_EQ_INT(console.cursor_column, 0);
    CHECK_EQ_INT(console.cursor_row, 0);
    CHECK_EQ_INT(m_Tram.attributes[0], 0x81);
    CHECK_EQ_INT(m_Tram.attributes[2000], 0x9999);
    CHECK_EQ_INT(g_Video.pages[0][0], 9);
    CHECK(!console.cursor_visible && !console.system_line_visible);
    Arena_Free(&fixture.arena);
}

TEST(option_values_restore_the_source_width_and_select_page_zero)
{
    FIXTURE fixture;
    M_Init(&fixture);
    g_Video.drawing_page = g_Video.visible_page = 1;
    fixture.glyphs.rows[0xa3][0] = 0x8000;
    OptionsMenuRender_Put(
        &fixture.settings, &fixture.glyphs, fixture.text, 2, 0x35e1);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(m_Tram.codes[18 * 80 + 49], 0x2357);
    CHECK_EQ_INT(m_Tram.attributes[18 * 80 + 49], 0x35e1);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        const bool restored =
            i / 640 >= 292 && i / 640 < 308 && i % 640 >= 392 && i % 640 < 424;
        CHECK_EQ_INT(
            g_Video.pages[0][i],
            i == 292 * 640 + 396 ? 0
                : restored       ? 6
                                 : 9);
        CHECK_EQ_INT(g_Video.pages[1][i], 6);
    }
    Arena_Free(&fixture.arena);
}

TEST(option_rank_and_music_keep_source_alignment_and_swapped_font_slots)
{
    FIXTURE fixture;
    M_Init(&fixture);
    OptionsMenuRender_Put(
        &fixture.settings, &fixture.glyphs, fixture.text, 0, 0xc1);
    CHECK_EQ_INT(m_Tram.codes[16 * 80 + 42], 0x4f57);
    CHECK_EQ_INT(m_Tram.codes[16 * 80 + 44], 0x3657);
    CHECK_EQ_INT(m_Tram.codes[16 * 80 + 50], 0x3757);
    fixture.settings.music = TH02_MUSIC_MIDI;
    OptionsMenuRender_Put(
        &fixture.settings, &fixture.glyphs, fixture.text, 1, 0xe1);
    CHECK_EQ_INT(m_Tram.codes[17 * 80 + 24], 0x3757);
    CHECK_EQ_INT(m_Tram.codes[17 * 80 + 47], 0x3757);
    CHECK_EQ_INT(m_Tram.codes[17 * 80 + 53], 0x3257);
    CHECK_EQ_INT(m_Tram.attributes[17 * 80 + 54], 0xe1);
    Arena_Free(&fixture.arena);
}

TEST(option_commands_preserve_charger_and_init_keeps_hidden_tails)
{
    FIXTURE fixture;
    M_Init(&fixture);
    Grcg_SetColor(7);
    Grcg_SetPlanes(1);
    const GRCG before = *g_Grcg;
    OptionsMenuRender_Put(
        &fixture.settings, &fixture.glyphs, fixture.text, 5, 0xc1);
    OptionsMenuRender_Put(
        &fixture.settings, &fixture.glyphs, fixture.text, 6, 0xe1);
    OptionsMenuRender_Put(
        &fixture.settings, &fixture.glyphs, fixture.text, -1, 0);
    CHECK(memcmp(g_Grcg, &before, sizeof(before)) == 0);
    CHECK_EQ_INT(m_Tram.codes[21 * 80 + 35], 0x3b57);
    CHECK_EQ_INT(m_Tram.codes[23 * 80 + 36], 0x3a57);
    OptionsMenuRender_Init(&fixture.glyphs, fixture.text, fixture.console, 5);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], i < VIDEO_VISIBLE_SIZE ? 6 : 9);
        CHECK_EQ_INT(g_Video.pages[1][i], 6);
    }
    for (size_t i = 0; i < TRAM_PLANE_CELLS; i++) {
        CHECK_EQ_INT(m_Tram.codes[i], i < 2000 ? ' ' : 0x9999);
        CHECK_EQ_INT(m_Tram.attributes[i], i < 2000 ? 5 : 0x9999);
    }
    Arena_Free(&fixture.arena);
}
