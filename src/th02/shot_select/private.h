#pragma once

// Shared between logic.c and render.c... logic.c's own state; nothing
// outside the shot_select/ directory sees this.

#include "th02/palette.h"
#include "th02/shot_select/logic.h"

typedef enum {
    SHOT_SELECT_READ,
    SHOT_SELECT_OLD_FIRST,
    SHOT_SELECT_OLD_SECOND,
    SHOT_SELECT_OLD_THIRD,
    SHOT_SELECT_NEW_COPY,
    SHOT_SELECT_NEW_DESCRIPTION,
    SHOT_SELECT_NEW_PICTURE,
    SHOT_SELECT_TAIL,
    SHOT_SELECT_DONE,
} SHOT_SELECT_PHASE;

typedef struct {
    TH02_SESSION *session;
    const SHOT_SELECT_ASSETS *assets;
    PALETTE_FADE fade;
    uint16_t keys;
    uint16_t input_locked;
    uint16_t input_delay;
    int8_t selection;
    SHOT_SELECT_PHASE phase;
    bool right_animation;
} SHOT_SELECT;

// Writable view of logic.c's private state, for tests.
extern SHOT_SELECT *const p_Select;
