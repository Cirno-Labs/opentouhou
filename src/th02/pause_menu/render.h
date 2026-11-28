#pragma once

// Drawing for the pause menu's prompt and labels.

#include "th02/pause_menu/assets.h"

typedef enum {
    PAUSE_MENU_RESUME_TEXT,
    PAUSE_MENU_QUIT_TEXT,
    PAUSE_MENU_PROMPT,
    PAUSE_MENU_NO_TEXT,
    PAUSE_MENU_YES_TEXT,
    PAUSE_MENU_HEADING,
    PAUSE_MENU_BLANK,
} PAUSE_MENU_DRAW_KIND;

// The pause menu's draw and palette-tone calls, set by PauseMenu_Init:
// the real functions below in production, an observer in tests.
typedef struct {
    void (*draw)(
        const PAUSE_MENU_ASSETS *assets, PAUSE_MENU_DRAW_KIND kind, uint16_t x,
        uint16_t y, uint16_t attribute);
    void (*tone)(int16_t tone);
} PAUSE_MENU_RENDER;

// Draws one cached label or the built-in heading/blank gaiji.
void PauseMenuRender_Draw(
    const PAUSE_MENU_ASSETS *assets, PAUSE_MENU_DRAW_KIND kind, uint16_t x,
    uint16_t y, uint16_t attribute);

extern const PAUSE_MENU_RENDER g_PauseMenuRender;
