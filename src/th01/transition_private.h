#pragma once

#include "th01/transition.h"

typedef struct {
    uint64_t last_tick;
    uint32_t credit;
    uint16_t step_ms;
    uint16_t frame;
    int16_t first_left;
    int16_t first_top;
    int16_t second_left;
    int16_t second_top;
    bool active;
} PAGE_TRANSITION;

// Writable view of the diagonal page transition for tests.
extern PAGE_TRANSITION *const p_Transition;
