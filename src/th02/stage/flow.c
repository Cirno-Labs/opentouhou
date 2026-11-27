#include "th02/stage/flow.h"

#include "th02/enemies.h"
#include "th02/stage.h"
#include "th02/stage/private.h"
#include "th02/stage_effects.h"

#include "core/base/assert.h"

#include <string.h>

typedef enum {
    STAGE_FLOW_SCROLL,
    STAGE_FLOW_ENTER,
    STAGE_FLOW_FIGHT,
    STAGE_FLOW_LEAVE,
    STAGE_FLOW_DONE,
} STAGE_FLOW_PHASE;

typedef struct {
    STAGE_FLOW_PHASE phase;
    bool enemy_updates;
    bool after_tile_effect;
} STAGE_FLOW;

static STAGE_FLOW m_Flow;

static void M_Black(void)
{
    if (g_Stage->index == 2) {
        Palette_SetColor(0, 0, 0, 0);
        Palette_Show();
    }
}

void StageFlow_Begin(const uint8_t stage, const bool reduce_effects)
{
    ASSERT(stage < 6);
    memset(&m_Flow, 0, sizeof(m_Flow));
    m_Flow.enemy_updates = true;
    m_Flow.after_tile_effect = stage < 3 || (stage == 3 && !reduce_effects);
    static const int16_t midboss_rows[] = { 116, 80, 103, 944, -1, 200 };
    Stage_SetMidbossRow(midboss_rows[stage]);
    Stage_SetProgression(0);
    p_Stage->scroll.speed = stage == 3 ? 2 : 1;
    p_Stage->scroll.interval = stage == 3 || stage == 4 ? 1
        : stage == 5                                    ? 2
                                                        : 4;
}

STAGE_FLOW_RESULT StageFlow_Enter(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, const TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    if (m_Flow.phase == STAGE_FLOW_SCROLL && g_Stage->scroll.done == 1) {
        Stage_SetProgression(1);
        if (g_Stage->index == 3) {
            m_Flow.after_tile_effect = false;
        }
        M_Black();
        m_Flow.phase = STAGE_FLOW_ENTER;
    }
    if (m_Flow.phase != STAGE_FLOW_ENTER) {
        return STAGE_FLOW_RUNNING;
    }
    if (!BossFight_Begin(patterns, assets, music, session, input)) {
        return STAGE_FLOW_WAIT;
    }
    Enemies_RemoveAll();
    m_Flow.enemy_updates = false;
    p_Stage->scroll.cycle = 255;
    m_Flow.phase = STAGE_FLOW_FIGHT;
    return STAGE_FLOW_BOSS_STARTED;
}

void StageFlow_Invalidate(const uint8_t stage, const bool reduce_effects)
{
    if (m_Flow.phase == STAGE_FLOW_FIGHT) {
        BossFight_Invalidate(stage, reduce_effects);
    }
}

void StageFlow_InvalidateEnemies(void)
{
    if (m_Flow.enemy_updates) {
        Enemies_Invalidate(StageScroll_Pages());
    }
}

void StageFlow_UpdateEffects(void)
{
    if (m_Flow.after_tile_effect) {
        StageEffects_Update(
            StageScroll_Pages(), g_Stage->index, g_Stage->reduce_effects,
            &p_Stage->world.slowdown);
    }
}

void StageFlow_UpdateEnemies(
    const STAGE_DATA *const data, const PATTERN_BANK *const patterns,
    const uint8_t rank)
{
    if (m_Flow.enemy_updates) {
        Enemies_Update(
            StageScroll_Pages(), g_Stage->scroll.moved, data, patterns,
            g_Stage->world.frame, rank, 1);
    }
}

STAGE_FLOW_RESULT StageFlow_Update(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    if (m_Flow.phase != STAGE_FLOW_FIGHT) {
        Stage_SetProgression(0);
        return STAGE_FLOW_RUNNING;
    }
    const BOSS_RESULT result =
        BossFight_Update(patterns, assets, music, session, input);
    if (result == BOSS_WAIT) {
        return STAGE_FLOW_WAIT;
    }
    Stage_SetProgression((uint8_t)result);
    return STAGE_FLOW_RUNNING;
}

STAGE_FLOW_RESULT StageFlow_End(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, const TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    if (m_Flow.phase == STAGE_FLOW_FIGHT && g_Stage->world.progression == 2) {
        M_Black();
        m_Flow.phase = STAGE_FLOW_LEAVE;
    }
    if (m_Flow.phase == STAGE_FLOW_LEAVE) {
        if (!BossFight_End(patterns, assets, music, session, input)) {
            return STAGE_FLOW_WAIT;
        }
        m_Flow.phase = STAGE_FLOW_DONE;
    }
    return m_Flow.phase == STAGE_FLOW_DONE ? STAGE_FLOW_CLEARED
                                           : STAGE_FLOW_RUNNING;
}
