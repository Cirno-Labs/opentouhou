#include "th01/timer.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"

static void M_Line(const TIMER_CONTEXT *const c, const char *const text)
{
    const GAME_TEXT_GLYPH *glyphs[5];
    int32_t count = 0;
    for (; text[count] != '\0'; count++) {
        ASSERT(count < 5);
        glyphs[count] =
            GameText_Find(c->display->font, (uint32_t)(uint8_t)text[count]);
        ASSERT(glyphs[count] != nullptr);
    }
    Tram_PutLine16(c->tram, glyphs, count, TRAM_RED, TRAM_BLACK);
}

static bool M_Continue(STAGE_TIMER *const timer, const TIMER_CONTEXT *const c)
{
    if (timer->phase == TIMER_WAIT_HURRY) {
        if (!Frame_DelayReady(c->vsync, 27)) {
            return false;
        }
        M_Line(c, "UP");
        timer->phase = TIMER_WAIT_UP;
    }
    if (!Frame_DelayReady(c->vsync, 27)) {
        return false;
    }
    Tram_Clear(c->tram);
    timer->phase = TIMER_IDLE;
    return true;
}

static void M_Rain(STAGE_TIMER *const timer, const TIMER_CONTEXT *const c)
{
    timer->frames_since_hurry++;
    timer->hurry_cycle++;
    const uint16_t frames = timer->frames_since_hurry;
    const uint8_t frequency = frames < 200 ? 5
        : frames < 300                     ? 4
        : frames < 400                     ? 3
        : frames < 500                     ? 2
                                           : 1;
    if (timer->hurry_cycle % frequency != 0) {
        return;
    }
    for (uint32_t i = 0; i <= (uint32_t)c->display->rank; i++) {
        const int16_t left = (int16_t)(MasterRng_Next(c->rng) % 640);
        Pellets_AddGroup(
            c->pellets, c->shared, c->display->player, c->rng, c->display->rank,
            left, 64, PELLET_GROUP_RANDOM_WIDE, 48);
    }
}

static void M_InitFor(
    STAGE_TIMER *const timer, const int16_t stage_id, const int16_t route)
{
    static const uint16_t times[35] = {
        1000, 1000, 1000, 1000, 2000, 1200, 1200, 1200, 1200, 3000, 1400, 1400,
        1400, 1400, 4000, 1600, 1600, 1600, 600,  6000, 1200, 1200, 1200, 1200,
        2000, 1400, 1400, 1400, 1400, 4000, 1600, 1800, 1800, 1600, 8000
    };
    ASSERT(stage_id >= 0 && stage_id < 20 && route >= 0 && route <= 1);
    ASSERT(timer->phase == TIMER_IDLE);
    timer->time = times[stage_id < 5 ? stage_id : route * 15 + stage_id];
    timer->frames_since_hurry = 0;
}

void Timer_Redraw(
    STAGE_TIMER *const timer, const TIMER_CONTEXT *const c,
    const bool first_stage, const bool timer_initialized, const int16_t route)
{
    Hud_PutMask(c->display);
    if (!timer_initialized) {
        M_InitFor(timer, Math16_Wrap(c->display->stage_number - 1), route);
    }
    if (first_stage) {
        Hud_SnapResources(c->hud, c->display);
        Hud_SnapStage(c->hud, c->display);
        Hud_SnapTimer(c->hud, c->display, timer->time);
    }
    Hud_PutStage(c->hud, c->display);
    c->hud->maximum_combo = 0;
    Hud_SnapNumbers(c->hud, c->display, first_stage);
    Hud_PutTimer(c->hud, c->display, timer->time);
    c->hud->force_numbers = true;
    Hud_ScoreAndCombo(c->hud, c->display);
    c->hud->force_numbers = false;
}

bool Timer_TickAndPut(STAGE_TIMER *const timer, const TIMER_CONTEXT *const c)
{
    if (timer->phase != TIMER_IDLE) {
        return M_Continue(timer, c);
    }
    if (timer->time == 0) {
        M_Rain(timer, c);
        return true;
    }
    timer->time = (uint16_t)(timer->time - 2);
    Hud_PutTimer(c->hud, c->display, timer->time);
    if (timer->time == 0) {
        Pellets_SpeedRaise(c->shared, 2);
        c->effect(c->user, 17);
        M_Line(c, "HARRY");
        timer->phase = TIMER_WAIT_HURRY;
        return M_Continue(timer, c);
    }
    return true;
}

void Timer_ExtendAndPut(STAGE_TIMER *const timer, const TIMER_CONTEXT *const c)
{
    ASSERT(timer->phase == TIMER_IDLE);
    timer->time = (uint16_t)(timer->time + (timer->time != 0 ? 200 : 1000));
    Hud_PutTimer(c->hud, c->display, timer->time);
    timer->frames_since_hurry = 0;
}
