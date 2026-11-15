#include "th07/screen_effect.h"

#include "th07/anm.h"
#include "th07/game.h"
#include "th07/stubs.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/gfx.h"
#include "core/math/x87.h"

#include "stg/chain.h"
#include "stg/rng.h"
#include "stg/timer.h"

#define M_POOL_SIZE 32
#define M_CALC_PRIORITY 15
#define M_DRAW_PRIORITY 17

typedef enum {
    M_AREA_SCREEN,
    M_AREA_PLAY,
} M_AREA;

typedef struct {
    bool in_use;
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    TIMER timer;
    int32_t duration;
    int32_t alpha;
    uint32_t color;
    M_AREA area;
    int32_t shake_from;
    int32_t shake_to;
    int32_t flashes; // left to run
    int32_t flash_alpha; // where each flash starts
} M_EFFECT;

static M_EFFECT m_Pool[M_POOL_SIZE];

static RESULT M_Added(void *const arg)
{
    M_EFFECT *const effect = arg;
    Timer_Set(&effect->timer, 0);
    return OK;
}

static void M_Deleted(void *const arg)
{
    M_EFFECT *const effect = arg;
    Chain_Remove(&effect->draw);
    effect->in_use = false;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    const M_EFFECT *const effect = arg;
    Anm_DrawRect(
        effect->area == M_AREA_PLAY ? (GFX_RECT) { 32, 16, 384, 448 }
                                    : (GFX_RECT) { 0, 0, 640, 480 },
        (uint32_t)effect->alpha << 24 | effect->color);
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Fade(M_EFFECT *const effect, const bool in)
{
    if (effect->duration != 0) {
        const float faded =
            Timer_AsFloat(&effect->timer) * 255.0f / (float)effect->duration;
        effect->alpha = X87_Ftol(in ? 255.0f - faded : faded);
        if (effect->alpha < 0) {
            effect->alpha = 0;
        }
    }
    if (effect->timer.current >= effect->duration) {
        return CHAIN_REMOVE;
    }
    Timer_TickAt(&effect->timer, g_GameSpeed);
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_FadeIn(void *const arg)
{
    return M_Fade(arg, true);
}

static CHAIN_STATUS M_FadeOut(void *const arg)
{
    return M_Fade(arg, false);
}

static CHAIN_STATUS M_Flash(void *const arg)
{
    M_EFFECT *const effect = arg;
    if (effect->timer.current < effect->duration) {
        effect->alpha = effect->flash_alpha
            - X87_Ftol((float)effect->flash_alpha
                       * Timer_AsFloat(&effect->timer)
                       / (float)effect->duration);
        if (effect->alpha < 0) {
            effect->alpha = 0;
        }
    } else {
        effect->alpha = 0;
        if (--effect->flashes <= 0) {
            return CHAIN_REMOVE;
        }
        Timer_Set(&effect->timer, 0);
    }
    // Ticks after the restart too, so each flash after the first starts
    // a frame in.
    Timer_TickAt(&effect->timer, g_GameSpeed);
    return CHAIN_CONTINUE;
}

static float M_ShakeAxis(const float shake)
{
    switch (Rng_U32InRange(&g_Rng, 3)) {
    case 1:
        return shake;
    case 2:
        return -shake;
    default:
        return 0.0f;
    }
}

static CHAIN_STATUS M_Shake(void *const arg)
{
    M_EFFECT *const effect = arg;
    if (g_GameManager.mode.frozen) {
        return CHAIN_CONTINUE;
    }
    if (g_GameManager.run.stage_frames <= 1) {
        return CHAIN_REMOVE;
    }
    Timer_TickAt(&effect->timer, g_GameSpeed);
    // Ends without clearing the offset; the HUD's draw clears it.
    if (effect->timer.current >= effect->duration) {
        return CHAIN_REMOVE;
    }
    const float shake = (float)(effect->shake_to - effect->shake_from)
            * Timer_AsFloat(&effect->timer) / (float)effect->duration
        + (float)effect->shake_from;
    const float x = M_ShakeAxis(shake);
    const float y = M_ShakeAxis(shake);
    Anm_SetShake(x, y);
    return CHAIN_CONTINUE;
}

static void M_Start(
    CHAIN_STATUS (*const update)(void *), const bool draws,
    const M_EFFECT *const setup)
{
    M_EFFECT *effect = m_Pool;
    while (effect->in_use) {
        if (++effect == m_Pool + M_POOL_SIZE) {
            LOG_FATAL("more than %d screen effects", M_POOL_SIZE);
        }
    }
    *effect = *setup;
    effect->in_use = true;
    Chain_InitElem(&effect->calc, update, effect);
    effect->calc.added = M_Added;
    effect->calc.deleted = M_Deleted;
    Chain_InitElem(&effect->draw, M_Draw, effect);
    ASSERT(IS_OK(Chain_AddCalc(&g_Chain, &effect->calc, M_CALC_PRIORITY)));
    if (draws) {
        ASSERT(IS_OK(Chain_AddDraw(&g_Chain, &effect->draw, M_DRAW_PRIORITY)));
    }
}

void ScreenEffect_FadeIn(const int32_t duration, const uint32_t color)
{
    M_Start(
        M_FadeIn, true, &(M_EFFECT) { .duration = duration, .color = color });
}

void ScreenEffect_Shake(
    const int32_t duration, const int32_t from, const int32_t to)
{
    M_Start(
        M_Shake, false,
        &(M_EFFECT) {
            .duration = duration, .shake_from = from, .shake_to = to });
}

void ScreenEffect_FadeOut(const int32_t duration, const uint32_t color)
{
    M_Start(
        M_FadeOut, true,
        &(M_EFFECT) {
            .duration = duration, .color = color, .area = M_AREA_PLAY });
}

void ScreenEffect_FadeOutScreen(const int32_t duration, const uint32_t color)
{
    M_Start(
        M_FadeOut, true, &(M_EFFECT) { .duration = duration, .color = color });
}

void ScreenEffect_Flash(
    const int32_t duration, const int32_t count, const uint32_t color)
{
    M_Start(
        M_Flash, true,
        &(M_EFFECT) { .duration = duration,
                      .color = color & 0xffffff,
                      .flashes = count,
                      .flash_alpha = (int32_t)(color >> 24),
                      .area = M_AREA_PLAY });
}
