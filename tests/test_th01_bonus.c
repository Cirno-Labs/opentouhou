#include "th01/bonus.h"
#include "pc98/vsync.h"

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

typedef struct {
    FIXTURE common;
    BONUS_TOTAL bonus;
    BONUS_TOTAL_ASSETS assets;
    BONUS_TOTAL_CONTEXT context;
    SHOTS shots;
    PELLETS pellets;
    int32_t effects, restores, continues;
    uint16_t history[4];
} TOTAL_FIXTURE;

static void M_TotalEffect(void *const user, const int16_t number)
{
    TOTAL_FIXTURE *const f = user;
    CHECK_EQ_INT(number, 2);
    CHECK(f->bonus.phase == BONUS_TOTAL_DIGIT_WAIT);
    f->effects++;
}

static void M_Shots(void *const user, const SHOTS_ACTION action)
{
    TOTAL_FIXTURE *const f = user;
    CHECK_EQ_INT(action.kind, SHOTS_ACTION_RESTORE);
    f->restores++;
    Shots_RenderAction(&f->common.video, &f->assets.numerals, action);
}

static void M_InitTotal(TOTAL_FIXTURE *const f, const ALLOCATOR *const a)
{
    M_Init(&f->common, a);
    f->assets.background =
        (PI_IMAGE) { .pixels = Allocator_Alloc(a, VIDEO_PAGE_SIZE),
                     .width = VIDEO_WIDTH,
                     .height = VIDEO_HEIGHT,
                     .has_palette = true };
    memset(f->assets.background.pixels, 14, VIDEO_PAGE_SIZE);
    memset(&f->assets.palette, 13, sizeof(PALETTE));
    f->assets.numerals =
        (PTN) { .count = 13, .pixels = Allocator_Alloc(a, 13 * 1024) };
    for (int32_t i = 0; i < 13; i++) {
        for (int32_t y = 0; y < 32; y++) {
            for (int32_t x = 0; x < 32; x++) {
                const int32_t quarter = y / 16 * 2 + x / 16;
                const int32_t digit = i < 10 ? i : (i - 10) * 4 + quarter;
                f->assets.numerals.pixels[i * 1024 + y * 32 + x] =
                    digit < 10 ? (uint8_t)digit : 15;
            }
        }
    }
    f->context = (BONUS_TOTAL_CONTEXT) { .common = &f->common.context,
                                         .assets = &f->assets,
                                         .shots = &f->shots,
                                         .shots_output = { M_Shots, f },
                                         .pellets = &f->pellets,
                                         .continues_per_scene = f->history,
                                         .continues_total = &f->continues,
                                         .effect = M_TotalEffect,
                                         .user = f };
    CHECK(Result_Ignore(Bonus_ValidateTotalAssets(&f->assets)));
}

