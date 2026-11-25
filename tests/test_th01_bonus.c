#include "../src/th01/bonus.c"
#include "../src/th01/hud.c"
#include "../src/th01/bonus_transition.c"
#include "../src/th01/vsync_irq.c"
#include "../src/th01/input.c"
#include "../src/th01/shots.c"
#include "../src/th01/player.c"
#include "../src/th01/pellets.c"
#include "th01/session.h"
#include "th01/shots.h"
#include "th01/rng.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "pc98/vsync.h"

#include "core/base/arena.h"

#include "th01_sprites.h"
#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

#include <string.h>

TEST(rowshift_preserves_pixel_order_and_source)
{
    ARENA arena;
    Arena_Init(&arena, "bonus", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    for (int32_t y = 0; y < 2; y++) {
        for (int32_t x = 0; x < VIDEO_WIDTH; x++) {
            g_Video.pages[0][y * VIDEO_WIDTH + x] = (uint8_t)(x % 15);
            g_Video.pages[1][y * VIDEO_WIDTH + x] = (uint8_t)(x / 32 % 16);
        }
    }
    for (uint16_t step = 0; step < 20; step++) {
        for (uint16_t y = 0; y < 2; y++) {
            Bonus_RowShift32(y, step * 4);
            const int32_t inserted = (step + 1) * 32;
            for (int32_t x = 0; x < VIDEO_WIDTH; x++) {
                const int32_t old_x = y == 0 ? x + inserted : x - inserted;
                const int32_t new_x = y == 0 ? x - VIDEO_WIDTH + inserted
                                             : x + VIDEO_WIDTH - inserted;
                const uint8_t expected = old_x >= 0 && old_x < VIDEO_WIDTH
                    ? (uint8_t)(old_x % 15)
                    : (uint8_t)(new_x / 32 % 16);
                CHECK_EQ_INT(Th01Test_Pixel(0, x, y), expected);
                CHECK_EQ_INT(Th01Test_Pixel(1, x, y), x / 32 % 16);
            }
        }
    }
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(row_transition_empty_scan_fades_and_final_delay)
{
    ARENA arena;
    Arena_Init(&arena, "bonus", 2 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&arena);
    Video_Init(a);
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    PALETTE_STATE palette = {};
    PALETTE target;
    memset(&target, 15, sizeof(target));
    Bonus_InitRowTransition();
    VSyncIrq_Reset();
    Bonus_BeginRowTransition();
    CHECK(!Bonus_ResumeRowTransition(&palette, &target));
    CHECK_EQ_INT(m_Transition.activated, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 639, 0), 3);
    for (int32_t step = 0; step < 70; step++) {
        const BONUS_ROW_TRANSITION frozen = m_Transition;
        CHECK(!Bonus_ResumeRowTransition(&palette, &target));
        CHECK(memcmp(&frozen, &m_Transition, sizeof(m_Transition)) == 0);
        m_VSync.count1 = 2;
        m_VSync.count2 = g_VSync->count2;
        const bool ready = Bonus_ResumeRowTransition(&palette, &target);
        CHECK(ready == (step == 69));
        CHECK_EQ_INT(g_VSync->count1, 0);
    }
    CHECK_EQ_INT(m_Transition.done, 400);
    CHECK(memcmp(g_Video.pages[0], g_Video.pages[1], VIDEO_PAGE_SIZE) == 0);
    CHECK_EQ_INT(palette.saved.colors[0][0], 15);
    CHECK_EQ_INT(g_Video.palette[0].red, 255);
    CHECK(Bonus_ResumeRowTransition(&palette, &target));
    Arena_Free(&arena);
}

typedef struct {
    BONUS_STAGE_ASSETS assets;
    PTN sprites;
    GAME_TEXT_FONT font;
} FIXTURE;

static void M_Init(FIXTURE *const f, const ALLOCATOR *const a)
{
    Th01Test_ResetWorld();
    GameInput_Init();
    StageTimer_Init();
    Video_Init(a);
    Hud_Init(a);
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
    Hud_Load(nullptr, &f->sprites, &f->font);
    m_Bonus.score_bonus = 0;
    memset(g_Session.bonus_per_stage, 0, sizeof(g_Session.bonus_per_stage));
    CHECK(Result_Ignore(Bonus_ValidateStageAssets(&f->assets, &f->font)));
    memset(f->assets.box, 0xff, sizeof(f->assets.box));
    Extend_Reset();
}

TEST(stage_box_exact_rows_score_hud_extend_and_release_wait)
{
    ARENA arena;
    Arena_Init(&arena, "bonus stage", 2 * 1024 * 1024);
    FIXTURE f = {};
    Th01Test_ResetWorld();
    M_Init(&f, Arena_Allocator(&arena));
    g_Stage.score = 393000;
    g_Stage.maximum_combo = 3;
    g_Stage.time = 100;
    g_Session.lives = 1;
    m_Player.remaining_bombs = 3;
    m_GameInput.bomb_frame = 97;
    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    memset(&g_StagePalette, 11, sizeof(g_StagePalette));
    Video_SelectDrawingPage(1);
    VSyncIrq_Reset();
    TH01_INPUT raw = {};
    CHECK(!Bonus_BeginStage(&f.assets, &f.font, 4, &raw));
    CHECK_EQ_INT(Th01Test_Pixel(1, 32, 180), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 32, 180), 5);
    CHECK_EQ_INT(Th01Test_Pixel(1, 32, 179), 6);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Palette.saved.colors[0][0], 11);
    for (int32_t delay = 0; delay < 24; delay++) {
        const BONUS frozen = m_Bonus;
        CHECK(!Bonus_Resume(&f.assets, &f.font, &raw));
        CHECK(memcmp(&frozen, &m_Bonus, sizeof(frozen)) == 0);
        m_VSync.count1 = 1;
        m_VSync.count2 = g_VSync->count2;
        CHECK(!Bonus_Resume(&f.assets, &f.font, &raw));
    }
    CHECK_EQ_INT(m_Bonus.run.phase, BONUS_CONFIRM);
    CHECK_EQ_INT(g_Stage.score, 410000);
    CHECK_EQ_INT(m_Bonus.score_bonus, 17000);
    CHECK_EQ_INT(g_Session.bonus_per_stage[3], 17000);
    CHECK_EQ_INT(g_Session.high_score, 410000);
    CHECK_EQ_INT(Extend_Next(), 2);
    CHECK_EQ_INT(m_Pellets.speed, 1);
    CHECK_EQ_INT(Th01Test_Pixel(1, 32, 41), 0);
    CHECK_EQ_INT(Th01Test_Pixel(1, 32, 319), 0);
    CHECK_EQ_INT(Th01Test_Pixel(1, 32, 40), 6);
    CHECK_EQ_INT(Th01Test_Pixel(1, 32, 320), 6);
    CHECK_EQ_INT(Th01Test_Pixel(1, 64, 80), 10);
    CHECK_EQ_INT(Th01Test_Pixel(1, 256, 128), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 64, 80), 5);
    CHECK(!Bonus_Resume(&f.assets, &f.font, &raw));
    raw.shot = true;
    CHECK(!Bonus_Resume(&f.assets, &f.font, &raw));
    CHECK_EQ_INT(g_GameInput->shot_presses, 1);
    raw.shot = false;
    CHECK(Bonus_Resume(&f.assets, &f.font, &raw));
    CHECK_EQ_INT(m_Bonus.score_bonus, 0);
    CHECK_EQ_INT(g_GameInput->bomb_frame, 0);
    CHECK(Bonus_Resume(&f.assets, &f.font, &raw));
    Arena_Free(&arena);
}

