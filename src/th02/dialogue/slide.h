#pragma once

#include "th02/draw.h"

typedef enum {
    DIALOGUE_SLIDE_FRAME,
    DIALOGUE_SLIDE_DONE,
} DIALOGUE_SLIDE_RESULT;

typedef struct {
    const PATTERN_BANK *patterns;
    int16_t left;
    int16_t top;
    int16_t bottom;
    bool leave;
    bool pending;
    bool done;
    bool restore_disabled;
} DIALOGUE_SLIDE;

// Starts the box's slide in, granting the pending score delta without
// clearing it; disabled tiles draw black until the slide ends.
void DialogueSlide_BeginEnter(
    DIALOGUE_SLIDE *slide, const PATTERN_BANK *patterns);

// Starts the box's slide out, keeping tone, score and the pending tile
// restoration.
void DialogueSlide_BeginLeave(
    DIALOGUE_SLIDE *slide, const PATTERN_BANK *patterns);

// Runs one slide frame, 23 in or 24 out, drawing both type-A options
// whatever the shot and power.
DIALOGUE_SLIDE_RESULT DialogueSlide_Update(DIALOGUE_SLIDE *slide);
