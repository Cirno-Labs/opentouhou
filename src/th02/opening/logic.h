#pragma once

#include "th02/main_menu.h"
#include "th02/music_room.h"
#include "th02/menu.h"
#include "th02/opening/assets.h"

typedef struct {
    void (*title)(void *context, const TITLE_AUDIO_REQUEST *request);
    MUSIC_ROOM_AUDIO music;
    void (*save_settings)(void *context);
    void *context;
} OPENING_AUDIO;

typedef struct {
    MAIN_MENU_EVENTS menu;
    bool hide_text;
} OPENING_EVENTS;

typedef struct {
    MAIN_MENU_SAMPLE menu;
    int32_t logo_frame;
    bool logo;
    bool in_menu;
} OPENING_SAMPLE;

// Returns the state replays observe.
OPENING_SAMPLE Opening_Sample(void);

// Starts the opening at the launcher logo.
void Opening_Begin(
    const OPENING_ASSETS *assets, TH02_SESSION *session,
    TH02_SETTINGS *settings, const HIGH_SCORES *scores,
    OPTIONS_MENU_SOUND *sound, TRAM_CONSOLE *console,
    const OPENING_AUDIO *audio, uint8_t next_demo);

// Reenters OP without the logo, keeping the music room's state and
// rechecking whether Extra is open.
void Opening_BeginProgram(void);

// Shows the 350-frame score return before the next title sequence.
void Opening_BeginDemoReturn(uint16_t keys);

// Records the demo played, for the next visit's rotation.
void Opening_DemoStarted(uint8_t demo);

// Returns the menu handoff and whether the launcher suppresses text.
OPENING_EVENTS Opening_Update(
    bool reference_edge, uint16_t keys, bool skip_logo);
MAIN_MENU_EVENTS Opening_Resume(uint16_t keys);
void Opening_Shutdown(void);