TEST(stage_wrap_clamp_and_pending_bonus_accumulator)
{
    ARENA arena;
    Arena_Init(&arena, "bonus wraps", 2 * 1024 * 1024);
    FIXTURE f = {};
    Th01Test_ResetWorld();
    M_Init(&f, Arena_Allocator(&arena));
    g_Session.lives = 6;
    m_Bonus.score_bonus = UINT32_MAX - 100;
    g_Stage.score = UINT32_MAX - 1000;
    g_Stage.time = 20000;
    g_Stage.maximum_combo = 100;
    VSyncIrq_Reset();
    m_VSync.count1 = 100;
    m_VSync.count2 = g_VSync->count2;
    const TH01_INPUT raw = { .ok = true };
    CHECK(!Bonus_BeginStage(&f.assets, &f.font, 19, &raw));
    for (int32_t i = 0; i < 24; i++) {
        m_VSync.count1 = 1;
        m_VSync.count2 = g_VSync->count2;
        CHECK(!Bonus_Resume(&f.assets, &f.font, &raw));
    }
    // Signed timer product wraps to -5536; combo clamps to 6553.
    const uint32_t expected =
        UINT32_MAX - 100 + (uint32_t)(-5536 * 10) + 65530 + 12000 + 36000;
    CHECK_EQ_INT(m_Bonus.score_bonus, expected);
    CHECK_EQ_INT((uint32_t)g_Session.bonus_per_stage[3], expected);
    CHECK_EQ_INT(g_Stage.score, UINT32_MAX - 1000 + expected);
    CHECK_EQ_INT(m_Bonus.run.phase, BONUS_CONFIRM);
    CHECK_EQ_INT(g_GameInput->bomb_frame, 0);
    TH01_INPUT released = {};
    CHECK(Bonus_Resume(&f.assets, &f.font, &released));
    Arena_Free(&arena);
}

