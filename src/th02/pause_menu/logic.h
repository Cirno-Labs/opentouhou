#pragma once

// The pause menu: Resume or Quit, with a confirmation prompt on Quit.

#include "th02/input.h"
#include "th02/palette.h"
#include "th02/pause_menu/render.h"

typedef enum {
    PAUSE_MENU_INPUT,
    PAUSE_MENU_FRAME,
    PAUSE_MENU_RESUME,
    PAUSE_MENU_QUIT,
} PAUSE_MENU_RESULT;

typedef struct {
    PAUSE_MENU_RESULT result;
    uint16_t keys;
} PAUSE_MENU_EVENT;

// Clears the pause state, once per MAIN run. render draws the screen:
// the real functions in production, a fake observer in tests.
void PauseMenu_Init(const PAUSE_MENU_RENDER *render);

// Opens the pause menu, keeping the selection and color bytes; null assets
// draw nothing.
void PauseMenu_Begin(const PAUSE_MENU_ASSETS *assets, uint16_t keys);

// Runs one frame for keys, read only after INPUT; confirmation wins over
// cancel.
PAUSE_MENU_EVENT PauseMenu_Update(uint16_t keys);
