#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_music_room_render.h"
#include "tests/th02/th02_palette_fixture.h"

typedef struct {
    MUSIC_ROOM_ASSETS assets;
    TITLE_ASSETS menu;
    uint8_t background[VIDEO_VISIBLE_SIZE];
    uint8_t menu_pixels[VIDEO_VISIBLE_SIZE];
    uint16_t rows[16];
    DRAW_TEXT_GLYPH glyph;
} FIXTURE;

static FIXTURE *M_Init(void)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    FIXTURE *const fixture = calloc(1, sizeof(*fixture));
    CHECK(fixture != nullptr);
    Video_Init(&g_HeapAllocator);
    for (int page = 0; page < 2; page++) {
        memset(g_Video.pages[page], 6, VIDEO_PAGE_SIZE);
    }
    Video_SetSplit(123, 41, 82);
    Clip_Reset();
    Grcg_Init();
    Palette_Init();
    memset(fixture->background, 2, sizeof(fixture->background));
    memset(fixture->menu_pixels, 4, sizeof(fixture->menu_pixels));
    fixture->assets.background.view =
        (TITLE_FLASH_IMAGE) { fixture->background, 640, 400 };
    fixture->menu.menu_background.view =
        (TITLE_FLASH_IMAGE) { fixture->menu_pixels, 640, 400 };
    for (int i = 0; i < 16; i++) {
        fixture->rows[i] = 0x8000;
        fixture->assets.background.palette[i] =
            (GFX_COLOR) { i * 17, 0, 0, 255 };
        fixture->menu.menu_background.palette[i] =
            fixture->menu.menu_palette[i] = (GFX_COLOR) { 0, i * 17, 0, 255 };
    }
    fixture->glyph = (DRAW_TEXT_GLYPH) { fixture->rows, false };
    for (int i = 0; i <= MUSIC_ROOM_QUIT; i++) {
        fixture->assets.choices[i].glyphs = &fixture->glyph;
        fixture->assets.choices[i].count = 1;
    }
    fixture->assets.comments[0][0].glyphs = &fixture->glyph;
    fixture->assets.comments[0][0].count = 1;
    fixture->assets.comments[0][1].glyphs = &fixture->glyph;
    fixture->assets.comments[0][1].count = 1;
    MusicRoomRender_Init();
    TestMusicRoomRender_State()->seed = 42;
    return fixture;
}

static void M_Free(FIXTURE *const fixture)
{
    Mem_Free(g_Video.pages[0]);
    free(fixture);
}

static void M_Hidden(const FIXTURE *const fixture)
{
    for (int page = 0; page < 2; page++) {
        for (size_t i = VIDEO_VISIBLE_SIZE; i < VIDEO_PAGE_SIZE; i++) {
            CHECK_EQ_INT(g_Video.pages[page][i], 6);
        }
    }
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 41);
    CHECK_EQ_INT(g_Video.second_address, 82);
}

TEST(begin_composes_both_pages_and_keeps_the_initial_page_marker)
{
    FIXTURE *const fixture = M_Init();
    MusicRoomRender_Begin(&fixture->assets, &fixture->menu, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(TestMusicRoomRender_State()->logical_page, 1);
    CHECK_EQ_INT(m_Palette.tone, 100);
    CHECK_EQ_INT(g_Video.palette[15].red, 255);
    CHECK_EQ_INT(TestMusicRoomRender_State()->seed, 42);
    CHECK(!TestMusicRoomRender_State()->polygons.initialized);
    CHECK(memcmp(g_Video.pages[0], g_Video.pages[1], VIDEO_VISIBLE_SIZE) == 0);
    CHECK_EQ_INT(g_Video.pages[0][96 * 640 + 16], 15);
    CHECK_EQ_INT(g_Video.pages[0][112 * 640 + 16], 3);
    CHECK_EQ_INT(g_Video.pages[0][64 * 640 + 160], 15);
    CHECK_EQ_INT(g_Video.pages[0][80 * 640 + 304], 13);
    CHECK_EQ_INT(g_Video.pages[0][0], 2);
    M_Hidden(fixture);
    M_Free(fixture);
}

TEST(comment_erases_old_text_and_track_redraw_leaves_logical_page_accessed)
{
    FIXTURE *const fixture = M_Init();
    MusicRoomRender_Begin(&fixture->assets, &fixture->menu, 0);
    MusicRoomRender_Track(0, 3);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    for (int page = 0; page < 2; page++) {
        CHECK_EQ_INT(g_Video.pages[page][96 * 640 + 16], 3);
    }
    MusicRoomRender_Comment(1);
    CHECK_EQ_INT(g_Video.pages[1][64 * 640 + 160], 2);
    CHECK_EQ_INT(g_Video.pages[1][80 * 640 + 304], 2);
    CHECK_EQ_INT(g_Video.pages[0][64 * 640 + 160], 15);
    CHECK_EQ_INT(g_Video.pages[1][96 * 640 + 16], 3);
    M_Hidden(fixture);
    M_Free(fixture);
}

TEST(render_restores_only_blue_and_keeps_animation_across_visits)
{
    FIXTURE *const fixture = M_Init();
    MusicRoomRender_Begin(&fixture->assets, &fixture->menu, 0);
    TestMusicRoomRender_State()->polygons.initialized = true;
    for (int i = 0; i < MUSIC_POLYGON_COUNT; i++) {
        TestMusicRoomRender_State()->polygons.polygons[i].x = 300;
        TestMusicRoomRender_State()->polygons.polygons[i].y_subpixel = -32000;
        TestMusicRoomRender_State()->polygons.polygons[i].velocity_x = 1;
    }
    MusicRoomRender_Flip(1, 0);
    g_Video.pages[1][0] = 15;
    MusicRoomRender_Frame();
    CHECK_EQ_INT(g_Video.pages[1][0], 14);
    CHECK_EQ_INT(g_Video.pages[0][0], 2);
    CHECK_EQ_INT(TestMusicRoomRender_State()->polygons.polygons[0].x, 301);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (int i = 0; i < 4; i++) {
        CHECK_EQ_INT(g_Grcg->tiles[i], 255);
    }
    MusicRoomRender_End();
    MusicRoomRender_Begin(&fixture->assets, &fixture->menu, 0);
    CHECK_EQ_INT(TestMusicRoomRender_State()->polygons.polygons[0].x, 301);
    CHECK_EQ_INT(TestMusicRoomRender_State()->seed, 42);
    M_Hidden(fixture);
    M_Free(fixture);
}

TEST(end_restores_menu_pages_palette_and_zero_charger_tiles)
{
    FIXTURE *const fixture = M_Init();
    MusicRoomRender_Begin(&fixture->assets, &fixture->menu, 0);
    MusicRoomRender_End();
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.palette[15].green, 255);
    CHECK_EQ_INT(g_Video.palette[15].red, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (int i = 0; i < 4; i++) {
        CHECK_EQ_INT(g_Grcg->tiles[i], 0);
    }
    for (int page = 0; page < 2; page++) {
        for (size_t i = 0; i < VIDEO_VISIBLE_SIZE; i++) {
            CHECK_EQ_INT(g_Video.pages[page][i], 4);
        }
    }
    M_Hidden(fixture);
    M_Free(fixture);
}
