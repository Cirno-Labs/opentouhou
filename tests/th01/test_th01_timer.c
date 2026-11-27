#include "tests/test.h"
#include "tests/th01/th01_world.h"
#include "th01/hud.c"
#include "th01/orb.h"
#include "th01/pellets.c"
#include "th01/pellets.h"
#include "th01/player.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/shots.h"
#include "th01/timer.c"
#include "th01/timer.h"
#include "th01/vsync_irq.c"

#include "core/base/arena.h"
#include "pc98/zun/master_rng.h"

typedef struct {
    ARENA arena;
    GRF mask;
    PTN sprites;
    GAME_TEXT_FONT font;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    g_Rng = 1;
    VSyncIrq_Reset();
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
    Hud_Load(&f->mask, &f->sprites, &f->font);
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_NORMAL;
    g_Session.stage_id = 0;
    Hud_SnapTimer(1000);
}

TEST(route_table_and_reinitialization_keep_cycle)
{
    FIXTURE f;
    M_Init(&f);
    m_Timer.hurry_cycle = 255;
    const uint16_t expected[2][20] = {
        { 1000, 1000, 1000, 1000, 2000, 1200, 1200, 1200, 1200, 3000,
          1400, 1400, 1400, 1400, 4000, 1600, 1600, 1600, 600,  6000 },
        { 1000, 1000, 1000, 1000, 2000, 1200, 1200, 1200, 1200, 2000,
          1400, 1400, 1400, 1400, 4000, 1600, 1800, 1800, 1600, 8000 }
    };
    for (int16_t route = 0; route < 2; route++) {
        for (int16_t stage = 0; stage < 20; stage++) {
            m_Timer.frames_since_hurry = 555;
            g_Session.stage_id = stage;
            StageTimer_Redraw(false, false, route);
            CHECK_EQ_INT(g_Stage.time, expected[route][stage]);
            CHECK_EQ_INT(m_Timer.hurry_cycle, 255);
            CHECK_EQ_INT(m_Timer.frames_since_hurry, 0);
        }
    }
    Arena_Free(&f.arena);
}

TEST(transition_waits_suspend_without_replaying_side_effects)
{
    FIXTURE f;
    M_Init(&f);
    g_Stage.time = 2;
    CHECK(!StageTimer_TickAndPut());
    CHECK_EQ_INT(m_Timer.phase, TIMER_WAIT_HURRY);
    CHECK_EQ_INT(g_Tram->pixels[80 * 640], TRAM_RED + 1);
    for (int32_t i = 0; i < 26; i++) {
        VSyncIrq_Edge();
        CHECK(!StageTimer_TickAndPut());
    }
    VSyncIrq_Edge();
    CHECK(!StageTimer_TickAndPut());
    CHECK_EQ_INT(m_Timer.phase, TIMER_WAIT_UP);
    CHECK_EQ_INT(g_VSync->count1, 0);
    CHECK_EQ_INT(g_Tram->pixels[80 * 640], TRAM_BLACK + 1);
    CHECK_EQ_INT(g_Tram->pixels[80 * 640 + 192], TRAM_RED + 1);
    for (int32_t i = 0; i < 27; i++) {
        VSyncIrq_Edge();
    }
    CHECK(StageTimer_TickAndPut());
    CHECK_EQ_INT(m_Pellets.speed, 2);
    CHECK_EQ_INT(m_Timer.frames_since_hurry, 0);
    CHECK_EQ_INT(m_Timer.hurry_cycle, 0);
    CHECK_EQ_INT(g_Tram->pixels[80 * 640 + 192], 0);
    Arena_Free(&f.arena);
}

TEST(ready_count_is_consumed_before_first_suspension_and_extension_wraps)
{
    FIXTURE f;
    M_Init(&f);
    g_Stage.time = 2;
    m_VSync.count1 = 31;
    m_VSync.count2 = g_VSync->count2;
    CHECK(!StageTimer_TickAndPut());
    CHECK_EQ_INT(m_Timer.phase, TIMER_WAIT_UP);
    CHECK_EQ_INT(g_VSync->count1, 0);
    m_VSync.count1 = 27;
    m_VSync.count2 = g_VSync->count2;
    CHECK(StageTimer_TickAndPut());
    m_Timer.hurry_cycle = 81;
    m_Timer.frames_since_hurry = 500;
    StageTimer_ExtendAndPut();
    CHECK_EQ_INT(g_Stage.time, 1000);
    CHECK_EQ_INT(m_Timer.frames_since_hurry, 0);
    CHECK_EQ_INT(m_Timer.hurry_cycle, 81);
    g_Stage.time = 65500;
    StageTimer_ExtendAndPut();
    CHECK_EQ_INT(g_Stage.time, 164);
    g_Stage.time = 1;
    CHECK(StageTimer_TickAndPut());
    CHECK_EQ_INT(g_Stage.time, 65535);
    Arena_Free(&f.arena);
}

TEST(full_pool_consumes_x_only_and_cycle_and_frame_wrap)
{
    FIXTURE f;
    M_Init(&f);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        m_Pellets.slots[i].moving = true;
    }
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_LUNATIC;
    uint32_t expected = g_Rng;
    m_Timer.hurry_cycle = 255;
    m_Timer.frames_since_hurry = 65535;
    for (int32_t i = 0; i < 4; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(StageTimer_TickAndPut());
    CHECK_EQ_INT(m_Timer.frames_since_hurry, 0);
    CHECK_EQ_INT(m_Timer.hurry_cycle, 0);
    CHECK_EQ_INT(g_Rng, expected);
    CHECK(StageTimer_TickAndPut());
    CHECK_EQ_INT(g_Rng, expected);
    m_Timer.frames_since_hurry = 499;
    for (int32_t i = 0; i < 4; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(StageTimer_TickAndPut());
    CHECK_EQ_INT(g_Rng, expected);
    Arena_Free(&f.arena);
}

TEST(redraw_does_not_claim_world_timer_initialization)
{
    FIXTURE f;
    M_Init(&f);
    m_Timer.hurry_cycle = 97;
    g_Session.stage_id = 19;
    StageTimer_Redraw(true, false, 1);
    CHECK_EQ_INT(g_Stage.time, 8000);
    CHECK_EQ_INT(m_Timer.hurry_cycle, 97);
    g_Stage.time = 750;
    StageTimer_Redraw(false, true, 0);
    CHECK_EQ_INT(g_Stage.time, 750);
    CHECK(!m_Hud.force_numbers);
    StageTimer_Redraw(false, false, 0);
    CHECK_EQ_INT(g_Stage.time, 6000);
    Arena_Free(&f.arena);
}
