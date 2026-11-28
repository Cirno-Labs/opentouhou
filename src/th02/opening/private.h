#pragma once

// logic.c's own state, exposed for tests; nothing outside the opening/
// module sees this.

#include "th02/opening/logic.h"
#include "th02/wait.h"

#include "pc98/zun/bfnt.h"
#include "pc98/zun/logo1.h"

typedef enum {
    OPENING_LOGO,
    OPENING_TITLE,
    OPENING_MENU,
    OPENING_SUSPENDED,
    OPENING_DONE,
    OPENING_SCORES,
    OPENING_MUSIC,
    OPENING_START,
    OPENING_SHOT_FADE_IN,
    OPENING_SHOTS,
    OPENING_SHOT_FADE_OUT,
    OPENING_RETURN_SCORES,
} OPENING_PHASE;

typedef struct {
    ZUN_LOGO1 logo;
    WAIT wait;
    OPENING_PHASE phase;
    uint8_t logo_wait;
    bool quit_pending;
} OPENING_PROGRAM;

typedef struct {
    const OPENING_ASSETS *assets;
    TH02_SESSION *session;
    TH02_SETTINGS *settings;
    OPTIONS_MENU_SOUND *sound;
    TRAM_CONSOLE *console;
    OPENING_AUDIO audio;
    const BFNT *logo;
    const TITLE_ASSETS *title;
    const SHOT_SELECT_ASSETS *shots;
    const HIGH_SCORES *scores;
    uint8_t next_demo;
    uint8_t last_demo;
    uint8_t clear_attribute;
    HIGH_SCORE_CLEARS clears;
    OPENING_PROGRAM program;
    const MUSIC_ROOM_RENDER *music_room_render;
    const HIGH_SCORE_VIEW_RENDER *high_score_view_render;
    const SHOT_SELECT_RENDER *shot_select_render;
    const MAIN_MENU_RENDER *main_menu_render;
} OPENING;

// A writable view of logic.c's private state, for tests.
extern OPENING *const p_Opening;
