#include "th06/screen_effect.h"

#include "th06/chain.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/timer.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#define M_POOL_SIZE 16
#define M_CALC_PRIORITY 14
#define M_DRAW_PRIORITY 16

typedef struct {
    bool in_use;
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    TIMER timer;
    int32_t duration;
    int32_t alpha;
    uint32_t color;
    int32_t shake_from;
    int32_t shake_to;
} M_EFFECT;

static M_EFFECT m_Pool[M_POOL_SIZE];

static const PLAY_RECT m_PlayArea = { 32.0f, 16.0f, 384.0f, 448.0f };

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
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Fade(M_EFFECT *const effect, const bool in)
{
    if (effect->duration != 0) {
        const float faded =
            (float)effect->timer.current * 255.0f / (float)effect->duration;
        effect->alpha = X87_Ftol(in ? 255.0f - faded : faded);
        if (effect->alpha < 0) {
            effect->alpha = 0;
        }
    }
    if (effect->timer.current >= effect->duration) {
        return CHAIN_REMOVE;
    }
    Timer_Tick(&effect->timer);
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

static CHAIN_STATUS M_Shake(void *const arg)
{
    M_EFFECT *const effect = arg;
    PLAY_RECT *const area = &g_GameManager.play_area;
    if (g_GameManager.mode.frozen) {
        *area = m_PlayArea;
        return CHAIN_CONTINUE;
    }
    Timer_Tick(&effect->timer);
    if (effect->timer.current >= effect->duration) {
        *area = m_PlayArea;
        return CHAIN_REMOVE;
    }
    const float shake = (float)effect->timer.current
            * (float)(effect->shake_to - effect->shake_from)
            / (float)effect->duration
        + (float)effect->shake_from;
    switch (Rng_U32InRange(&g_Rng, 3)) {
    case 0:
        area->x = m_PlayArea.x;
        area->width = m_PlayArea.width;
        break;
    case 1:
        area->x = m_PlayArea.x + shake;
        area->width = m_PlayArea.width - shake;
        break;
    case 2:
        area->x = m_PlayArea.x;
        area->width = m_PlayArea.width - shake;
        break;
    }
    switch (Rng_U32InRange(&g_Rng, 3)) {
    case 0:
        area->y = m_PlayArea.y;
        area->height = m_PlayArea.height;
        break;
    case 1:
        area->y = m_PlayArea.y + shake;
        area->height = m_PlayArea.height - shake;
        break;
    case 2:
        area->y = m_PlayArea.y;
        area->height = m_PlayArea.height - shake;
        break;
    }
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
        M_FadeOut, true, &(M_EFFECT) { .duration = duration, .color = color });
}
