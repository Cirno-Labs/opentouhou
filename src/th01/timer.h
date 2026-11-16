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

// Zero-initialize once per main scene. InitFor accepts stage_id 0..19 and
// route 0 (Makai) or 1 (Jigoku); it resets time and frames_since_hurry only.
// Preserve hurry_cycle between stages and timer extensions. Never reinitialize
// or extend while a cooperative operation is pending.
void Timer_InitFor(STAGE_TIMER *timer, int16_t stage_id, int16_t route);

// Implements the complete HUD redraw order. timer_initialized is borrowed
// world state: a false value initializes time but remains unchanged. The
// gameplay owner sets it only after its real wait-for-shot boundary.
void Timer_Redraw(
    STAGE_TIMER *timer, const TIMER_CONTEXT *context, bool first_stage,
    bool timer_initialized, int16_t route);

// Executes one source timer call, subtracting two in unsigned 16 bits.
// The world calls this once per (frame_rand & 3)==0, then suspends the world
// until true. A false result requires another call after advancing the real
// Frame VSync; no prefix side effect is replayed. count1 may satisfy the
// first 27-frame delay immediately. Uses real Pellets operations and shared
// RNG, consuming random X even if the pool is full. The required synchronous
// effect callback must post effect 17 to the scene's audio owner. It must not
// change borrowed timer/world state. The gameplay scene remains external.
// All operations use loaded glyphs and caller storage, and allocate nothing.
bool Timer_TickAndPut(STAGE_TIMER *timer, const TIMER_CONTEXT *context);
void Timer_ExtendAndPut(STAGE_TIMER *timer, const TIMER_CONTEXT *context);
