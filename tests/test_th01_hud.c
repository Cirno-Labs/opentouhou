#include "th01/hud.h"
#include "core/base/arena.h"
#include "test.h"

typedef struct {
    ARENA arena;
    VIDEO video;
    HUD hud;
    HUD_CONTEXT context;
    PTN sprites;
    GRF mask;
    GAME_TEXT_FONT font;
    PLAYER player;
    STAGE_GAMEPLAY gameplay;
    uint32_t high_score;
    int16_t lives;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Arena_Init(&f->arena, "hud", 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(&f->video, a);
    Hud_Init(&f->hud, a);
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
    f->context = (HUD_CONTEXT) { .video = &f->video,
                                 .mask = &f->mask,
                                 .sprites = &f->sprites,
                                 .font = &f->font,
                                 .gameplay = &f->gameplay,
                                 .player = &f->player,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives,
                                 .stage_number = 1,
                                 .rank = SETTINGS_DIFFICULTY_NORMAL };
    CHECK(Result_Ignore(Hud_ValidateAssets(&f->mask, &f->sprites, &f->font)));
}

TEST(mask_covers_playfield_overlap_and_preserves_clear_dots)
{
    FIXTURE f;
    M_Init(&f);
    memset(f.video.pages[0], 2, VIDEO_PAGE_SIZE);
    memset(f.video.pages[1], 5, VIDEO_PAGE_SIZE);
    f.mask.coverage[65 * 640 + 639] = 1;
    Hud_PutMask(&f.context);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 639, 65), 10);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 639, 65), 10);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 638, 65), 2);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 639, 66), 5);
    CHECK_EQ_INT(f.video.drawing_page, 0);
    Arena_Free(&f.arena);
}

TEST(resources_snap_before_selection_and_restore_opaque_color15)
{
    FIXTURE f;
    M_Init(&f);
    memset(f.video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(f.video.pages[1], 6, VIDEO_PAGE_SIZE);
    f.lives = 2;
    f.player.remaining_bombs = 1;
    Hud_SnapResources(&f.hud, &f.context);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 128, 0), 3);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 129, 0), 15);
    f.lives = 1;
    Hud_LivesPut(&f.hud, &f.context, 2);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 144, 0), 15);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 144, 0), 15);
    Video_SelectDrawingPage(&f.video, 1);
    f.lives = 2;
    Hud_LivesPut(&f.hud, &f.context, 1);
    CHECK_EQ_INT(f.hud.lives[256], 15);
    CHECK_EQ_INT(f.video.drawing_page, 0);
    f.player.remaining_bombs = 0;
    Hud_BombsPut(&f.hud, &f.context, 1);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 128, 16), 15);
    Arena_Free(&f.arena);
}

TEST(timer_preserves_duplicate_snapshot_zero_spaces_and_red_threshold)
{
    FIXTURE f;
    M_Init(&f);
    for (int32_t y = 0; y < 16; y++) {
        for (int32_t x = 512; x < 544; x++) {
            f.video.pages[0][y * 640 + x] = x < 528 ? 2 : 3;
        }
    }
    Hud_SnapTimer(&f.hud, &f.context, 1000);
    Hud_PutTimer(&f.hud, &f.context, 0);
    for (int32_t page = 0; page < 2; page++) {
        CHECK_EQ_INT(Video_ReadPixel(&f.video, page, 528, 1), 2);
        CHECK_EQ_INT(Video_ReadPixel(&f.video, page, 512, 0), 2);
    }
    Hud_PutTimer(&f.hud, &f.context, 198);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 520, 0), 10);
    Hud_PutTimer(&f.hud, &f.context, 200);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 520, 0), 7);
    Arena_Free(&f.arena);
}

TEST(force_draws_leading_zeroes_and_preserves_resident_high_score)
{
    FIXTURE f;
    M_Init(&f);
    f.high_score = 100;
    Hud_SnapNumbers(&f.hud, &f.context, true);
    f.gameplay.score = 1;
    f.hud.force_numbers = true;
    Hud_ScoreAndCombo(&f.hud, &f.context);
    CHECK_EQ_INT(f.high_score, 100);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 256, 16), 7);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 256, 16), 7);
    f.hud.force_numbers = false;
    f.gameplay.score = 101;
    f.gameplay.combo = 257;
    Hud_ScoreAndCombo(&f.hud, &f.context);
    CHECK_EQ_INT(f.high_score, 101);
    CHECK_EQ_INT(f.hud.maximum_combo, 1);
    CHECK_EQ_INT(f.hud.max_previous, 257);
    Hud_SnapNumbers(&f.hud, &f.context, false);
    CHECK_EQ_INT(f.hud.score_previous, 101);
    CHECK_EQ_INT(f.hud.combo_previous, 257);
    CHECK_EQ_INT(f.hud.maximum_combo, 0);
    Arena_Free(&f.arena);
}
