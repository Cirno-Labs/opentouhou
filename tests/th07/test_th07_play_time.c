#include "tests/shell_fake.h"
#include "tests/test.h"
#include "th07/game_manager.h"
#include "th07/play_time.h"

static void M_Setup(void)
{
    FakeShell_Reset();
    GameManager_Create(&g_GameManager);
}

TEST(add_run_accumulates_into_the_run_time)
{
    M_Setup();
    g_FakeShell.wall_clock = 100;
    PlayTime_AddRun();
    g_FakeShell.wall_clock = 2100;
    PlayTime_AddRun();
    const PLAY_STATS_RECORD *const stats = &g_GameManager.records.play_stats;
    CHECK_EQ_INT(stats->run_hours, 0);
    CHECK_EQ_INT(stats->run_minutes, 0);
    CHECK_EQ_INT(stats->run_seconds, 2);
    CHECK_EQ_INT(stats->run_milliseconds, 100);
}

TEST(add_carries_milliseconds_into_seconds_minutes_and_hours)
{
    M_Setup();
    g_FakeShell.wall_clock = 0;
    PlayTime_Add();
    g_FakeShell.wall_clock = (uint64_t)3661 * 1000 + 500;
    PlayTime_Add();
    const PLAY_STATS_RECORD *const stats = &g_GameManager.records.play_stats;
    CHECK_EQ_INT(stats->play_hours, 1);
    CHECK_EQ_INT(stats->play_minutes, 1);
    CHECK_EQ_INT(stats->play_seconds, 1);
    CHECK_EQ_INT(stats->play_milliseconds, 500);
}

TEST(restart_moves_the_baseline_without_adding)
{
    M_Setup();
    g_FakeShell.wall_clock = 1000;
    PlayTime_Add();
    g_FakeShell.wall_clock = 5000;
    PlayTime_Restart();
    const PLAY_STATS_RECORD *const stats = &g_GameManager.records.play_stats;
    CHECK_EQ_INT(stats->play_seconds, 1);
    g_FakeShell.wall_clock = 6000;
    PlayTime_Add();
    CHECK_EQ_INT(stats->play_seconds, 2);
}

TEST(stop_zeroes_the_baseline)
{
    M_Setup();
    g_FakeShell.wall_clock = 1000;
    PlayTime_Add();
    PlayTime_Stop();
    CHECK_EQ_INT(g_GameManager.play_time.play_tick, 0);
}

TEST(a_wall_clock_going_backward_resets_the_baseline)
{
    M_Setup();
    g_FakeShell.wall_clock = 5000;
    PlayTime_Add();
    g_FakeShell.wall_clock = 2000;
    PlayTime_Add();
    const PLAY_STATS_RECORD *const stats = &g_GameManager.records.play_stats;
    // The second call sees the wall clock behind the baseline, resets the
    // baseline to 0, and adds the whole new reading as elapsed time.
    CHECK_EQ_INT(stats->play_seconds, 7);
}
