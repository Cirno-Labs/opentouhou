#pragma once

// The clip rectangle that drawing stays inside, in inclusive screen
// pixels. Most draws clip only vertically or not at all, as in the original;
// each function's doc says which.

#include "core/base/result.h"
#include "core/base/types.h"

typedef struct {
    int16_t left, top, right, bottom;
} CLIP;

extern const CLIP *const g_Clip;

// Initializes inclusive screen bounds (0,0)..(639,399).
void Clip_Init(CLIP *clip);

// Sets the bounds to a rectangle within the screen; failure on Y still
// sets X, as in retail.
RESULT Clip_Set(
    CLIP *clip, int16_t left, int16_t top, int16_t right, int16_t bottom);

// Resets g_Clip to the full screen.
void Clip_Reset(void);

// Sets g_Clip to the stage playfield bounds (32,0)..(416,399).
void Clip_SetPlayfield(void);

// Sets g_Clip to the side panel, (416,0)..(639,399).
void Clip_SetSidePanel(void);

// Sets g_Clip to the boss summon circle's area, (32,16)..(416,320).
void Clip_SetSummonArea(void);

// Sets g_Clip to the score view's table box, (128,96)..(512,304).
void Clip_SetScoreTable(void);
