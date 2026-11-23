#pragma once

#include "th01/interleave.h"
#include "th01/palette.h"

typedef struct {
    uint64_t last_tick;
    uint32_t credit;
    uint16_t step_ms, frame;
    int16_t first_left, first_top, second_left, second_top;
    bool active;
} PAGE_TRANSITION;

typedef struct {
    PALETTE_STATE *palette;
    PALETTE *current;
    const PALETTE *target;
    const INTERLEAVE_ASSETS *assets;
} TRANSITION_CONTEXT;

// Starts both diagonal fronts and draws the first step immediately. Keep
// context and its borrowed objects alive and identically bound until done;
// target colors and page 1 remain live. Set current and target to
// palette->saved for the no-fade form. Zero-initialize once and begin only
// when inactive. Returns true after all 130 steps and their final delay.
// A zero delay completes synchronously; a nonzero delay suspends after the
// first step. Moves signed current components toward target every eight
// steps, then clamps and copies current into saved and the displayed DAC.
bool Transition_BeginDiagonal(
    PAGE_TRANSITION *transition, const TRANSITION_CONTEXT *context,
    uint16_t step_ms, uint64_t host_tick);

// Resumes using a monotonic serial of fixed 60 Hz HOST ticks, independent
// of reference VSyncs. May catch up several steps. Carries fractional delay
// credit, so 130 delays of 40 ms finish after 312 host ticks (5.2 seconds),
// rather than rounding each delay upward. Polling the same serial is inert.
// Quantizes visibility to host ticks; does not model CPU time spent drawing
// or DOS busy-wait calibration. Never reads a clock or changes VSync state.
// Operations use borrowed storage, allocate nothing and perform no IO.
bool Transition_Advance(
    PAGE_TRANSITION *transition, const TRANSITION_CONTEXT *context,
    uint64_t host_tick);
