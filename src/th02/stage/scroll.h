#pragma once

#include "th02/entity.h"

#include "core/base/types.h"

typedef struct {
    int16_t line;
    int16_t previous;
    int16_t page_lines[2];
    uint16_t address;
    uint16_t rows;
    int16_t row_changed;
    uint8_t speed;
    uint8_t interval;
    uint8_t cycle;
    uint8_t moved;
    uint8_t done;
    uint8_t front;
    uint8_t back;
} STAGE_SCROLL;

// Clears only the per-page positions.
void StageScroll_InitPages(void);

// Saves the current line and back-page history for cleanup.
void StageScroll_Begin(void);

// Scrolls by the byte speed with signed-word wrap, adding 400 only once
// when negative. Requires a nonzero interval while enabled.
void StageScroll_Advance(void);

// Copies the new rows at the current speed, even when it differs from the
// distance moved.
void StageScroll_Finish(void);

// Shows the back page and selects the other one after the stage wait.
void StageScroll_Swap(void);

// Returns the current back, front and scroll-line pages for the
// background to scroll against.
PLAYFIELD_PAGES StageScroll_Pages(void);
