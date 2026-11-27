#pragma once

#include "th02/boss/mima/assets.h"
#include "th02/boss/talk.h"
#include "th02/boss.h"

// Shared between Mima's combat (logic.c) and her form-change script
// (transition.c); nothing outside the mima/ directory sees these.

typedef enum {
    MIMA_TRANSITION_WAIT,
    MIMA_TRANSITION_FORM,
    MIMA_TRANSITION_POST,
    MIMA_TRANSITION_ENDED,
} MIMA_TRANSITION;

// Copies Mima's boss slots into patterns and imports their palette without
// showing it; missing banks keep the current patterns.
void Mima_SelectPatterns(
    PATTERN_BANK *patterns, const MIMA_ASSETS *assets, MIMA_BANK bank);

// Places Mima at (144,64) with sprite 128.
void Mima_Place(BOSS_STATE *boss);

// Draws Mima's body, flashing green on every fourth pending hit_flash.
void Mima_DrawBody(BOSS_STATE *boss, const PATTERN_BANK *patterns);

// Starts a form change once; returns the script to run, WAIT while it runs
// and ENDED after the ending handoff.
MIMA_TRANSITION Mima_BeginTransition(
    BOSS_STATE *boss, const PATTERN_BANK *patterns);

// Enters the final form only without continues; otherwise awards the ending
// score and returns BOSS_FINISHED.
BOSS_RESULT Mima_EndTransition(BOSS_STATE *boss, bool continued);

// The stage 5 pre-fight dialogue: reveals Mima through a summon and her
// first form.
void Mima_TalkBegin(
    BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
    const BOSS_MUSIC *music, const TH02_SESSION *session, bool post);
bool Mima_TalkUpdate(
    BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
    const BOSS_MUSIC *music, const TH02_SESSION *session, bool post,
    const TH02_INPUT *input);

// Runs Mima's form change; RUNNING when combat resumes, FINISHED at the
// ending.
BOSS_RESULT Mima_Transition(
    BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
    const BOSS_MUSIC *music, const TH02_SESSION *session,
    const TH02_INPUT *input);
