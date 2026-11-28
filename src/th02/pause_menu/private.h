#pragma once

// logic.c's own state, exposed for tests; nothing outside the
// pause_menu/ directory sees this.

#include "th02/pause_menu/logic.h"

typedef enum {
    PAUSE_MENU_SHOW,
    PAUSE_MENU_READ,
    PAUSE_MENU_DONE,
} PAUSE_MENU_PHASE;

typedef struct {
    const PAUSE_MENU_ASSETS *assets;
    uint16_t frame;
    uint16_t keys;
    uint8_t state;
    uint8_t selected;
    uint8_t first_color;
    uint8_t second_color;
    PAUSE_MENU_PHASE phase;
    PAUSE_MENU_RESULT result;
} PAUSE;

// A writable view of logic.c's private state, for tests.
extern PAUSE *const p_Pause;
