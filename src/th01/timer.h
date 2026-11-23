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

// Holds the stage timer. Zero it once per main scene: hurry_cycle carries
// over between stages.
typedef struct {
    uint16_t time, frames_since_hurry;
    uint8_t hurry_cycle;
    TIMER_PHASE phase;
} STAGE_TIMER;

typedef struct {
    HUD *hud;
    const HUD_CONTEXT *display;
    TRAM *tram;
    FRAME_VSYNC *vsync;
    PELLETS *pellets;
    PELLET_SHARED *shared;
    uint32_t *rng;
    void (*effect)(void *user, int16_t number);
    void *user;
} TIMER_CONTEXT;

// Sets the time for stage_id 0..19 and route 0 (Makai) or 1 (Jigoku), and
// resets frames_since_hurry.
void Timer_InitFor(STAGE_TIMER *timer, int16_t stage_id, int16_t route);

// Redraws the HUD in the original's order. While timer_initialized is
// false, first sets the time for the current stage.
void Timer_Redraw(
    STAGE_TIMER *timer, const TIMER_CONTEXT *context, bool first_stage,
    bool timer_initialized, int16_t route);

// Counts the time down by two, wrapping in unsigned 16 bits. At zero, raises
// the pellet speed, plays effect 17 through effect and shows HARRY, then UP,
// each for 27 frames; count1 may end the first delay at once. Once the time
// is out, rains pellets, drawing a random X even when the pool is full.
// Returns false while a delay is pending: call again after the next VSync.
bool Timer_TickAndPut(STAGE_TIMER *timer, const TIMER_CONTEXT *context);

// Adds 200 to the time, or 1000 once it ran out.
void Timer_ExtendAndPut(STAGE_TIMER *timer, const TIMER_CONTEXT *context);
