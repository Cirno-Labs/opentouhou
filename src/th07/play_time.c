#include "th07/play_time.h"

#include "th07/game_manager.h"

#include "shell/game/requests.h"

#include <stdint.h>

// Adds delta milliseconds to hours, minutes, seconds and milliseconds, as
// App::AddPlayTime and App::AddTotalPlayTime both do.
static void M_AddElapsed(
    uint32_t *const hours, uint32_t *const minutes, uint32_t *const seconds,
    uint32_t *const milliseconds, uint64_t delta)
{
    *hours += (uint32_t)(delta / 3600000);
    delta %= 3600000;
    *minutes += (uint32_t)(delta / 60000);
    delta %= 60000;
    *seconds += (uint32_t)(delta / 1000);
    delta %= 1000;
    *milliseconds += (uint32_t)delta;
    if (*milliseconds >= 1000) {
        *seconds += *milliseconds / 1000;
        *milliseconds %= 1000;
    }
    if (*seconds >= 60) {
        *minutes += *seconds / 60;
        *seconds %= 60;
    }
    if (*minutes >= 60) {
        *hours += *minutes / 60;
        *minutes %= 60;
    }
}

void PlayTime_AddRun(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    const uint64_t now = Shell_GetWallClock();
    if (now < gm->play_time.run_tick) {
        gm->play_time.run_tick = 0;
    }
    PLAY_STATS_RECORD *const stats = &gm->records.play_stats;
    M_AddElapsed(
        &stats->run_hours, &stats->run_minutes, &stats->run_seconds,
        &stats->run_milliseconds, now - gm->play_time.run_tick);
    gm->play_time.run_tick = now;
}

void PlayTime_Add(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    const uint64_t now = Shell_GetWallClock();
    if (now < gm->play_time.play_tick) {
        gm->play_time.play_tick = 0;
    }
    PLAY_STATS_RECORD *const stats = &gm->records.play_stats;
    M_AddElapsed(
        &stats->play_hours, &stats->play_minutes, &stats->play_seconds,
        &stats->play_milliseconds, now - gm->play_time.play_tick);
    gm->play_time.play_tick = now;
}

void PlayTime_Restart(void)
{
    g_GameManager.play_time.play_tick = Shell_GetWallClock();
}

void PlayTime_Stop(void)
{
    g_GameManager.play_time.play_tick = 0;
}
