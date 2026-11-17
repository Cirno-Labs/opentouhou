#include "th01/bonus.h"

#include "core/base/arena.h"

#include "test.h"

TEST(rowshift_preserves_pixel_order_and_source)
{
    ARENA arena;
    Arena_Init(&arena, "bonus", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    for (int32_t y = 0; y < 2; y++) {
        for (int32_t x = 0; x < VIDEO_WIDTH; x++) {
            video.pages[0][y * VIDEO_WIDTH + x] = (uint8_t)(x % 15);
            video.pages[1][y * VIDEO_WIDTH + x] = (uint8_t)(x / 32 % 16);
        }
    }
    for (uint16_t step = 0; step < 20; step++) {
        for (uint16_t y = 0; y < 2; y++) {
            Bonus_RowShift32(&video, y, step * 4);
            const int32_t inserted = (step + 1) * 32;
            for (int32_t x = 0; x < VIDEO_WIDTH; x++) {
                const int32_t old_x = y == 0 ? x + inserted : x - inserted;
                const int32_t new_x = y == 0 ? x - VIDEO_WIDTH + inserted
                                             : x + VIDEO_WIDTH - inserted;
                const uint8_t expected = old_x >= 0 && old_x < VIDEO_WIDTH
                    ? (uint8_t)(old_x % 15)
                    : (uint8_t)(new_x / 32 % 16);
                CHECK_EQ_INT(Video_ReadPixel(&video, 0, x, y), expected);
                CHECK_EQ_INT(Video_ReadPixel(&video, 1, x, y), x / 32 % 16);
            }
        }
    }
    CHECK_EQ_INT(video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(row_transition_empty_scan_fades_and_final_delay)
{
    ARENA arena;
    Arena_Init(&arena, "bonus", 2 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&arena);
    VIDEO video;
    Video_Init(&video, a);
    memset(video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(video.pages[1], 7, VIDEO_PAGE_SIZE);
    PALETTE_STATE palette = {};
    PALETTE target;
    memset(&target, 15, sizeof(target));
    BONUS_ROW_TRANSITION t = {};
    FRAME_VSYNC vsync = {};
    Bonus_BeginRowTransition(&t);
    CHECK(!Bonus_ResumeRowTransition(&t, &video, &palette, &target, &vsync));
    CHECK_EQ_INT(t.activated, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 639, 0), 3);
    for (int32_t step = 0; step < 70; step++) {
        const BONUS_ROW_TRANSITION frozen = t;
        CHECK(
            !Bonus_ResumeRowTransition(&t, &video, &palette, &target, &vsync));
        CHECK(memcmp(&frozen, &t, sizeof(t)) == 0);
        vsync.count1 = 2;
        const bool ready =
            Bonus_ResumeRowTransition(&t, &video, &palette, &target, &vsync);
        CHECK(ready == (step == 69));
        CHECK_EQ_INT(vsync.count1, 0);
    }
    CHECK_EQ_INT(t.done, 400);
    CHECK(memcmp(video.pages[0], video.pages[1], VIDEO_PAGE_SIZE) == 0);
    CHECK_EQ_INT(palette.saved.colors[0][0], 15);
    CHECK_EQ_INT(video.palette[0].red, 255);
    CHECK(Bonus_ResumeRowTransition(&t, &video, &palette, &target, &vsync));
    Arena_Free(&arena);
}

typedef struct {
    VIDEO video;
    HUD hud;
    HUD_CONTEXT display;
    EXTEND_CONTEXT extend;
    BONUS_CONTEXT context;
    BONUS_STAGE_ASSETS assets;
    BONUS bonus;
    STAGE_GAMEPLAY gameplay;
    STAGE_TIMER timer;
    PLAYER player;
    PELLET_SHARED shared;
    GAME_INPUT input;
    PALETTE_STATE palette;
    PALETTE stage_palette;
    PTN sprites;
    GAME_TEXT_FONT font;
    uint32_t high, score_bonus;
    int32_t history[4], effects;
    int16_t lives, next;
    int8_t resident_lives;
} FIXTURE;

static void M_Effect(void *const user, const int16_t number)
{
    FIXTURE *const f = user;
    CHECK_EQ_INT(number, 15);
    CHECK_EQ_INT(f->gameplay.score, 410000);
    CHECK_EQ_INT(f->history[3], 17000);
    CHECK_EQ_INT(f->hud.score_previous, 410000);
    CHECK_EQ_INT(f->lives, 2);
    f->effects++;
}

static void M_Init(FIXTURE *const f, const ALLOCATOR *const a)
{
    Video_Init(&f->video, a);
    Hud_Init(&f->hud, a);
    GAME_TEXT_GLYPH glyphs[96] = { { .codepoint = ' ', .width = 8 },
                                   { .codepoint = 0x3000, .width = 16 } };
    for (uint32_t i = 0; i < 94; i++) {
        glyphs[i + 2].codepoint = 0xff01 + i;
        glyphs[i + 2].width = 16;
        for (int32_t row = 0; row < 16; row++) {
            glyphs[i + 2].rows[row] = 0x8000;
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 96, a, &f->font)));
    f->sprites = (PTN) { .count = 1, .pixels = Allocator_Alloc(a, 1024) };
    memset(f->sprites.pixels, 3, 1024);
    f->display = (HUD_CONTEXT) { .video = &f->video,
                                 .sprites = &f->sprites,
                                 .font = &f->font,
                                 .gameplay = &f->gameplay,
                                 .player = &f->player,
                                 .remaining_lives = &f->lives,
                                 .high_score = &f->high };
    f->extend = (EXTEND_CONTEXT) { .hud = &f->hud,
                                   .display = &f->display,
                                   .resident_lives = &f->resident_lives,
                                   .next = &f->next,
                                   .shared = &f->shared,
                                   .effect = M_Effect,
                                   .user = f };
    f->context = (BONUS_CONTEXT) { .stage = &f->assets,
                                   .font = &f->font,
                                   .stage_palette = &f->stage_palette,
                                   .palette = &f->palette,
                                   .hud = &f->hud,
                                   .display = &f->display,
                                   .extend = &f->extend,
                                   .timer = &f->timer,
                                   .input = &f->input,
                                   .score_bonus = &f->score_bonus,
                                   .bonus_per_stage = f->history };
    CHECK(Result_Ignore(Bonus_ValidateStageAssets(&f->assets, &f->font)));
    memset(f->assets.box, 0xff, sizeof(f->assets.box));
    f->next = 1;
}

TEST(stage_box_exact_rows_score_hud_extend_and_release_wait)
{
    ARENA arena;
    Arena_Init(&arena, "bonus stage", 2 * 1024 * 1024);
    FIXTURE f = {};
    M_Init(&f, Arena_Allocator(&arena));
    f.gameplay.score = 393000;
    f.gameplay.maximum_combo = 3;
    f.timer.time = 100;
    f.lives = 1;
    f.player.remaining_bombs = 3;
    f.input.bomb_frame = 97;
    memset(f.video.pages[0], 5, VIDEO_PAGE_SIZE);
    memset(f.video.pages[1], 6, VIDEO_PAGE_SIZE);
    memset(&f.stage_palette, 11, sizeof(f.stage_palette));
    Video_SelectDrawingPage(&f.video, 1);
    FRAME_VSYNC vsync = {};
    TH01_INPUT raw = {};
    CHECK(!Bonus_BeginStage(&f.bonus, &f.context, 4, &raw, &vsync));
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 32, 180), 0);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 32, 180), 5);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 32, 179), 6);
    CHECK_EQ_INT(f.video.drawing_page, 1);
    CHECK_EQ_INT(f.palette.saved.colors[0][0], 11);
    for (int32_t delay = 0; delay < 24; delay++) {
        const BONUS frozen = f.bonus;
        CHECK(!Bonus_Resume(&f.bonus, &f.context, &raw, &vsync));
        CHECK(memcmp(&frozen, &f.bonus, sizeof(frozen)) == 0);
        vsync.count1 = 1;
        CHECK(!Bonus_Resume(&f.bonus, &f.context, &raw, &vsync));
    }
    CHECK_EQ_INT(f.bonus.phase, BONUS_CONFIRM);
    CHECK_EQ_INT(f.gameplay.score, 410000);
    CHECK_EQ_INT(f.score_bonus, 17000);
    CHECK_EQ_INT(f.history[3], 17000);
    CHECK_EQ_INT(f.high, 410000);
    CHECK_EQ_INT(f.effects, 1);
    CHECK_EQ_INT(f.next, 2);
    CHECK_EQ_INT(f.shared.speed, 1);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 32, 41), 0);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 32, 319), 0);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 32, 40), 6);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 32, 320), 6);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 64, 80), 10);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 1, 256, 128), 7);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 64, 80), 5);
    CHECK(!Bonus_Resume(&f.bonus, &f.context, &raw, &vsync));
    raw.shot = true;
    CHECK(!Bonus_Resume(&f.bonus, &f.context, &raw, &vsync));
    CHECK_EQ_INT(f.input.shot_presses, 1);
    raw.shot = false;
    CHECK(Bonus_Resume(&f.bonus, &f.context, &raw, &vsync));
    CHECK_EQ_INT(f.score_bonus, 0);
    CHECK_EQ_INT(f.input.bomb_frame, 0);
    CHECK(Bonus_Resume(&f.bonus, &f.context, &raw, &vsync));
    Arena_Free(&arena);
}

