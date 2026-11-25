#pragma once

#include "th01/vsync_irq.h"

typedef enum {
    WAIT_IDLE,
    WAIT_DELAY,
    WAIT_SCROLL,
    WAIT_TICKS,
} WAIT_KIND;

// One suspended wait of the original's blocking code. Zero is idle.
typedef struct {
    WAIT_KIND kind;
    VSYNC_WAIT edge;
    uint16_t frames;
    int16_t scroll;
    uint64_t since;
    uint8_t ticks;
} WAIT;

// Each returns whether the wait is already over, and otherwise arms it.
// Polls once at once, as VSyncIrq_DelayReady.
bool Wait_Delay(WAIT *wait, uint16_t frames);

// Sets the scroll line at the next VSync edge.
bool Wait_Scroll(WAIT *wait, int16_t line);

// Waits ticks 60 Hz host ticks from now.
bool Wait_Ticks(WAIT *wait, uint64_t now, uint8_t ticks);

// Polls an armed wait and is idle again once it is over. An idle wait is
// over.
bool Wait_Ready(WAIT *wait, uint64_t now);
bool Wait_Idle(const WAIT *wait);
