#include "th01/boss_defeat.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    PALETTE_STATE palette;
    PALETTE target;
    PTN miko;
    PLAYER player;
    int16_t origin;
    bool cleared;
    BOSS_DEFEAT_CONTEXT context;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena, const int8_t target)
{
    *f = (FIXTURE) {};
    Video_Init(Arena_Allocator(arena));
    memset(g_Video.pages[0], 2, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 5, VIDEO_PAGE_SIZE);
    Palette_Init(&f->palette);
    Palette_SetGray(&f->palette, 3);
    memset(f->target.colors, target, sizeof(f->target.colors));
    f->miko = (PTN) { .count = 1, .pixels = Arena_Alloc(arena, 1024) };
    memset(f->miko.pixels, PTN_TRANSPARENT, 1024);
    f->miko.pixels[0] = 2;
    f->player.left = 17;
    f->player.mode_frame = 9;
    f->player.bomb_frame = 123;
    f->origin = 64;
    f->context = (BOSS_DEFEAT_CONTEXT) { &f->palette, &f->target, &f->miko,
                                         &f->player,  &f->origin, &f->cleared };
}

static void M_ReachWhitein(BOSS_DEFEAT *const defeat, FIXTURE *const f)
{
    int32_t edges = 0;
    while (defeat->phase != BOSS_DEFEAT_WHITEIN_DELAY && edges < 500) {
        Frame_VSync();
        CHECK(!BossDefeat_Resume(defeat, &f->context));
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
    Frame_Reset();
    g_VSync.edge = 42;
    g_VSync.count1 = 1000;
    g_VSync.count2 = 8;
    BOSS_DEFEAT defeat = {};
    Video_SetScroll(12);
    BossDefeat_Begin(&defeat, BOSS_DEFEAT_REGULAR);
    CHECK(!BossDefeat_Resume(&defeat, &f.context));
    CHECK_EQ_INT(g_Video.scroll_top, 12);
    CHECK_EQ_INT(g_Video.pages[0][80 * VIDEO_WIDTH], 2);
    CHECK_EQ_INT(g_VSync.count1, 1000);
    f.origin = 80;
    Frame_VSync();
    CHECK(!BossDefeat_Resume(&defeat, &f.context));
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    CHECK_EQ_INT(defeat.frame, 2);
    CHECK_EQ_INT(defeat.top, 92);
    CHECK_EQ_INT(defeat.bottom, 100);
    CHECK_EQ_INT(g_Video.pages[0][94 * VIDEO_WIDTH], 7);
    CHECK_EQ_INT(g_Video.pages[0][96 * VIDEO_WIDTH], 7);
    CHECK_EQ_INT(g_Video.pages[0][98 * VIDEO_WIDTH], 7);
    CHECK_EQ_INT(g_Video.pages[0][92 * VIDEO_WIDTH], 2);
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, 9);
    const BOSS_DEFEAT waiting = defeat;
    CHECK(!BossDefeat_Resume(&defeat, &f.context));
    CHECK(memcmp(&waiting, &defeat, sizeof(waiting)) == 0);
    CHECK(!f.cleared && !f.player.is_hit);
    Arena_Free(&arena);
}

TEST(restore_and_extra_fade_interval_precede_clear_flags)
{
    ARENA arena;
    Arena_Init(&arena, "defeat", 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena, 14);
    Frame_Reset();
    BOSS_DEFEAT defeat = {};
    BossDefeat_Begin(&defeat, BOSS_DEFEAT_REGULAR);
    M_ReachWhitein(&defeat, &f);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(f.palette.saved.colors[0][0], 15);
    CHECK_EQ_INT(g_Video.palette[0].red, 255);
    CHECK_EQ_INT(g_Video.pages[0][368 * VIDEO_WIDTH + 16], 2);
    CHECK_EQ_INT(g_Video.pages[0][368 * VIDEO_WIDTH + 17], 5);
    CHECK_EQ_INT(g_Video.pages[1][368 * VIDEO_WIDTH + 16], 5);
    CHECK_EQ_INT(f.player.mode_frame, 9);
    CHECK_EQ_INT(f.player.bomb_frame, 123);
    for (int32_t frame = 2; frame < 10; frame++) {
        Frame_VSync();
        CHECK(!BossDefeat_Resume(&defeat, &f.context));
        CHECK_EQ_INT(defeat.frame, frame);
        CHECK_EQ_INT(f.palette.saved.colors[0][0], frame >= 5 ? 14 : 15);
        CHECK(!f.cleared && !f.player.is_hit);
    }
    Frame_VSync();
    CHECK(BossDefeat_Resume(&defeat, &f.context));
    CHECK_EQ_INT(defeat.frame, 10);
    CHECK(f.cleared && f.player.is_hit);
    const FRAME_VSYNC finished = g_VSync;
    CHECK(BossDefeat_Resume(&defeat, &f.context));
    CHECK(memcmp(&finished, &g_VSync, sizeof(g_VSync)) == 0);
    Arena_Free(&arena);
}

TEST(singyoku_parameters_leave_route_and_gameplay_flags_pending)
{
    ARENA arena;
    Arena_Init(&arena, "defeat", 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena, 15);
    f.origin = 80;
    Frame_Reset();
    g_VSync.count1 = 10;
    BOSS_DEFEAT defeat = {};
    BossDefeat_Begin(&defeat, BOSS_DEFEAT_SINGYOKU);
    Frame_VSync();
    CHECK(!BossDefeat_Resume(&defeat, &f.context));
    CHECK_EQ_INT(defeat.top, 126);
    CHECK_EQ_INT(defeat.bottom, 130);
    M_ReachWhitein(&defeat, &f);
    for (int32_t frame = 2; frame <= 15; frame++) {
        Frame_VSync();
        CHECK_EQ_INT(BossDefeat_Resume(&defeat, &f.context), frame == 15);
        CHECK(!f.cleared && !f.player.is_hit);
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
    Frame_Reset();
    BOSS_DEFEAT defeat = {};
    BossDefeat_Begin(&defeat, BOSS_DEFEAT_REGULAR);
    M_ReachWhitein(&defeat, &f);
    while (defeat.frame < 5) {
        Frame_VSync();
        CHECK(!BossDefeat_Resume(&defeat, &f.context));
    }
    memset(f.target.colors, 15, sizeof(f.target.colors));
    bool done = false;
    while (!done && defeat.frame < 20) {
        Frame_VSync();
        done = BossDefeat_Resume(&defeat, &f.context);
    }
    CHECK(done);
    CHECK_EQ_INT(defeat.frame, 10);
    CHECK_EQ_INT(f.palette.saved.colors[0][0], 14);
    Arena_Free(&arena);
}
