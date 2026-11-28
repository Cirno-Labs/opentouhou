#pragma once

#include "th01/opening/opening.h"
#include "th01/palette.h"
#include "th01/settings.h"

typedef struct {
    PALETTE_STATE palette;
    PALETTE_FADE fade;
    MENU_INPUT input;
    OPENING_ACTION exit_action;
    int32_t line_count;
    int32_t line_y[2];
    int32_t prompt_frame;
    int32_t selection;
    int32_t option_item_count;
    int32_t music_mode;
    int32_t wait;
    bool drawn_rows[VIDEO_HEIGHT];
    bool entering;
    bool left_locked[2];
    bool right_locked[2];
    bool confirm_locked;
} OPENING_MENU;

// Writable views of the opening screen state for tests.
extern OPENING *const p_Opening;
extern OPENING_MENU *const p_Menu;