typedef struct {
    FIXTURE common;
    BONUS_TOTAL_ASSETS assets;
} TOTAL_FIXTURE;

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
    memset(
        g_Session.continues_per_scene, 0,
        sizeof(g_Session.continues_per_scene));
    g_Session.continues_total = 0;
    CHECK(Result_Ignore(Bonus_ValidateTotalAssets(&f->assets)));
}

TEST(total_resets_transfers_animates_awards_and_records_continues)
{
    ARENA arena;
    Arena_Init(&arena, "bonus total", 2 * 1024 * 1024);
    TOTAL_FIXTURE f = {};
    M_InitTotal(&f, Arena_Allocator(&arena));
    g_Stage.time = 321;
    g_Stage.maximum_combo = 7;
    g_Stage.score = 390000;
    g_Session.lives = 2;
    m_Player.remaining_bombs = 3;
    m_GameInput.bomb_frame = 123;
    g_Session.continues_total = 65539;
    m_Shots.slots[0] = (SHOT) { .moving = true, .left = 100, .top = 100 };
    m_Shots.slots[1] = (SHOT) { .decay_frame = 2, .left = 200, .top = 100 };
    m_Pellets.slots[0] = (PELLET) { .moving = true, .not_rendered = true };
    m_Pellets.slots[1] = (PELLET) { .cloud_frame = 5 };
    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    VSyncIrq_Reset();
    TH01_INPUT raw = {};
    CHECK(!Bonus_BeginTotal(&f.assets, 5, &raw, 0));
    CHECK(!m_Shots.slots[0].moving);
    CHECK_EQ_INT(m_Shots.slots[1].decay_frame, 0);
    CHECK(!m_Pellets.slots[0].moving);
    CHECK_EQ_INT(m_Pellets.slots[1].cloud_frame, 5);
    CHECK_EQ_INT(m_Total.phase, BONUS_TOTAL_INITIAL_WAIT);
    CHECK_EQ_INT(Th01Test_Pixel(1, 0, 0), 6);
    m_VSync.count1 = 50;
    m_VSync.count2 = g_VSync->count2;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(m_Total.phase, BONUS_TOTAL_TRANSITION);
    CHECK_EQ_INT(Th01Test_Pixel(1, 0, 0), 14);
    CHECK_EQ_INT(g_Palette.resource.colors[0][0], 13);
    for (int32_t i = 0; i < 70; i++) {
        m_VSync.count1 = 2;
        m_VSync.count2 = g_VSync->count2;
        CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    }
    CHECK_EQ_INT(m_Total.phase, BONUS_TOTAL_STAGE_WAIT);
    CHECK_EQ_INT(Th01Test_Pixel(0, 224, 14), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 256, 14), 5);
    CHECK_EQ_INT(g_Palette.saved.colors[0][0], 13);
    m_VSync.count1 = 30;
    m_VSync.count2 = g_VSync->count2;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(m_Bonus.score_bonus, 16050);
    CHECK_EQ_INT(g_Stage.score, 390000);
    CHECK_EQ_INT(m_Total.phase, BONUS_TOTAL_DIGIT_WAIT);
    VSYNC_CLOCK clock = {};
    int32_t ticks = 0;
    while (m_Total.phase != BONUS_TOTAL_CONFIRM && ticks < 1000) {
        const BONUS_TOTAL frozen = m_Total;
        CHECK(!Bonus_ResumeTotal(&f.assets, &raw, (uint64_t)ticks));
        CHECK(memcmp(&frozen, &m_Total, sizeof(frozen)) == 0);
        ticks++;
        if (VSync_Advance(&clock)) {
            VSyncIrq_Edge();
        }
        CHECK(!Bonus_ResumeTotal(&f.assets, &raw, (uint64_t)ticks));
    }
    CHECK(ticks < 1000);
    CHECK_EQ_INT(m_Bonus.score_bonus, 96050);
    CHECK_EQ_INT(g_Stage.score, 486050);
    CHECK_EQ_INT(Extend_Next(), 1);
    CHECK_EQ_INT(g_Session.lives, 2);
    CHECK_EQ_INT(m_Hud.score_previous, 0);
    CHECK_EQ_INT(g_Session.bonus_per_stage[0], 0);
    CHECK_EQ_INT(g_Session.continues_per_scene[0], 3);
    CHECK_EQ_INT(g_GameInput->bomb_frame, 123);
    const int16_t sum_digits[] = { 0, 9, 6, 0, 5, 0 };
    for (int32_t i = 0; i < 6; i++) {
        CHECK_EQ_INT(Th01Test_Pixel(0, 208 + i * 16, 196), sum_digits[i]);
        CHECK_EQ_INT(Th01Test_Pixel(1, 208 + i * 16, 196), 14);
    }
    raw.ok = true;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, (uint64_t)ticks));
    raw.ok = false;
    CHECK(Bonus_ResumeTotal(&f.assets, &raw, (uint64_t)ticks));
    CHECK_EQ_INT(m_Bonus.score_bonus, 0);
    CHECK(Bonus_ResumeTotal(&f.assets, &raw, (uint64_t)ticks));
    Arena_Free(&arena);
}