TEST(stage_wrap_clamp_and_pending_bonus_accumulator)
{
    ARENA arena;
    Arena_Init(&arena, "bonus wraps", 2 * 1024 * 1024);
    FIXTURE f = {};
    M_Init(&f, Arena_Allocator(&arena));
    f.lives = 6;
    f.score_bonus = UINT32_MAX - 100;
    f.gameplay.score = UINT32_MAX - 1000;
    f.timer.time = 20000;
    f.gameplay.maximum_combo = 100;
    FRAME_VSYNC vsync = { .count1 = 100 };
    const TH01_INPUT raw = { .ok = true };
    CHECK(!Bonus_BeginStage(&f.bonus, &f.context, 19, &raw, &vsync));
    for (int32_t i = 0; i < 24; i++) {
        vsync.count1 = 1;
        CHECK(!Bonus_Resume(&f.bonus, &f.context, &raw, &vsync));
    }
    // Signed timer product wraps to -5536; combo clamps to 6553.
    const uint32_t expected =
        UINT32_MAX - 100 + (uint32_t)(-5536 * 10) + 65530 + 12000 + 36000;
    CHECK_EQ_INT(f.score_bonus, expected);
    CHECK_EQ_INT((uint32_t)f.history[3], expected);
    CHECK_EQ_INT(f.gameplay.score, UINT32_MAX - 1000 + expected);
    CHECK_EQ_INT(f.bonus.phase, BONUS_CONFIRM);
    CHECK_EQ_INT(f.input.bomb_frame, 0);
    TH01_INPUT released = {};
    CHECK(Bonus_Resume(&f.bonus, &f.context, &released, &vsync));
    Arena_Free(&arena);
}
