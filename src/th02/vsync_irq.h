#pragma once

// The counters that the original's VSync interrupt handler increments and
// the game reads for its frame waits and timing, plus a serial of display
// edges.

#include "pc98/vsync.h"

typedef struct {
    uint16_t count1, count2;
    uint64_t edges;
} VSYNC_COUNTERS;

extern VSYNC_COUNTERS g_VSync;

void VSyncIrq_BeginApplication(VSYNC_CLOCK *clock);

// Clears the program's counters, keeping the edge serial.
void VSyncIrq_BeginProgram(void);
void VSyncIrq_VSync(void);