TEST(total_second_loop_scrambles_then_restores_final_digit)
{
    ARENA arena;
    Arena_Init(&arena, "bonus scramble", 2 * 1024 * 1024);
    TOTAL_FIXTURE f = {};
    M_InitTotal(&f, Arena_Allocator(&arena));
    memset(g_Video.pages[1], 14, VIDEO_PAGE_SIZE);
    const TH01_INPUT raw = {};
    VSyncIrq_Reset();
    Bonus_InitTotal();
    m_Total.phase = BONUS_TOTAL_DIGIT;
    m_Total.top = 86;
    m_Total.digit_count = 1;
    m_Total.digits[0] = 9;
    m_Total.spin = 10;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(Th01Test_Pixel(0, 224, 86), 2);
    m_Total.credit = 12;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(Th01Test_Pixel(0, 224, 86), 3);
    m_Total.spin = 17;
    m_Total.credit = 12;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(Th01Test_Pixel(0, 224, 86), 14);
    m_Total.credit = 12;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(Th01Test_Pixel(0, 224, 86), 9);
    CHECK_EQ_INT(m_Total.phase, BONUS_TOTAL_METRIC_WAIT);
    Arena_Free(&arena);
}

TEST(total_millisecond_credit_crosses_digits_and_count1_can_satisfy_wait)
{
    ARENA arena;
    Arena_Init(&arena, "bonus credit", 2 * 1024 * 1024);
    TOTAL_FIXTURE f = {};
    M_InitTotal(&f, Arena_Allocator(&arena));
    const TH01_INPUT raw = {};
    VSyncIrq_Reset();
    m_VSync.count1 = 5;
    m_VSync.count2 = g_VSync->count2;
    Bonus_InitTotal();
    m_Total.phase = BONUS_TOTAL_DIGIT;
    m_Total.top = 196;
    m_Total.metric = 4;
    m_Total.digit_count = 2;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    for (uint64_t tick = 1; tick <= 4; tick++) {
        CHECK(!Bonus_ResumeTotal(&f.assets, &raw, tick));
    }
    CHECK_EQ_INT(m_Total.place, 1);
    CHECK_EQ_INT(m_Total.credit, 8);
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 5));
    CHECK_EQ_INT(m_Total.phase, BONUS_TOTAL_CONFIRM);
    CHECK_EQ_INT(g_VSync->count1, 0);
    CHECK_EQ_INT(m_Total.credit, 10);
    Arena_Free(&arena);
}

