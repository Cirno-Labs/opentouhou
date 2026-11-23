#include "th01/timer.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"
#include "pc98/zun/master_rng.h"
#include "core/base/arena.h"
#include "th01_world.h"
#include "test.h"

typedef struct {
    ARENA arena;
    HUD_CONTEXT display;
    TIMER_CONTEXT context;
    GRF mask;
    PTN sprites;
    GAME_TEXT_FONT font;
    uint32_t high_score;
    int16_t lives;
    int32_t effects;
} FIXTURE;

static void M_Effect(void *const user, const int16_t number)
{
    FIXTURE *const f = user;
    CHECK_EQ_INT(number, 17);
    CHECK_EQ_INT(g_PelletShared.speed, 2);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_StageTimer.time, 0);
    CHECK_EQ_INT(g_StageTimer.phase, TIMER_IDLE);
    f->effects++;
}

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    g_Rng = 1;
    Frame_Reset();
    StageTimer_Init();
    Arena_Init(&f->arena, "timer", 2 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(a);
    Hud_Init(a);
    Tram_Init(a);
    GAME_TEXT_GLYPH glyphs[105] = {};
    for (int32_t i = 0; i < 105; i++) {
        glyphs[i].codepoint = (uint32_t)(i < 95 ? i + 32 : i - 95 + 0xff10);
        glyphs[i].width = i < 95 ? 8 : 16;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i].rows[y] = i == 0 ? 0 : 0x8000;
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 105, a, &f->font)));
    f->mask.coverage = Allocator_AllocZ(a, 640 * 66);
    f->sprites = (PTN) { .count = 1, .pixels = Allocator_AllocZ(a, 1024) };
    f->display = (HUD_CONTEXT) { .mask = &f->mask,
                                 .sprites = &f->sprites,
                                 .font = &f->font,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives,
                                 .stage_number = 1,
                                 .rank = SETTINGS_DIFFICULTY_NORMAL };
    f->context = (TIMER_CONTEXT) { .display = &f->display,
                                   .effect = M_Effect,
                                   .user = f };
    Hud_SnapTimer(&f->display, 1000);
}

TEST(route_table_and_reinitialization_keep_cycle)
{
    FIXTURE f;
    M_Init(&f);
    g_StageTimer.hurry_cycle = 255;
    const uint16_t expected[2][20] = {
        { 1000, 1000, 1000, 1000, 2000, 1200, 1200, 1200, 1200, 3000,
          1400, 1400, 1400, 1400, 4000, 1600, 1600, 1600, 600,  6000 },
        { 1000, 1000, 1000, 1000, 2000, 1200, 1200, 1200, 1200, 2000,
          1400, 1400, 1400, 1400, 4000, 1600, 1800, 1800, 1600, 8000 }
    };
    for (int16_t route = 0; route < 2; route++) {
        for (int16_t stage = 0; stage < 20; stage++) {
            g_StageTimer.frames_since_hurry = 555;
            f.display.stage_number = (uint16_t)(stage + 1);
            StageTimer_Redraw(&f.context, false, false, route);
            CHECK_EQ_INT(g_StageTimer.time, expected[route][stage]);
            CHECK_EQ_INT(g_StageTimer.hurry_cycle, 255);
            CHECK_EQ_INT(g_StageTimer.frames_since_hurry, 0);
        }
    }
    Arena_Free(&f.arena);
}

