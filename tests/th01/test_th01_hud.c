#include "tests/test.h"
#include "tests/th01/th01_text_fixture.h"
#include "tests/th01/th01_video.h"
#include "tests/th01/th01_world.h"
#include "th01/hud.c"
#include "th01/hud.h"
#include "th01/orb.h"
#include "th01/pellets.h"
#include "th01/player.c"
#include "th01/player.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/shots.h"

#include "core/base/arena.h"

#include <string.h>

typedef struct {
    ARENA arena;
    PTN sprites;
    GRF mask;
    GAME_TEXT_FONT font;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    Arena_Init(&f->arena, "hud", 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(a);
    Hud_Init(a);
    GAME_TEXT_GLYPH glyphs[105] = {};
    for (int32_t i = 0; i < 105; i++) {
        glyphs[i].codepoint = (uint32_t)(i < 95 ? i + 32 : i - 95 + 0xff10);
        glyphs[i].width = i < 95 ? 8 : 16;
        glyphs[i].rows[0] = i < 95 ? 0x8000 : (uint16_t)(0x8000u >> (i - 95));
    }
    glyphs[0].rows[0] = 0;
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 105, a, &f->font)));
    f->sprites = (PTN) { .count = 1, .pixels = Allocator_Alloc(a, 1024) };
    memset(f->sprites.pixels, 15, 1024);
    for (int32_t y = 0; y < 16; y++) {
        f->sprites.pixels[y * 32] = 3;
        f->sprites.pixels[y * 32 + 16] = 4;
    }
    f->mask.coverage = Allocator_AllocZ(a, 640 * 66);
    Hud_Load(&f->mask, &f->sprites, &f->font);
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_NORMAL;
    CHECK(Result_Ignore(Hud_ValidateAssets(&f->mask, &f->sprites, &f->font)));
}

TEST(mask_covers_playfield_overlap_and_preserves_clear_dots)
{
    FIXTURE f;
    M_Init(&f);
    memset(g_Video.pages[0], 2, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 5, VIDEO_PAGE_SIZE);
    f.mask.coverage[65 * 640 + 639] = 1;
    Hud_PutMask();
    CHECK_EQ_INT(Th01Test_Pixel(0, 639, 65), 10);
    CHECK_EQ_INT(Th01Test_Pixel(1, 639, 65), 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 638, 65), 2);
    CHECK_EQ_INT(Th01Test_Pixel(1, 639, 66), 5);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&f.arena);
}

TEST(resources_snap_before_selection_and_restore_opaque_color15)
{
    FIXTURE f;
    M_Init(&f);
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    g_Session.lives = 2;
    m_Player.remaining_bombs = 1;
    Hud_SnapResources();
    CHECK_EQ_INT(Th01Test_Pixel(1, 128, 0), 3);
    CHECK_EQ_INT(Th01Test_Pixel(1, 129, 0), 15);
    g_Session.lives = 1;
    Hud_LivesPut(2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 144, 0), 15);
    CHECK_EQ_INT(Th01Test_Pixel(1, 144, 0), 15);
    Video_SelectDrawingPage(1);
    g_Session.lives = 2;
    Hud_LivesPut(1);
    CHECK_EQ_INT(m_Hud.lives[256], 15);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    m_Player.remaining_bombs = 0;
    Hud_BombsPut(1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 128, 16), 15);
    Arena_Free(&f.arena);
}

TEST(timer_preserves_duplicate_snapshot_zero_spaces_and_red_threshold)
{
    FIXTURE f;
    M_Init(&f);
    for (int32_t y = 0; y < 16; y++) {
        for (int32_t x = 512; x < 544; x++) {
            g_Video.pages[0][y * 640 + x] = x < 528 ? 2 : 3;
        }
    }
    Hud_SnapTimer(1000);
    Hud_PutTimer(0);
    for (int32_t page = 0; page < 2; page++) {
        CHECK_EQ_INT(Th01Test_Pixel(page, 528, 1), 2);
        CHECK_EQ_INT(Th01Test_Pixel(page, 512, 0), 2);
    }
    Hud_PutTimer(198);
    CHECK_EQ_INT(Th01Test_Pixel(0, 520, 0), 10);
    Hud_PutTimer(200);
    CHECK_EQ_INT(Th01Test_Pixel(0, 520, 0), 7);
    Arena_Free(&f.arena);
}

TEST(force_draws_leading_zeroes_and_preserves_resident_high_score)
{
    FIXTURE f;
    M_Init(&f);
    g_Session.high_score = 100;
    Hud_SnapNumbers(true);
    g_Stage.score = 1;
    m_Hud.force_numbers = true;
    Hud_ScoreAndCombo();
    CHECK_EQ_INT(g_Session.high_score, 100);
    CHECK_EQ_INT(Th01Test_Pixel(0, 256, 16), 7);
    CHECK_EQ_INT(Th01Test_Pixel(1, 256, 16), 7);
    m_Hud.force_numbers = false;
    g_Stage.score = 101;
    g_Stage.combo = 257;
    Hud_ScoreAndCombo();
    CHECK_EQ_INT(g_Session.high_score, 101);
    CHECK_EQ_INT(m_Hud.maximum_combo, 1);
    CHECK_EQ_INT(m_Hud.max_previous, 257);
    Hud_SnapNumbers(false);
    CHECK_EQ_INT(m_Hud.score_previous, 101);
    CHECK_EQ_INT(m_Hud.combo_previous, 257);
    CHECK_EQ_INT(m_Hud.maximum_combo, 0);
    Arena_Free(&f.arena);
}

TEST(negative_unchanged_combo_keeps_glyphs_and_background)
{
    FIXTURE f;
    M_Init(&f);
    Hud_SnapNumbers(true);
    for (int32_t page = 0; page < 2; page++) {
        memset(g_Video.pages[page] + 17 * 640 + 400, 7, 32);
    }
    g_Stage.combo = -1;
    m_Hud.combo_previous = -1;
    Hud_ScoreAndCombo();
    CHECK_EQ_INT(Th01Test_Pixel(0, 400, 17), 7);
    CHECK_EQ_INT(Th01Test_Pixel(1, 400, 17), 7);
    Arena_Free(&f.arena);
}