TEST(total_metric_widths_clamp_after_wrapping_and_sum_wraps)
{
    ARENA arena;
    Arena_Init(&arena, "bonus metric widths", 2 * 1024 * 1024);
    TOTAL_FIXTURE f = {};
    M_InitTotal(&f, Arena_Allocator(&arena));
    const TH01_INPUT raw = {};
    VSyncIrq_Reset();
    g_Stage.time = 13108;
    Bonus_InitTotal();
    m_Total.phase = BONUS_TOTAL_PREPARE_METRIC;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(m_Bonus.score_bonus, 40);
    CHECK_EQ_INT(m_Total.digits[3], 4);
    CHECK_EQ_INT(m_Total.credit, 1);
    g_Stage.time = UINT16_MAX;
    Bonus_InitTotal();
    m_Total.phase = BONUS_TOTAL_PREPARE_METRIC;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(m_Bonus.score_bonus, 65570);
    g_Stage.maximum_combo = 200;
    Bonus_InitTotal();
    m_Total.phase = BONUS_TOTAL_PREPARE_METRIC;
    m_Total.metric = 1;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(m_Bonus.score_bonus, (uint32_t)-189790);
    m_Bonus.score_bonus = 123;
    g_Stage.score = UINT32_MAX - 100;
    Bonus_InitTotal();
    m_Total.phase = BONUS_TOTAL_PREPARE_METRIC;
    m_Total.metric = 4;
    CHECK(!Bonus_ResumeTotal(&f.assets, &raw, 0));
    CHECK_EQ_INT(g_Stage.score, 22);
    CHECK_EQ_INT(m_Bonus.score_bonus, 123);
    CHECK_EQ_INT(m_Total.digits[3], 1);
    CHECK_EQ_INT(m_Total.digits[4], 2);
    CHECK_EQ_INT(m_Total.digits[5], 3);
    Arena_Free(&arena);
}

TEST(resource_validation_rejects_missing_inputs)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    BONUS_STAGE_ASSETS *stage = nullptr;
    CHECK(!Result_Ignore(Bonus_LoadStageAssets(vfs, nullptr, &stage)));
    CHECK(stage == nullptr);
    Vfs_Destroy(vfs);
    CHECK(!Result_Ignore(Bonus_ValidateStageAssets(nullptr, nullptr)));
    CHECK(!Result_Ignore(Bonus_ValidateTotalAssets(nullptr)));
    BONUS_TOTAL_ASSETS total = {};
    CHECK(!Result_Ignore(Bonus_ValidateTotalAssets(&total)));
}

TEST(loads_the_stage_box_from_the_built_in_sheet)
{
    VFS *const vfs = Th01Test_Sprites();
    BONUS_STAGE_ASSETS *stage = nullptr;
    CHECK(IS_OK(Bonus_LoadStageAssets(vfs, &g_HeapAllocator, &stage)));
    const uint8_t box[4] = { 0xee, 0xff, 0xbb, 0xff };
    CHECK(memcmp(stage->box, box, sizeof(box)) == 0);
    Mem_Free(stage);
    Vfs_Destroy(vfs);
}
