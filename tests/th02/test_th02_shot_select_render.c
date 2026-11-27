#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "th02/shot_select/logic.h"

#include "core/base/arena.h"

typedef struct {
    ARENA arena;

    TH02_SESSION session;
    SHOT_SELECT_ASSETS assets;
    uint8_t background[32];
    uint8_t portrait[32];
} FIXTURE;

static void M_Begin(FIXTURE *const fixture)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    Grcg_Init();
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "Shot renderer test", 1024 * 1024);
    Video_Init(Arena_Allocator(&fixture->arena));
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    g_Video.visible_page = 1;
    Video_SetSplit(123, 456, 789);
    Clip_Reset();
    Palette_Init();
    fixture->session.shot_type = 2;
    fixture->session.frame = 1234;
    memset(fixture->background, 3, sizeof(fixture->background));
    memset(fixture->portrait, 10, sizeof(fixture->portrait));
    fixture->assets.background.view =
        (TITLE_FLASH_IMAGE) { fixture->background, 16, 2 };
    for (size_t i = 0; i < 3; i++) {
        fixture->assets.portraits[i] =
            (TITLE_FLASH_IMAGE) { fixture->portrait, 16, 2 };
    }
    const bool cleared[3] = {};
    ShotSelect_Begin(&fixture->session, &fixture->assets, cleared);
}

static int M_Fade(void)
{
    int edges = 0;
    do {
        edges++;
    } while (!ShotSelect_VSync() && edges < 100);
    return edges;
}

TEST(shot_init_keeps_clean_background_page_and_darkens_alternate_portrait_dots)
{
    FIXTURE fixture;
    M_Begin(&fixture);
    CHECK_EQ_INT(g_Video.pages[1][0], 3);
    CHECK_EQ_INT(g_Video.pages[1][32000], 9);
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_VISIBLE_SIZE], 6);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE], 9);
    CHECK_EQ_INT(g_Video.pages[0][136 * 640 + 24], 10);
    CHECK_EQ_INT(g_Video.pages[0][136 * 640 + 25], 0);
    CHECK_EQ_INT(g_Video.pages[0][137 * 640 + 24], 0);
    CHECK_EQ_INT(g_Video.pages[0][137 * 640 + 25], 10);
    CHECK_EQ_INT(g_Video.pages[0][224 * 640 + 224], 10);
    CHECK_EQ_INT(g_Video.pages[0][136 * 640 + 440], 10);
    CHECK_EQ_INT(fixture.session.shot_type, 1);
    CHECK_EQ_INT(fixture.session.frame, 1234);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(m_Palette.tone, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(M_Fade(), 35);
    CHECK_EQ_INT(m_Palette.tone, 100);
    CHECK(ShotSelect_VSync());
    for (size_t i = 0; i < 3; i++) {
        fixture.assets.portraits[i].pixels = nullptr;
    }
    ShotSelect_BeginFadeOut();
    CHECK_EQ_INT(M_Fade(), 18);
    CHECK_EQ_INT(m_Palette.tone, 0);
    CHECK(ShotSelect_VSync());
    Arena_Free(&fixture.arena);
}

TEST(shot_highlight_restores_only_the_source_bottom_and_right_borders)
{
    FIXTURE fixture;
    M_Begin(&fixture);
    M_Fade();
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    g_Video.drawing_page = 1;
    const GRCG before = *g_Grcg;
    ShotSelectRender_Background(1, true);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % 640;
        const size_t y = pixel / 640;
        const bool border = (x >= 416 && x < 432 && y >= 232 && y < 376)
            || (x >= 232 && x < 424 && y >= 368 && y < 376);
        CHECK_EQ_INT(g_Video.pages[0][pixel], border ? 6 : 9);
        CHECK_EQ_INT(g_Video.pages[1][pixel], 6);
    }
    CHECK(memcmp(g_Grcg, &before, sizeof(before)) == 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    Arena_Free(&fixture.arena);
}

TEST(shot_checkerboard_ignores_clip_and_preserves_the_other_page)
{
    FIXTURE fixture;
    M_Begin(&fixture);
    M_Fade();
    TestClip_Set(5, 7, 13, 8);
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    g_Video.drawing_page = 1;
    ShotSelectRender_Darken(24, 136);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % 640;
        const size_t y = pixel / 640;
        const bool covered =
            x >= 24 && x < 216 && y >= 136 && y < 280 && ((x + y) & 1) != 0;
        CHECK_EQ_INT(g_Video.pages[1][pixel], covered ? 0 : 7);
        CHECK_EQ_INT(g_Video.pages[0][pixel], 9);
    }
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(g_Grcg->tiles[i], 0);
    }
    Arena_Free(&fixture.arena);
}
