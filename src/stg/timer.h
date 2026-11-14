#pragma once

#include <stdint.h>

// Counts frames, and fractions of a frame while the game runs slow.
// `previous` holds the count before the last tick or set, so a check can
// tell whether the count changed in between. Adding to or subtracting
// from `current` directly leaves `previous` unchanged, as the original
// does at full speed. th06 never runs slow and leaves `sub_frame` at 0.
typedef struct {
    int32_t previous;
    int32_t current;
    float sub_frame;
} TIMER;

// Starts counting from 0, with no previous count (-1) and no fraction.
void Timer_Init(TIMER *timer);

// Sets the count to frames, with no previous count (-999) and no
// fraction.
void Timer_Set(TIMER *timer, int32_t frames);

// Advances by one frame and keeps the old count in `previous`.
void Timer_Tick(TIMER *timer);

// Returns whether the count changed since the last tick or set.
bool Timer_HasTicked(const TIMER *timer);

// Advances by one tick at a game speed, where 1 is full speed, and keeps
// the old count in `previous`. Over 0.99, the count goes up by one frame.
// At 0.99 or less, the fraction goes up by speed and gives at most one
// whole frame to the count.
void Timer_TickAt(TIMER *timer, float speed);

// Adds frames at a game speed. Over 0.99, only `current` changes, as
// with a direct addition. At 0.99 or less, the old count goes to
// `previous` and the fraction goes up by speed times frames, giving every
// whole frame to the count; negative frames go to Timer_SubAt.
void Timer_AddAt(TIMER *timer, int32_t frames, float speed);

// Subtracts frames at a game speed, as Timer_AddAt in reverse: the
// fraction goes down and takes a frame from the count while it is below
// 0; negative frames go to Timer_AddAt.
void Timer_SubAt(TIMER *timer, int32_t frames, float speed);

// Returns the count with its fraction.
float Timer_AsFloat(const TIMER *timer);
