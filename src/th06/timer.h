// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <stdint.h>

// Counts frames. `previous` holds the count before the last tick or set,
// so a check can tell whether the count changed in between. Adding to or
// subtracting from `current` directly leaves `previous` unchanged, as the
// original does at full speed.
typedef struct {
    int32_t previous;
    int32_t current;
} TIMER;

// Starts counting from 0, with no previous count (-1).
void Timer_Init(TIMER *timer);

// Sets the count to frames, with no previous count (-999).
void Timer_Set(TIMER *timer, int32_t frames);

// Advances by one frame and keeps the old count in `previous`.
void Timer_Tick(TIMER *timer);

// Returns whether the count changed since the last tick or set.
bool Timer_HasTicked(const TIMER *timer);
