#pragma once

// The main menu: Start, Extra, Scores, Options, Music Room and Quit.

#include "th02/input.h"
#include "th02/main_menu/render.h"
#include "th02/options_menu.h"
#include "th02/session.h"

typedef enum {
    MAIN_MENU_NONE,
    MAIN_MENU_START,
    MAIN_MENU_EXTRA,
    MAIN_MENU_SCORES,
    MAIN_MENU_OPTIONS,
    MAIN_MENU_MUSIC,
    MAIN_MENU_DEMO,
} MAIN_MENU_ACTION;

typedef struct {
    MAIN_MENU_ACTION action;
    uint16_t score_frames;
    uint8_t demo;
    bool wait_frame;
    bool quit;
} MAIN_MENU_EVENTS;

typedef struct {
    int8_t selection;
    uint16_t idle_frame;
    bool in_options;
    bool main_ready;
    bool main_input;
    bool options_ready;
    bool options_input;
} MAIN_MENU_SAMPLE;

// Returns the menu counters and input gates.
MAIN_MENU_SAMPLE MainMenu_Sample(void);

void MainMenu_Init(void);

void MainMenu_Begin(
    TH02_SESSION *session, TH02_SETTINGS *settings, const TITLE_ASSETS *assets,
    const OPTIONS_MENU_TEXT *option_text, OPTIONS_MENU_SOUND *sound,
    TRAM_CONSOLE *console, bool extra_unlocked, uint8_t demo);
MAIN_MENU_EVENTS MainMenu_Update(uint16_t keys);
MAIN_MENU_EVENTS MainMenu_Resume(uint16_t keys);
