#pragma once

#include "th02/boss/assets.h"
#include "th02/boss/talk.h"

// Returns the live boss or midboss state.
const BOSS_STATE *BossFight_State(void);

// Initializes the boss state and every landed boss, once per MAIN process.
void BossFight_Init(void);

// Clears the damage, phase and defeat state at stage entry.
void BossFight_BeginStage(void);

// Starts the stage's boss; keep the banks and music bound until done.
// Returns true at once when the stage's boss hasn't landed yet.
bool BossFight_Begin(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    const TH02_SESSION *session, const TH02_INPUT *input);

// Invalidates the background under the stage's boss.
void BossFight_Invalidate(uint8_t stage, bool reduce_effects);

// Runs one boss frame; WAIT suspends the stage until the boss resumes.
// Finishes at once when the stage's boss hasn't landed yet.
BOSS_RESULT BossFight_Update(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    TH02_SESSION *session, const TH02_INPUT *input);

// Runs the post-fight dialogue, if any, without awarding again; returns
// true when done.
bool BossFight_End(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    const TH02_SESSION *session, const TH02_INPUT *input);

// Reports whether the stage has a landed midboss.
bool BossFight_HasMidboss(uint8_t stage);

// Invalidates the background under the midboss; returns whether it is
// active.
bool BossFight_MidbossInvalidate(uint8_t stage);

// Runs one midboss frame.
void BossFight_MidbossUpdate(
    const PATTERN_BANK *patterns, uint8_t stage, uint8_t rank);
