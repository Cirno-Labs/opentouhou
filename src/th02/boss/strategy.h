#pragma once

#include "th02/boss.h"
#include "th02/boss/assets.h"
#include "th02/boss/talk.h"
#include "th02/input.h"
#include "th02/patterns.h"
#include "th02/session.h"

// One boss's lifecycle, calling back into the shared boss state.
typedef struct {
    // Resets the boss's state, once at program start.
    void (*init)(void);
    void (*begin)(
        PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
        const TH02_SESSION *session);
    void (*invalidate)(bool reduce_effects);
    BOSS_RESULT (*update)(
        PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
        const BOSS_MUSIC *music, TH02_SESSION *session,
        const TH02_INPUT *input);
    // Whether a fight ends with the shared post-battle dialogue.
    bool post_dialogue;
    // Optional midboss operations, nullptr when the stage has none.
    bool (*midboss_invalidate)(void);
    void (*midboss_update)(const PATTERN_BANK *patterns, uint8_t rank);
    // The stage's dialogue entrance/reveal hooks; see talk.h.
    BOSS_TALK_HOOKS talk;
} BOSS_STRATEGY;

// Returns the lifecycle of the stage's boss, or nullptr when it hasn't
// landed yet; callers must handle a null result without crashing.
const BOSS_STRATEGY *BossStrategy_Get(uint8_t stage);

// Resets every landed boss module's state, once at program start.
void BossStrategy_InitAll(void);
