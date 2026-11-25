#pragma once

#include "core/base/types.h"

// Clears the stage time and hurry history for a new gameplay run.
void StageTimer_Init(void);

// Redraws the HUD in the original's order. While timer_initialized is
// false, first sets the time for the current stage.
void StageTimer_Redraw(bool first_stage, bool timer_initialized, int16_t route);

// Counts the time down by two, wrapping in unsigned 16 bits. At zero, raises
// the pellet speed, plays effect 17 and shows HARRY, then UP,
// each for 27 frames; count1 may end the first delay at once. Once the time
// is out, rains pellets, drawing a random X even when the pool is full.
// Returns false while a delay is pending: call again after the next VSync.
bool StageTimer_TickAndPut(void);

// Adds 200 to the time, or 1000 once it ran out.
void StageTimer_ExtendAndPut(void);
