#pragma once

#include "th01/timer.h"

typedef enum {
    TIMER_IDLE,
    TIMER_WAIT_HURRY,
    TIMER_WAIT_UP,
} TIMER_PHASE;

typedef struct {
    uint16_t frames_since_hurry;
    uint8_t hurry_cycle;
    TIMER_PHASE phase;
} STAGE_TIMER;

// Writable view of the stage timer for tests.
extern STAGE_TIMER *const p_Timer;
