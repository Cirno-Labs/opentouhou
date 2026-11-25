#pragma once

#include "th01/vsync_irq.h"
#include "th01/palette.h"
#include "th01/player.h"

typedef enum {
    BOSS_DEFEAT_REGULAR,
    BOSS_DEFEAT_SINGYOKU,
} BOSS_DEFEAT_KIND;

typedef enum {
    BOSS_DEFEAT_DONE,
    BOSS_DEFEAT_SCROLL_WAIT,
    BOSS_DEFEAT_WHITEOUT,
    BOSS_DEFEAT_WHITEOUT_DELAY,
    BOSS_DEFEAT_WHITEIN,
    BOSS_DEFEAT_WHITEIN_DELAY,
} BOSS_DEFEAT_PHASE;

typedef struct {
    BOSS_DEFEAT_PHASE phase;
    BOSS_DEFEAT_KIND kind;
    VSYNC_WAIT wait;
    int16_t top, bottom, frame;
} BOSS_DEFEAT;

RESULT BossDefeat_ValidateSprites(const PTN *miko);

// Starts the defeat sequence; begin only when the previous one is done.
// Keep the bound storage alive and the bindings unchanged until completion.
void BossDefeat_Begin(BOSS_DEFEAT *defeat, BOSS_DEFEAT_KIND kind);

// Resumes the defeat sequence; returns true when done. Regular completion
// sets stage_cleared and player.is_hit; SinGyoku leaves them to the caller,
// which sets them after the route chooser's scene starts.
bool BossDefeat_Resume(
    BOSS_DEFEAT *defeat, const PALETTE *post_defeat, const PTN *miko,
    const int16_t *origin_top);
