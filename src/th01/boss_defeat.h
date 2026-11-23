#pragma once

#include "th01/frame.h"
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
    FRAME_WAIT wait;
    int16_t top, bottom, frame;
} BOSS_DEFEAT;

typedef struct {
    PALETTE_STATE *palette;
    const PALETTE *post_defeat;
    const PTN *miko;
    const int16_t *origin_top;
    bool *stage_cleared;
} BOSS_DEFEAT_CONTEXT;

RESULT BossDefeat_ValidateSprites(const PTN *miko);

// Starts with a fresh raw VSync wait, without drawing or resetting counters.
// Zero-initialize once and begin only when done. Keep borrowed storage alive
// and supply the same context bindings until completion. origin_top is the live
// Orb top for regular bosses or SinGyoku entity top; its center (+16 or +48)
// must be in 0..399. Assets must be validated and post_defeat components must
// stay in 0..15.
void BossDefeat_Begin(BOSS_DEFEAT *defeat, BOSS_DEFEAT_KIND kind);

// Resumes white-line expansion, whiteout, background/player restoration and
// fade to the live post-defeat palette. Retains the final extra fade interval
// and source count1 reuse. Repeated polling before readiness is inert.
// Regular completion sets stage_cleared and player.is_hit. SinGyoku leaves
// them unchanged: start the route chooser immediately in the same tick and
// set them only after its cached scene activation. Does not advance VSyncs,
// actors or gameplay counters. Allocates nothing and performs no IO.
bool BossDefeat_Resume(BOSS_DEFEAT *defeat, const BOSS_DEFEAT_CONTEXT *context);