TEST(total_resets_transfers_animates_awards_and_records_continues)
{
    ARENA arena;
    Arena_Init(&arena, "bonus total", 2 * 1024 * 1024);
    TOTAL_FIXTURE f = {};
    M_InitTotal(&f, Arena_Allocator(&arena));
    f.common.timer.time = 321;
    f.common.gameplay.maximum_combo = 7;
    f.common.gameplay.score = 390000;
    f.common.lives = 2;
    f.common.player.remaining_bombs = 3;
    f.common.input.bomb_frame = 123;
    f.continues = 65539;
    f.shots.slots[0] = (SHOT) { .moving = true, .left = 100, .top = 100 };
    f.shots.slots[1] = (SHOT) { .decay_frame = 2, .left = 200, .top = 100 };
    f.pellets.slots[0] = (PELLET) { .moving = true, .not_rendered = true };
    f.pellets.slots[1] = (PELLET) { .cloud_frame = 5 };
    memset(f.common.video.pages[0], 5, VIDEO_PAGE_SIZE);
    memset(f.common.video.pages[1], 6, VIDEO_PAGE_SIZE);
    FRAME_VSYNC vsync = {};
    TH01_INPUT raw = {};
    CHECK(!Bonus_BeginTotal(&f.bonus, &f.context, 5, &raw, &vsync, 0));
    CHECK_EQ_INT(f.restores, 2);
    CHECK(!f.shots.slots[0].moving);
    CHECK_EQ_INT(f.shots.slots[1].decay_frame, 0);
    CHECK(!f.pellets.slots[0].moving);
    CHECK_EQ_INT(f.pellets.slots[1].cloud_frame, 5);
    CHECK_EQ_INT(f.bonus.phase, BONUS_TOTAL_INITIAL_WAIT);
    CHECK_EQ_INT(Video_ReadPixel(&f.common.video, 1, 0, 0), 6);
    vsync.count1 = 50;
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(f.bonus.phase, BONUS_TOTAL_TRANSITION);
    CHECK_EQ_INT(Video_ReadPixel(&f.common.video, 1, 0, 0), 14);
    CHECK_EQ_INT(f.common.palette.resource.colors[0][0], 13);
    for (int32_t i = 0; i < 70; i++) {
        vsync.count1 = 2;
        CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    }
    CHECK_EQ_INT(f.bonus.phase, BONUS_TOTAL_STAGE_WAIT);
    CHECK_EQ_INT(Video_ReadPixel(&f.common.video, 0, 224, 14), 0);
    CHECK_EQ_INT(Video_ReadPixel(&f.common.video, 0, 256, 14), 5);
    CHECK_EQ_INT(f.common.palette.saved.colors[0][0], 13);
    vsync.count1 = 30;
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(f.common.score_bonus, 16050);
    CHECK_EQ_INT(f.common.gameplay.score, 390000);
    CHECK_EQ_INT(f.bonus.phase, BONUS_TOTAL_DIGIT_WAIT);
    VSYNC_CLOCK clock = {};
    int32_t ticks = 0;
    while (f.bonus.phase != BONUS_TOTAL_CONFIRM && ticks < 1000) {
        const BONUS_TOTAL frozen = f.bonus;
        const int32_t effects = f.effects;
        CHECK(!Bonus_ResumeTotal(
            &f.bonus, &f.context, &raw, &vsync, (uint64_t)ticks));
        CHECK(memcmp(&frozen, &f.bonus, sizeof(frozen)) == 0);
        CHECK_EQ_INT(f.effects, effects);
        ticks++;
        if (VSync_Advance(&clock)) {
            Frame_VSync(&vsync);
        }
        CHECK(!Bonus_ResumeTotal(
            &f.bonus, &f.context, &raw, &vsync, (uint64_t)ticks));
    }
    CHECK(ticks < 1000);
    CHECK_EQ_INT(f.effects, 319);
    CHECK_EQ_INT(f.common.score_bonus, 96050);
    CHECK_EQ_INT(f.common.gameplay.score, 486050);
    CHECK_EQ_INT(f.common.next, 1);
    CHECK_EQ_INT(f.common.lives, 2);
    CHECK_EQ_INT(f.common.hud.score_previous, 0);
    CHECK_EQ_INT(f.common.history[0], 0);
    CHECK_EQ_INT(f.history[0], 3);
    CHECK_EQ_INT(f.common.input.bomb_frame, 123);
    const int16_t sum_digits[] = { 0, 9, 6, 0, 5, 0 };
    for (int32_t i = 0; i < 6; i++) {
        CHECK_EQ_INT(
            Video_ReadPixel(&f.common.video, 0, 208 + i * 16, 196),
            sum_digits[i]);
        CHECK_EQ_INT(
            Video_ReadPixel(&f.common.video, 1, 208 + i * 16, 196), 14);
    }
    raw.ok = true;
    CHECK(!Bonus_ResumeTotal(
        &f.bonus, &f.context, &raw, &vsync, (uint64_t)ticks));
    raw.ok = false;
    CHECK(
        Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, (uint64_t)ticks));
    CHECK_EQ_INT(f.common.score_bonus, 0);
    CHECK(
        Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, (uint64_t)ticks));
    Arena_Free(&arena);
}

TEST(total_second_loop_scrambles_then_restores_final_digit)
{
    ARENA arena;
    Arena_Init(&arena, "bonus scramble", 2 * 1024 * 1024);
    TOTAL_FIXTURE f = {};
    M_InitTotal(&f, Arena_Allocator(&arena));
    memset(f.common.video.pages[1], 14, VIDEO_PAGE_SIZE);
    const TH01_INPUT raw = {};
    FRAME_VSYNC vsync = {};
    f.bonus = (BONUS_TOTAL) { .phase = BONUS_TOTAL_DIGIT,
                              .top = 86,
                              .digit_count = 1,
                              .digits = { 9 },
                              .spin = 10 };
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(Video_ReadPixel(&f.common.video, 0, 224, 86), 2);
    f.bonus.credit = 12;
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(Video_ReadPixel(&f.common.video, 0, 224, 86), 3);
    f.bonus.spin = 17;
    f.bonus.credit = 12;
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(Video_ReadPixel(&f.common.video, 0, 224, 86), 14);
    f.bonus.credit = 12;
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(Video_ReadPixel(&f.common.video, 0, 224, 86), 9);
    CHECK_EQ_INT(f.bonus.phase, BONUS_TOTAL_METRIC_WAIT);
    CHECK_EQ_INT(f.effects, 3);
    Arena_Free(&arena);
}

