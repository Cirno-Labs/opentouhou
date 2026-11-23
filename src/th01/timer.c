#include "th01/timer.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/rng.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"

#include <string.h>

STAGE_TIMER g_StageTimer;

static void M_Line(const char *const text)
{
    const GAME_TEXT_GLYPH *glyphs[5];
    int32_t count = 0;
    for (; text[count] != '\0'; count++) {
        ASSERT(count < 5);
        glyphs[count] =
            GameText_Find(g_Hud.font, (uint32_t)(uint8_t)text[count]);
        ASSERT(glyphs[count] != nullptr);
    }
    Tram_PutLine16(glyphs, count, TRAM_RED, TRAM_BLACK);
}

static bool M_Continue(void)
{
    if (g_StageTimer.phase == TIMER_WAIT_HURRY) {
        if (!Frame_DelayReady(27)) {
            return false;
        }
        M_Line("UP");
        g_StageTimer.phase = TIMER_WAIT_UP;
    }
    if (!Frame_DelayReady(27)) {
        return false;
    }
    Tram_Clear();
    g_StageTimer.phase = TIMER_IDLE;
    return true;
}

static void M_Rain(void)
{
    g_StageTimer.frames_since_hurry++;
    g_StageTimer.hurry_cycle++;
    const uint16_t frames = g_StageTimer.frames_since_hurry;
    const uint8_t frequency = frames < 200 ? 5
        : frames < 300                     ? 4
        : frames < 400                     ? 3
        : frames < 500                     ? 2
                                           : 1;
    if (g_StageTimer.hurry_cycle % frequency != 0) {
        return;
    }
    for (uint32_t i = 0; i <= (uint32_t)g_Session.settings.difficulty; i++) {
        const int16_t left = (int16_t)(MasterRng_Next(&g_Rng) % 640);
        Pellets_AddGroup(
            g_Session.settings.difficulty, left, 64, PELLET_GROUP_RANDOM_WIDE,
            48);
    }
}

static void M_InitFor(const int16_t stage_id, const int16_t route)
{
    static const uint16_t times[35] = {
        1000, 1000, 1000, 1000, 2000, 1200, 1200, 1200, 1200, 3000, 1400, 1400,
        1400, 1400, 4000, 1600, 1600, 1600, 600,  6000, 1200, 1200, 1200, 1200,
        2000, 1400, 1400, 1400, 1400, 4000, 1600, 1800, 1800, 1600, 8000
    };
    ASSERT(stage_id >= 0 && stage_id < 20 && route >= 0 && route <= 1);
    ASSERT(g_StageTimer.phase == TIMER_IDLE);
    g_StageTimer.time = times[stage_id < 5 ? stage_id : route * 15 + stage_id];
    g_StageTimer.frames_since_hurry = 0;
}

void StageTimer_Init(void)
{
    memset(&g_StageTimer, 0, sizeof(g_StageTimer));
}

void StageTimer_Redraw(
    const bool first_stage, const bool timer_initialized, const int16_t route)
{
    Hud_PutMask();
    if (!timer_initialized) {
        M_InitFor(g_Session.stage_id, route);
    }
    if (first_stage) {
        Hud_SnapResources();
        Hud_SnapStage();
        Hud_SnapTimer(g_StageTimer.time);
    }
    Hud_PutStage();
    g_Hud.maximum_combo = 0;
    Hud_SnapNumbers(first_stage);
    Hud_PutTimer(g_StageTimer.time);
    g_Hud.force_numbers = true;
    Hud_ScoreAndCombo();
    g_Hud.force_numbers = false;
}

bool StageTimer_TickAndPut(void)
{
    if (g_StageTimer.phase != TIMER_IDLE) {
        return M_Continue();
    }
    if (g_StageTimer.time == 0) {
        M_Rain();
        return true;
    }
    g_StageTimer.time = (uint16_t)(g_StageTimer.time - 2);
    Hud_PutTimer(g_StageTimer.time);
    if (g_StageTimer.time == 0) {
        Pellets_SpeedRaise(2);
        Sound_Effect(17);
        M_Line("HARRY");
        g_StageTimer.phase = TIMER_WAIT_HURRY;
        return M_Continue();
    }
    return true;
}

void StageTimer_ExtendAndPut(void)
{
    ASSERT(g_StageTimer.phase == TIMER_IDLE);
    g_StageTimer.time =
        (uint16_t)(g_StageTimer.time + (g_StageTimer.time != 0 ? 200 : 1000));
    Hud_PutTimer(g_StageTimer.time);
    g_StageTimer.frames_since_hurry = 0;
}
