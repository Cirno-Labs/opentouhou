#include "th01/timer.h"
#include "th01/vsync_irq.h"
#include "th01/hud.h"
#include "th01/pellets.h"
#include "th01/tram.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/rng.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"

#include <string.h>

typedef enum {
    TIMER_IDLE,
    TIMER_WAIT_HURRY,
    TIMER_WAIT_UP,
} TIMER_PHASE;

typedef struct {
    uint16_t frames_since_hurry;
    uint8_t hurry_cycle;
    TIMER_PHASE phase;
} STAGE_TIMER;

static STAGE_TIMER m_Timer;

static bool M_Continue(void)
{
    if (m_Timer.phase == TIMER_WAIT_HURRY) {
        if (!VSyncIrq_DelayReady(27)) {
            return false;
        }
        Hud_PutMessage("UP");
        m_Timer.phase = TIMER_WAIT_UP;
    }
    if (!VSyncIrq_DelayReady(27)) {
        return false;
    }
    Tram_Clear();
    m_Timer.phase = TIMER_IDLE;
    return true;
}

static void M_Rain(void)
{
    m_Timer.frames_since_hurry++;
    m_Timer.hurry_cycle++;
    const uint16_t frames = m_Timer.frames_since_hurry;
    const uint8_t frequency = frames < 200 ? 5
        : frames < 300                     ? 4
        : frames < 400                     ? 3
        : frames < 500                     ? 2
                                           : 1;
    if (m_Timer.hurry_cycle % frequency != 0) {
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
    ASSERT(m_Timer.phase == TIMER_IDLE);
    g_Stage.time = times[stage_id < 5 ? stage_id : route * 15 + stage_id];
    m_Timer.frames_since_hurry = 0;
}

void StageTimer_Init(void)
{
    memset(&m_Timer, 0, sizeof(m_Timer));
    g_Stage.time = 0;
}

void StageTimer_Redraw(
    const bool first_stage, const bool timer_initialized, const int16_t route)
{
    Hud_PutMask();
    if (!timer_initialized) {
        M_InitFor(g_Session.stage_id, route);
    }
    Hud_Redraw(first_stage, g_Stage.time);
}

bool StageTimer_TickAndPut(void)
{
    if (m_Timer.phase != TIMER_IDLE) {
        return M_Continue();
    }
    if (g_Stage.time == 0) {
        M_Rain();
        return true;
    }
    g_Stage.time = (uint16_t)(g_Stage.time - 2);
    Hud_PutTimer(g_Stage.time);
    if (g_Stage.time == 0) {
        Pellets_SpeedRaise(2);
        Sound_Effect(17);
        Hud_PutMessage("HARRY");
        m_Timer.phase = TIMER_WAIT_HURRY;
        return M_Continue();
    }
    return true;
}

void StageTimer_ExtendAndPut(void)
{
    ASSERT(m_Timer.phase == TIMER_IDLE);
    g_Stage.time = (uint16_t)(g_Stage.time + (g_Stage.time != 0 ? 200 : 1000));
    Hud_PutTimer(g_Stage.time);
    m_Timer.frames_since_hurry = 0;
}
