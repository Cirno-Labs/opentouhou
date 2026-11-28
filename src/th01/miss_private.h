#pragma once

#include "th01/miss.h"
#include "th01/vsync_irq.h"

typedef enum {
    MISS_DONE,
    MISS_SHAKE_WAIT,
    MISS_SHAKE_DELAY,
    MISS_SCROLL_RESET_WAIT,
    MISS_EFFECT_DELAY,
} MISS_PHASE;

typedef struct {
    MISS_PHASE phase;
    VSYNC_WAIT wait;
    int16_t frame;
    int16_t effect_left;
    int16_t target_right;
    int16_t target_left;
} MISS;

// Writable view of the miss sequence for tests.
extern MISS *const p_Miss;
