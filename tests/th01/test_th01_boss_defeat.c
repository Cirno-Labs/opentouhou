#include "tests/test.h"
#include "tests/th01/th01_world.h"
#include "th01/boss/private.h"
#include "th01/orb.h"
#include "th01/pellets.h"
#include "th01/player_private.h"
#include "th01/rng.h"
#include "th01/shots.h"
#include "th01/vsync_irq.c"

#include "core/base/arena.h"

#include <string.h>

static const BOSS_DEFEAT_PARAMS m_Regular = {
    .shift = 16,
    .step = 2,
    .whiteout_period = 7,
    .whitein_period = 5,
    .clears_stage = true,
};

static const BOSS_DEFEAT_PARAMS m_Singyoku = {
    .shift = 48,
    .step = 1,
    .whiteout_period = 15,
    .whitein_period = 15,
    .clears_stage = false,
};

typedef struct {
    PALETTE target;
    PTN miko;
    int16_t origin;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena, const int8_t target)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    Video_Init(Arena_Allocator(arena));
    memset(g_Video->pages[0], 2, VIDEO_PAGE_SIZE);
    memset(g_Video->pages[1], 5, VIDEO_PAGE_SIZE);
    Palette_SetGray(p_Palette, 3);
    memset(f->target.colors, target, sizeof(f->target.colors));
    f->miko = (PTN) { .count = 1, .pixels = Arena_Alloc(arena, 1024) };
    memset(f->miko.pixels, PTN_TRANSPARENT, 1024);
    f->miko.pixels[0] = 2;
    p_Player->left = 17;
    p_Player->mode_frame = 9;
    p_Player->bomb_frame = 123;
    f->origin = 64;
}

static void M_ReachWhitein(BOSS_DEFEAT *const defeat, FIXTURE *const f)
{
    int32_t edges = 0;
    while (defeat->phase != BOSS_DEFEAT_WHITEIN_DELAY && edges < 500) {
        VSyncIrq_Edge();
        CHECK(!BossDefeat_Resume(defeat, &f->target, &f->miko, &f->origin));
        edges++;
    }
    CHECK(edges < 500);
    CHECK_EQ_INT(defeat->phase, BOSS_DEFEAT_WHITEIN_DELAY);
    CHECK_EQ_INT(defeat->frame, 1);
}

TEST(raw_wait_reuses_counter_without_replaying_lines)
{
    ARENA arena;
    Arena_Init(&arena, "defeat", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena, 14);
    VSyncIrq_Reset();
    m_VSync.edge = 42;
    m_VSync.count1 = 1000;
    m_VSync.count2 = 8;
    BOSS_DEFEAT defeat = {};
    Video_SetScroll(12);
    BossDefeat_Begin(&defeat, m_Regular);
    CHECK(!BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin));
    CHECK_EQ_INT(g_Video->scroll_top, 12);
    CHECK_EQ_INT(g_Video->pages[0][80 * VIDEO_WIDTH], 2);
    CHECK_EQ_INT(g_VSync->count1, 1000);
    f.origin = 80;
    VSyncIrq_Edge();
    CHECK(!BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin));
    CHECK_EQ_INT(g_Video->scroll_top, 0);
    CHECK_EQ_INT(defeat.frame, 2);
    CHECK_EQ_INT(defeat.top, 92);
    CHECK_EQ_INT(defeat.bottom, 100);
    CHECK_EQ_INT(g_Video->pages[0][94 * VIDEO_WIDTH], 7);
    CHECK_EQ_INT(g_Video->pages[0][96 * VIDEO_WIDTH], 7);
    CHECK_EQ_INT(g_Video->pages[0][98 * VIDEO_WIDTH], 7);
    CHECK_EQ_INT(g_Video->pages[0][92 * VIDEO_WIDTH], 2);
    CHECK_EQ_INT(g_VSync->count1, 0);
    CHECK_EQ_INT(g_VSync->count2, 9);
    const BOSS_DEFEAT waiting = defeat;
    CHECK(!BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin));
    CHECK(memcmp(&waiting, &defeat, sizeof(waiting)) == 0);
    CHECK(!g_Stage->cleared && !p_Player->is_hit);
    Arena_Free(&arena);
}