TEST(total_millisecond_credit_crosses_digits_and_count1_can_satisfy_wait)
{
    ARENA arena;
    Arena_Init(&arena, "bonus credit", 2 * 1024 * 1024);
    TOTAL_FIXTURE f = {};
    M_InitTotal(&f, Arena_Allocator(&arena));
    const TH01_INPUT raw = {};
    FRAME_VSYNC vsync = { .count1 = 5 };
    f.bonus = (BONUS_TOTAL) { .phase = BONUS_TOTAL_DIGIT,
                              .top = 196,
                              .metric = 4,
                              .digit_count = 2,
                              .digits = { 0, 0 } };
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    for (uint64_t tick = 1; tick <= 4; tick++) {
        CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, tick));
    }
    CHECK_EQ_INT(f.effects, 16);
    CHECK_EQ_INT(f.bonus.place, 1);
    CHECK_EQ_INT(f.bonus.credit, 8);
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 5));
    CHECK_EQ_INT(f.effects, 20);
    CHECK_EQ_INT(f.bonus.phase, BONUS_TOTAL_CONFIRM);
    CHECK_EQ_INT(vsync.count1, 0);
    CHECK_EQ_INT(f.bonus.credit, 10);
    Arena_Free(&arena);
}

TEST(total_metric_widths_clamp_after_wrapping_and_sum_wraps)
{
    ARENA arena;
    Arena_Init(&arena, "bonus metric widths", 2 * 1024 * 1024);
    TOTAL_FIXTURE f = {};
    M_InitTotal(&f, Arena_Allocator(&arena));
    const TH01_INPUT raw = {};
    FRAME_VSYNC vsync = {};
    f.common.timer.time = 13108;
    f.bonus = (BONUS_TOTAL) { .phase = BONUS_TOTAL_PREPARE_METRIC };
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(f.common.score_bonus, 40);
    CHECK_EQ_INT(f.bonus.digits[3], 4);
    CHECK_EQ_INT(f.effects, 4);
    CHECK_EQ_INT(f.bonus.credit, 1);
    f.common.timer.time = UINT16_MAX;
    f.bonus = (BONUS_TOTAL) { .phase = BONUS_TOTAL_PREPARE_METRIC };
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(f.common.score_bonus, 65570);
    f.common.gameplay.maximum_combo = 200;
    f.bonus =
        (BONUS_TOTAL) { .phase = BONUS_TOTAL_PREPARE_METRIC, .metric = 1 };
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(f.common.score_bonus, (uint32_t)-189790);
    f.common.score_bonus = 123;
    f.common.gameplay.score = UINT32_MAX - 100;
    f.bonus =
        (BONUS_TOTAL) { .phase = BONUS_TOTAL_PREPARE_METRIC, .metric = 4 };
    CHECK(!Bonus_ResumeTotal(&f.bonus, &f.context, &raw, &vsync, 0));
    CHECK_EQ_INT(f.common.gameplay.score, 22);
    CHECK_EQ_INT(f.common.score_bonus, 123);
    CHECK_EQ_INT(f.bonus.digits[3], 1);
    CHECK_EQ_INT(f.bonus.digits[4], 2);
    CHECK_EQ_INT(f.bonus.digits[5], 3);
    Arena_Free(&arena);
}

TEST(resource_validation_rejects_missing_inputs)
{
    READER reader;
    Reader_OpenBuffer(&reader, nullptr, 0, nullptr);
    BONUS_STAGE_ASSETS *stage = nullptr;
    CHECK(!Result_Ignore(
        Bonus_LoadStageAssets(&reader, nullptr, 0, nullptr, &stage)));
    CHECK(stage == nullptr);
    CHECK(!Result_Ignore(Bonus_ValidateStageAssets(nullptr, nullptr)));
    CHECK(!Result_Ignore(Bonus_ValidateTotalAssets(nullptr)));
    BONUS_TOTAL_ASSETS total = {};
    CHECK(!Result_Ignore(Bonus_ValidateTotalAssets(&total)));
}
