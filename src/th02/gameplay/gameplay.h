#pragma once

#include "th02/bomb/assets.h"
#include "th02/boss/fight.h"
#include "th02/demo.h"
#include "th02/gameplay/assets.h"
#include "th02/music/wait.h"
#include "th02/stage/assets.h"

typedef enum {
    GAMEPLAY_RUNNING,
    GAMEPLAY_CLEAR,
    GAMEPLAY_GAME_OVER,
    GAMEPLAY_DEMO_END,
    GAMEPLAY_ENDING,
} GAMEPLAY_RESULT;

typedef struct {
    const MUSIC_WAIT *entry;
    void (*stage)(void *context);
    BOSS_MUSIC bosses;
} GAMEPLAY_MUSIC;

// Starts a run; keep session and assets alive until done. Without the
// bomb overread fix, a complete 72-byte tail is required.
void Gameplay_Begin(
    TH02_SESSION *session, const GAMEPLAY_ASSETS *assets,
    const GAMEPLAY_MUSIC *music, const DEMO_RECORD *demo,
    int32_t highest_record, bool fix_bomb_transition_overread,
    const uint8_t *bomb_transition_tail);

// Begins the next stage, keeping run state, weapon cadence and RNG cursor;
// after a clear, the score delta must be committed.
void Gameplay_BeginStage(
    const STAGE_ASSETS *stage, const PATTERN_BANK *patterns,
    const BOMB_ASSETS *bomb);

// Ends the continue screen; session.score is in registration units.
void Gameplay_EndCredit(bool resume);

// Runs one frame after VSyncIrq_VSync. ENDING follows the final boss's
// dialogue; Extra returns CLEAR and shows its ending after the bonus.
GAMEPLAY_RESULT Gameplay_Update(const TH02_INPUT *input);

// Drops the borrowed session and assets.
void Gameplay_Shutdown(void);
