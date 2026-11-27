#pragma once

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

// Gives a range of a module's code.
typedef struct {
    uintptr_t start;
    size_t size;
} CLOCK_RANGE;

// Puts module's timeGetTime and QueryPerformanceCounter on a frame clock
// that starts at 0 and moves on only by Clock_Advance, one frame of
// frame_rate rounded up to the millisecond. A read from one of the count
// pacing ranges moves a clock of its own on by a whole frame instead, so
// a timed wait there ends at its first check, and the game asks Windows
// for messages once rather than once per millisecond; Clock_Advance sets
// it to the frame clock. PeekMessageA asks Windows again only in the next
// frame once the queue ran empty.
// QueryPerformanceFrequency reports 1,000,000 per second. With real_time,
// Clock_Advance also waits until the frame clock's time since the first
// frame has passed in real time, so that a person plays at frame_rate. Returns
// whether module imports either clock.
bool Clock_Install(
    HMODULE module, double frame_rate, const CLOCK_RANGE *pacing, size_t count,
    bool real_time);

// Moves the frame clock on by one frame.
void Clock_Advance(void);
