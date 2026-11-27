#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/main_menu/render.h"

#include "core/base/arena.h"

#include <string.h>

typedef struct {
    ARENA arena;

    GAIJI glyphs;

    TH02_SETTINGS settings;
    bool extra_unlocked;
    TITLE_IMAGE background;
    GFX_COLOR menu_palette[16];
    uint8_t pixels[32];
    TITLE_ASSETS assets;
    TRAM_CONSOLE *console;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    memset(p_Palette, 0, sizeof((*p_Palette)));
    Grcg_Init();
    Tram_Init();
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "Menu renderer test", 1024 * 1024);
    Video_Init(Arena_Allocator(&fixture->arena));
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    memset(&m_Tram, 0x99, sizeof(m_Tram));
    TestClip_Set(0, 0, 639, 399);
    Palette_Init();
    fixture->settings.difficulty = TH02_DIFFICULTY_NORMAL;
    fixture->background.view = (TITLE_FLASH_IMAGE) { fixture->pixels, 16, 2 };
    for (size_t i = 0; i < 16; i++) {
        fixture->menu_palette[i] = (GFX_COLOR) { 255, 0, 0, 255 };
    }
    fixture->assets.glyphs = &fixture->glyphs;
    fixture->assets.menu_background = fixture->background;
    memcpy(
        fixture->assets.menu_palette, fixture->menu_palette,
        sizeof(fixture->menu_palette));
}

TEST(menu_clear_uses_live_console_state_instead_of_the_fallback_attribute)
{
    FIXTURE fixture;
    M_Init(&fixture);
    TRAM_CONSOLE console;
    TramConsole_Init(&console, 0xc1);
    console.cursor_visible = console.system_line_visible = false;
    console.cursor_column = 61;
    console.cursor_row = 19;
    fixture.console = &console;
    Menu_Clear(fixture.console, 5);
    CHECK_EQ_INT(console.cursor_column, 0);
    CHECK_EQ_INT(console.cursor_row, 0);
    CHECK(!console.cursor_visible && !console.system_line_visible);
    CHECK_EQ_INT(m_Tram.attributes[0], 0xc1);
    CHECK_EQ_INT(m_Tram.attributes[1999], 0xc1);
    CHECK_EQ_INT(m_Tram.attributes[2000], 0x9999);
    CHECK_EQ_INT(g_Video.pages[0][0], 9);
    CHECK_EQ_INT(g_Video.pages[1][0], 6);
    Arena_Free(&fixture.arena);
}

TEST(menu_labels_use_original_glyph_slots_rank_positions_and_lock_color)
{
    FIXTURE fixture;
    M_Init(&fixture);
    MainMenuRender_Put(&fixture.settings, fixture.extra_unlocked, 0, 0xe1);
    CHECK_EQ_INT(m_Tram.codes[16 * 80 + 35], 0x3c57);
    CHECK_EQ_INT(m_Tram.codes[16 * 80 + 36], 0xbc57);
    CHECK_EQ_INT(m_Tram.attributes[16 * 80 + 35], 0xe1);
    CHECK_EQ_INT(m_Tram.codes[23 * 80 + 26], 0x3b57);
    CHECK_EQ_INT(m_Tram.attributes[23 * 80 + 26], 0x81);
    CHECK_EQ_INT(m_Tram.codes[23 * 80 + 40], 0x3657);
    CHECK_EQ_INT(m_Tram.codes[23 * 80 + 46], 0x3757);
    MainMenuRender_Put(&fixture.settings, fixture.extra_unlocked, 1, 0x35e1);
    CHECK_EQ_INT(m_Tram.attributes[17 * 80 + 29], 0x21);
    fixture.extra_unlocked = true;
    MainMenuRender_Put(&fixture.settings, fixture.extra_unlocked, 1, 0x35e1);
    CHECK_EQ_INT(m_Tram.attributes[17 * 80 + 29], 0x35e1);
    MainMenuRender_Put(&fixture.settings, fixture.extra_unlocked, -1, 0);
    CHECK_EQ_INT(m_Tram.attributes[23 * 80 + 38], 0x81);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 9);
        CHECK_EQ_INT(g_Video.pages[1][i], 6);
    }
    Arena_Free(&fixture.arena);
}

TEST(menu_init_copies_visible_background_and_keeps_hidden_video_and_text_tails)
{
    FIXTURE fixture;
    M_Init(&fixture);
    Grcg_SetColor(15);
    p_Palette->tone = 50;
    g_Video.visible_page = g_Video.drawing_page = 1;
    Video_SetSplit(123, 456, 789);
    MainMenuRender_Init(&fixture.assets, &fixture.settings, fixture.console, 5);
    CHECK_EQ_INT(p_Palette->tone, 50);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(g_Grcg->tiles[i], 0);
    }
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

TEST(score_return_keeps_text_and_tone_while_restoring_both_visible_pages)
{
    FIXTURE fixture;
    M_Init(&fixture);
    memset(fixture.pixels, 3, sizeof(fixture.pixels));
    p_Palette->tone = 50;
    MainMenuRender_Restore(&fixture.assets);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(p_Palette->tone, 50);
    for (size_t i = 0; i < TRAM_PLANE_CELLS; i++) {
        CHECK_EQ_INT(m_Tram.codes[i], 0x9999);
        CHECK_EQ_INT(m_Tram.attributes[i], 0x9999);
    }
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        const bool image = i / 640 < 2 && i % 640 < 16;
        CHECK_EQ_INT(g_Video.pages[1][i], image ? 3 : 6);
        CHECK_EQ_INT(
            g_Video.pages[0][i],
            i >= VIDEO_VISIBLE_SIZE ? 9
                : image             ? 3
                                    : 6);
    }
    for (size_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(g_Video.palette[i].red, 119);
        CHECK_EQ_INT(g_Video.palette[i].green, 0);
        CHECK_EQ_INT(g_Video.palette[i].blue, 0);
        CHECK_EQ_INT(p_Palette->colors[i][0], 255);
    }
    Arena_Free(&fixture.arena);
}
