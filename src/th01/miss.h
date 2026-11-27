#pragma once

#include "th01/player.h"
#include "th01/vsync_irq.h"

// Clears all state for a new gameplay session.
void Miss_Init(void);

// Validates all images used by the sequence before ticks. Assets belong to
// the caller's grouped lifetime and remain immutable throughout the miss.
RESULT Miss_ValidateSprites(const PTN *miko, const PTN *stage);

// Starts the miss animation; lives must already be decremented.
// Suspend ordinary actor updates until Miss_Resume returns true.
void Miss_Begin(const PTN *miko, const PTN *stage);

// Resumes the miss animation; returns true when done. The caller then clears
// player.is_hit and sets invincibility_time to 150.
bool Miss_Resume(const PTN *miko, const PTN *stage, int16_t extra_bombs);