TEST(restore_and_extra_fade_interval_precede_clear_flags)
{
    ARENA arena;
    Arena_Init(&arena, "defeat", 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena, 14);
    VSyncIrq_Reset();
    BOSS_DEFEAT defeat = {};
    BossDefeat_Begin(&defeat, m_Regular);
    M_ReachWhitein(&defeat, &f);
    CHECK_EQ_INT(g_Video->drawing_page, 0);
    CHECK_EQ_INT(g_Palette->saved.colors[0][0], 15);
    CHECK_EQ_INT(g_Video->palette[0].red, 255);
    CHECK_EQ_INT(g_Video->pages[0][368 * VIDEO_WIDTH + 16], 2);
    CHECK_EQ_INT(g_Video->pages[0][368 * VIDEO_WIDTH + 17], 5);
    CHECK_EQ_INT(g_Video->pages[1][368 * VIDEO_WIDTH + 16], 5);
    CHECK_EQ_INT(p_Player->mode_frame, 9);
    CHECK_EQ_INT(p_Player->bomb_frame, 123);
    for (int32_t frame = 2; frame < 10; frame++) {
        VSyncIrq_Edge();
        CHECK(!BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin));
        CHECK_EQ_INT(defeat.frame, frame);
        CHECK_EQ_INT(g_Palette->saved.colors[0][0], frame >= 5 ? 14 : 15);
        CHECK(!g_Stage->cleared && !p_Player->is_hit);
    }
    VSyncIrq_Edge();
    CHECK(BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin));
    CHECK_EQ_INT(defeat.frame, 10);
    CHECK(g_Stage->cleared && p_Player->is_hit);
    const VSYNC_COUNTERS finished = *g_VSync;
    CHECK(BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin));
    CHECK(memcmp(&finished, g_VSync, sizeof(finished)) == 0);
    Arena_Free(&arena);
}

TEST(singyoku_parameters_leave_route_and_gameplay_flags_pending)
{
    ARENA arena;
    Arena_Init(&arena, "defeat", 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena, 15);
    f.origin = 80;
    VSyncIrq_Reset();
    m_VSync.count1 = 10;
    m_VSync.count2 = g_VSync->count2;
    BOSS_DEFEAT defeat = {};
    BossDefeat_Begin(&defeat, m_Singyoku);
    VSyncIrq_Edge();
    CHECK(!BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin));
    CHECK_EQ_INT(defeat.top, 126);
    CHECK_EQ_INT(defeat.bottom, 130);
    M_ReachWhitein(&defeat, &f);
    for (int32_t frame = 2; frame <= 15; frame++) {
        VSyncIrq_Edge();
        CHECK_EQ_INT(
            BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin),
            frame == 15);
        CHECK(!g_Stage->cleared && !p_Player->is_hit);
    }
    CHECK_EQ_INT(defeat.phase, BOSS_DEFEAT_DONE);
    Arena_Free(&arena);
}

TEST(live_target_can_stop_below_a_raised_palette)
{
    ARENA arena;
    Arena_Init(&arena, "defeat", 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena, 14);
    VSyncIrq_Reset();
    BOSS_DEFEAT defeat = {};
    BossDefeat_Begin(&defeat, m_Regular);
    M_ReachWhitein(&defeat, &f);
    while (defeat.frame < 5) {
        VSyncIrq_Edge();
        CHECK(!BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin));
    }
    memset(f.target.colors, 15, sizeof(f.target.colors));
    bool done = false;
    while (!done && defeat.frame < 20) {
        VSyncIrq_Edge();
        done = BossDefeat_Resume(&defeat, &f.target, &f.miko, &f.origin);
    }
    CHECK(done);
    CHECK_EQ_INT(defeat.frame, 10);
    CHECK_EQ_INT(g_Palette->saved.colors[0][0], 14);
    Arena_Free(&arena);
}
