#pragma once

#include "th02/boss.h"
#include "th02/boss/assets.h"
#include "th02/input.h"
#include "th02/session.h"

// One stage's talk hooks: the common flow fades out, lets the boss place
// itself and draw its reveal, fades in and continues the script; a boss
// whose dialogue doesn't fit that shape sets begin/update instead, taking
// over the whole flow. Every hook is optional except where noted.
typedef struct {
    // One-time setup before the first box of the pre-fight dialogue runs
    // (Meira forces her entrance sprite here).
    void (*prepare)(BOSS_STATE *boss);
    // Overrides the common flow below; begin and update are set together.
    void (*begin)(
        BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
        const BOSS_MUSIC *music, const TH02_SESSION *session, bool post);
    bool (*update)(
        BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
        const BOSS_MUSIC *music, const TH02_SESSION *session, bool post,
        const TH02_INPUT *input);
    // Common flow: required unless begin/update override it.
    void (*place)(BOSS_STATE *boss);
    void (*draw_scene)(const BOSS_STATE *boss, const PATTERN_BANK *patterns);
    // Swaps in a boss's own combat bank before its reveal (Sigma, from the
    // extra background phase below); bank is the boss's own bank selector,
    // meaningless to a boss that ignores it.
    void (*select_patterns)(
        PATTERN_BANK *patterns, const BOSS_ASSETS *assets, int bank);
    // Disables the tile background and selects patterns before entering
    // (Sigma's bare white room), instead of the ordinary intro box.
    bool extra_background;
    bool music_before;
    bool music_after;
    // Runs the boss's own post-defeat form change, when it has one (Mima);
    // reached through BossStrategy_Get(stage)->talk.transition by the
    // boss's own update, once it lands.
    BOSS_RESULT (*transition)(
        BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
        const BOSS_MUSIC *music, const TH02_SESSION *session,
        const TH02_INPUT *input);
} BOSS_TALK_HOOKS;

// Clears dialogue cursors and entrance state at stage entry.
void BossTalk_BeginStage(void);

// Prepares the dialogue with a one-VSync wait; returns true when ready.
bool BossTalk_Prepare(const PATTERN_BANK *patterns);

// Runs the stage's pre- or post-fight dialogue through its strategy's talk
// hooks; returns true when done. Skips the dialogue when its stage text or
// strategy is missing (no boss landed on that stage yet).
bool BossTalk_Update(
    BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
    const BOSS_MUSIC *music, const TH02_SESSION *session, bool post,
    const TH02_INPUT *input);