TEST(transition_waits_suspend_without_replaying_side_effects)
{
    FIXTURE f;
    M_Init(&f);
    g_StageTimer.time = 2;
    CHECK(!StageTimer_TickAndPut(&f.context));
    CHECK_EQ_INT(f.effects, 1);
    CHECK_EQ_INT(g_StageTimer.phase, TIMER_WAIT_HURRY);
    CHECK_EQ_INT(g_Tram.pixels[80 * 640], TRAM_RED + 1);
    for (int32_t i = 0; i < 26; i++) {
        Frame_VSync();
        CHECK(!StageTimer_TickAndPut(&f.context));
    }
    Frame_VSync();
    CHECK(!StageTimer_TickAndPut(&f.context));
    CHECK_EQ_INT(g_StageTimer.phase, TIMER_WAIT_UP);
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_Tram.pixels[80 * 640], TRAM_BLACK + 1);
    CHECK_EQ_INT(g_Tram.pixels[80 * 640 + 192], TRAM_RED + 1);
    for (int32_t i = 0; i < 27; i++) {
        Frame_VSync();
    }
    CHECK(StageTimer_TickAndPut(&f.context));
    CHECK_EQ_INT(f.effects, 1);
    CHECK_EQ_INT(g_PelletShared.speed, 2);
    CHECK_EQ_INT(g_StageTimer.frames_since_hurry, 0);
    CHECK_EQ_INT(g_StageTimer.hurry_cycle, 0);
    CHECK_EQ_INT(g_Tram.pixels[80 * 640 + 192], 0);
    Arena_Free(&f.arena);
}

TEST(ready_count_is_consumed_before_first_suspension_and_extension_wraps)
{
    FIXTURE f;
    M_Init(&f);
    g_StageTimer.time = 2;
    g_VSync.count1 = 31;
    CHECK(!StageTimer_TickAndPut(&f.context));
    CHECK_EQ_INT(g_StageTimer.phase, TIMER_WAIT_UP);
    CHECK_EQ_INT(g_VSync.count1, 0);
    g_VSync.count1 = 27;
    CHECK(StageTimer_TickAndPut(&f.context));
    g_StageTimer.hurry_cycle = 81;
    g_StageTimer.frames_since_hurry = 500;
    StageTimer_ExtendAndPut(&f.context);
    CHECK_EQ_INT(g_StageTimer.time, 1000);
    CHECK_EQ_INT(g_StageTimer.frames_since_hurry, 0);
    CHECK_EQ_INT(g_StageTimer.hurry_cycle, 81);
    g_StageTimer.time = 65500;
    StageTimer_ExtendAndPut(&f.context);
    CHECK_EQ_INT(g_StageTimer.time, 164);
    g_StageTimer.time = 1;
    CHECK(StageTimer_TickAndPut(&f.context));
    CHECK_EQ_INT(g_StageTimer.time, 65535);
    CHECK_EQ_INT(f.effects, 1);
    Arena_Free(&f.arena);
}

TEST(full_pool_consumes_x_only_and_cycle_and_frame_wrap)
{
    FIXTURE f;
    M_Init(&f);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        g_Pellets.slots[i].moving = true;
    }
    f.display.rank = SETTINGS_DIFFICULTY_LUNATIC;
    uint32_t expected = g_Rng;
    g_StageTimer.hurry_cycle = 255;
    g_StageTimer.frames_since_hurry = 65535;
    for (int32_t i = 0; i < 4; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(StageTimer_TickAndPut(&f.context));
    CHECK_EQ_INT(g_StageTimer.frames_since_hurry, 0);
    CHECK_EQ_INT(g_StageTimer.hurry_cycle, 0);
    CHECK_EQ_INT(g_Rng, expected);
    CHECK(StageTimer_TickAndPut(&f.context));
    CHECK_EQ_INT(g_Rng, expected);
    g_StageTimer.frames_since_hurry = 499;
    for (int32_t i = 0; i < 4; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(StageTimer_TickAndPut(&f.context));
    CHECK_EQ_INT(g_Rng, expected);
    Arena_Free(&f.arena);
}

TEST(redraw_does_not_claim_world_timer_initialization)
{
    FIXTURE f;
    M_Init(&f);
    g_StageTimer.hurry_cycle = 97;
    f.display.stage_number = 20;
    StageTimer_Redraw(&f.context, true, false, 1);
    CHECK_EQ_INT(g_StageTimer.time, 8000);
    CHECK_EQ_INT(g_StageTimer.hurry_cycle, 97);
    g_StageTimer.time = 750;
    StageTimer_Redraw(&f.context, false, true, 0);
    CHECK_EQ_INT(g_StageTimer.time, 750);
    CHECK(!g_Hud.force_numbers);
    StageTimer_Redraw(&f.context, false, false, 0);
    CHECK_EQ_INT(g_StageTimer.time, 6000);
    Arena_Free(&f.arena);
}
