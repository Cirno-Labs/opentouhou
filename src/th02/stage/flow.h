#pragma once

#include "th02/boss/fight.h"
#include "th02/stage/data.h"

typedef enum {
    STAGE_FLOW_WAIT,
    STAGE_FLOW_RUNNING,
    STAGE_FLOW_CLEARED,
    STAGE_FLOW_BOSS_STARTED,
} STAGE_FLOW_RESULT;

// The stage's boss fight calls, set by StageFlow_Init: the real
// BossFight_* functions by default, a fake observer in tests.
typedef struct {
    bool (*begin)(
        PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
        const BOSS_MUSIC *music, const TH02_SESSION *session,
        const TH02_INPUT *input);
    void (*invalidate)(uint8_t stage, bool reduce_effects);
    BOSS_RESULT (*update)(
        PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
        const BOSS_MUSIC *music, TH02_SESSION *session,
        const TH02_INPUT *input);
    bool (*end)(
        PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
        const BOSS_MUSIC *music, const TH02_SESSION *session,
        const TH02_INPUT *input);
} STAGE_FLOW_BOSS;

extern const STAGE_FLOW_BOSS g_StageFlowBoss;

// Swaps the boss fight hooks; defaults to g_StageFlowBoss, so only tests
// need to call this.
void StageFlow_Init(const STAGE_FLOW_BOSS *boss);

// Sets the scroll rules and clears progression at stage entry.
void StageFlow_Begin(uint8_t stage, bool reduce_effects);

// Enters the boss after the page swap; returns BOSS_STARTED once when the
// entrance completes.
STAGE_FLOW_RESULT StageFlow_Enter(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    const TH02_SESSION *session, const TH02_INPUT *input);

void StageFlow_Invalidate(uint8_t stage, bool reduce_effects);

// Invalidates enemies while stage scrolling or entrance dialogue is active.
void StageFlow_InvalidateEnemies(void);

// Updates the stage effects at their selected position before tile drawing.
void StageFlow_UpdateEffects(void);

// Updates enemies while stage scrolling or entrance dialogue is active.
void StageFlow_UpdateEnemies(
    const STAGE_DATA *data, const PATTERN_BANK *patterns, uint8_t rank);

// Runs one boss frame during the fight, setting progression from its
// result; WAIT suspends the stage.
STAGE_FLOW_RESULT StageFlow_Update(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    TH02_SESSION *session, const TH02_INPUT *input);

// Runs the defeat dialogue once, before the stage bonus.
STAGE_FLOW_RESULT StageFlow_End(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    const TH02_SESSION *session, const TH02_INPUT *input);
