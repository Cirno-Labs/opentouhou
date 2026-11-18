#pragma once

#include <windows.h>

#include <stdint.h>

// Makes module's timeGetTime and QueryPerformanceCounter count from 0, on
// one clock. With step 0 they follow real time; otherwise each call moves
// the clock step ms on. QueryPerformanceFrequency reports 1,000,000 per
// second. Returns whether module imports either.
bool Clock_Install(HMODULE module, uint32_t step);

// Continues the current clock with fixed reads within each frame.
// Call after Install, then AdvanceFrame once before each draw/calc cycle's
// pacing wait. Milliseconds round upward so retail's wait can finish.
void Clock_UseFrames(void);

// Advances the shared clock by one 60 Hz frame.
void Clock_AdvanceFrame(void);

typedef struct {
    uintptr_t start;
    size_t size;
} CLOCK_WAIT_RANGE;

// Lets reads from these ranges advance a separate clock for retail's
// timed waits. Other code keeps frame time. Keeps ranges until process
// exit; a zero count disables them.
void Clock_SetPacingRanges(const CLOCK_WAIT_RANGE *ranges, size_t count);
