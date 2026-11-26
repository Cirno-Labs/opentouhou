#pragma once

// The options menu entered from the main menu: difficulty, music, lives,
// bombs and the effects toggle.

#include "th02/menu.h"
#include "th02/options_menu/render.h"
#include "th02/session.h"
#include "th02/title.h"

typedef enum {
    OPTIONS_MENU_SOUND_STOP,
    OPTIONS_MENU_SOUND_DETERMINE,
    OPTIONS_MENU_SOUND_PLAY,
} OPTIONS_MENU_SOUND_ACTION;

typedef struct {
    OPTIONS_MENU_SOUND_ACTION action;
    bool midi_active;
    bool active;
    bool fm_possible;
} OPTIONS_MENU_SOUND_EVENT;

typedef struct {
    bool fm_available;
    bool midi_possible;
    bool fm_possible;
    bool midi_active;
    bool active;
    void (*emit)(void *context, const OPTIONS_MENU_SOUND_EVENT *event);
    void *context;
} OPTIONS_MENU_SOUND;

typedef struct {
    bool wait_frame;
    bool return_main;
} OPTIONS_MENU_EVENTS;

typedef struct {
    bool ready;
    bool input;
} OPTIONS_MENU_SAMPLE;

// What the main menu hands the options menu: the shared state the main
// menu owns, and the nav and idle counter the two menus share.
typedef struct {
    TH02_SESSION *session;
    TH02_SETTINGS *settings;
    const TITLE_ASSETS *assets;
    const OPTIONS_MENU_TEXT *text;
    OPTIONS_MENU_SOUND *sound;
    TRAM_CONSOLE *console;
    uint8_t clear_attribute;
    MENU_NAV *nav;
    uint16_t *idle_frame;
} OPTIONS_MENU_CONTEXT;

void OptionsMenu_Init(void);
void OptionsMenu_Begin(const OPTIONS_MENU_CONTEXT *context);
OPTIONS_MENU_EVENTS OptionsMenu_Update(uint16_t keys);

// Returns the options menu's counters and input gate.
OPTIONS_MENU_SAMPLE OptionsMenu_Sample(void);
