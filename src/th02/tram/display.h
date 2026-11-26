#pragma once

// The text layer's display: the blink cycle that hides and shows it, kept
// in step with the physical VSync.

#include "core/base/types.h"

typedef struct {
    uint16_t first_cell;
    uint8_t blink_frame;
    bool visible;
} TRAM_DISPLAY;

// Starts the blink cycle of 128 edges, 32 hidden and 96 visible.
void TramDisplay_Init(TRAM_DISPLAY *display, uint8_t blink_frame);

// Advances the blink on each physical VSync, even while text is hidden.
void TramDisplay_VSync(TRAM_DISPLAY *display);

// Samples the physical display clock.
void TramDisplay_Clock(TRAM_DISPLAY *display, uint64_t microseconds);

bool TramDisplay_BlinkVisible(const TRAM_DISPLAY *display);
