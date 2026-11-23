#pragma once

#include "th01/frame.h"
#include "th01/hud.h"
#include "th01/pellets.h"
#include "th01/tram.h"

typedef enum {
    TIMER_IDLE,
    TIMER_WAIT_HURRY,
    TIMER_WAIT_UP,
} TIMER_PHASE;

// Holds the stage timer. StageTimer_Init zeroes it once per main scene:
// hurry_cycle carries over between stages.
typedef struct {
    uint16_t time, frames_since_hurry;
    uint8_t hurry_cycle;
    TIMER_PHASE phase;
} STAGE_TIMER;

extern STAGE_TIMER g_StageTimer;

typedef struct {
    const HUD_CONTEXT *display;
} TIMER_CONTEXT;

void StageTimer_Init(void);

// Redraws the HUD in the original's order. While timer_initialized is
// false, first sets the time for the current stage.
void StageTimer_Redraw(
    const TIMER_CONTEXT *context, bool first_stage, bool timer_initialized,
    int16_t route);

// Counts the time down by two, wrapping in unsigned 16 bits. At zero, raises
// the pellet speed, plays effect 17 through effect and shows HARRY, then UP,
// each for 27 frames; count1 may end the first delay at once. Once the time
// is out, rains pellets, drawing a random X even when the pool is full.
// Returns false while a delay is pending: call again after the next VSync.
bool StageTimer_TickAndPut(const TIMER_CONTEXT *context);

// Adds 200 to the time, or 1000 once it ran out.
void StageTimer_ExtendAndPut(const TIMER_CONTEXT *context);
