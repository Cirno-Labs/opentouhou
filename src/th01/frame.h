#pragma once

#include "core/base/types.h"

typedef struct {
    uint32_t phase;
} FRAME_CLOCK;

// Advances one 60 Hz host tick and returns whether a reference VSync is due.
// Uses the 24 kHz mode's 56.423134648 Hz cadence. Zero initialization starts
// at the beginning of a period, so the first VSync is due on host tick two.
// Each VSync is delayed by less than one host tick; the integer phase keeps
// the long-term rate exact. Keep this clock across scene changes, and age
// gameplay counters only where the original gameplay loop advances them.
bool Frame_Advance(FRAME_CLOCK *clock);

// Keeps program-local signed interrupt counters and a logical edge serial.
// Advance exactly once per reference VSync, including edges inside nested
// operations. Zero initialization starts both source counters at zero.
typedef struct {
    int16_t count1, count2;
    uint64_t edge;
} FRAME_VSYNC;

typedef struct {
    uint64_t edge;
} FRAME_WAIT;

void Frame_VSync(FRAME_VSYNC *vsync);

// Advances only the edge serial, for a VSync while the program has masked
// IRQ 2 (vsync_exit): the counters stop, and polls of the GDC still see it.
void Frame_PollVSync(FRAME_VSYNC *vsync);

// Starts a new program's counters at one, keeping the edge serial. The old
// program's vsync_exit masks IRQ 2 while its armed CRT interrupt is still due;
// the 8259 latches it, and the new program's vsync_init delivers it at once.
void Frame_SwitchProgram(FRAME_VSYNC *vsync);

// Captures the current edge. Ready requires a subsequent edge, regardless
// of counter values, and never resets either interrupt counter.
void Frame_BeginWait(FRAME_WAIT *wait, const FRAME_VSYNC *vsync);
bool Frame_WaitReady(const FRAME_WAIT *wait, const FRAME_VSYNC *vsync);

// Compares count1 as unsigned 16 bits. Resets count1 only on success;
// frames == 0 succeeds immediately. Preserves count2 and the edge serial.
bool Frame_DelayReady(FRAME_VSYNC *vsync, uint16_t frames);
