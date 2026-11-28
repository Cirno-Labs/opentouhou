#pragma once

// logic.c's own state, exposed for tests; nothing outside the
// main_menu/ directory sees this.

#include "th02/main_menu/logic.h"

typedef struct {
    MENU_NAV nav;
    TH02_SESSION *session;
    TH02_SETTINGS *settings;
    const TITLE_ASSETS *assets;
    const OPTIONS_MENU_TEXT *option_text;
    OPTIONS_MENU_SOUND *sound;
    TRAM_CONSOLE *console;
    uint8_t clear_attribute;
    uint16_t idle_frame;
    uint8_t demo;
    bool initialized;
    bool input_allowed;
    bool quit;
    bool suspended;
    MAIN_MENU_ACTION pending;
} MAIN_MENU;

// A writable view of logic.c's private state, for tests.
extern MAIN_MENU *const p_MainMenu;
