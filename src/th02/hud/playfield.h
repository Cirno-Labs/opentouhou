#pragma once

// The playfield's text cells, left of the HUD column: the instant wipe that
// blanks them and the wipe that closes or opens them over 21 frames.

#include "th02/wait.h"

typedef struct {
    WAIT wait;
    uint8_t frame;
    uint8_t first;
    bool complete;
} HUD_WIPE;

// Blanks the playfield's text cells, keeping the HUD column.
void Hud_WipePlayfield(void);

// Starts 21 wipe frames, two VSyncs each, closing the playfield when leave
// is set and opening it otherwise; clears only the first VSync counter.
void Hud_BeginWipe(HUD_WIPE *wipe, bool leave);

// Advances the wipe; returns true when it is done.
bool Hud_UpdateWipe(HUD_WIPE *wipe);
