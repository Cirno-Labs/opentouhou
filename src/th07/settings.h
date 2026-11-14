#pragma once

#include "core/config.h"

#include <stdint.h>

// Holds the settings of the original's th07.cfg that change what the port
// does. shell/game keeps them in the [game] section of config.ini. The
// title's option menu changes them during a tick, as the original changes
// its configuration.
//
// These settings of the original are not ported:
// - the colour depth, the frame skip, MIDI music (MIDI plays the wave
//   files), and the option flags of the original's configuration tool;
// - the pad axis thresholds, which core's [input] settings own.
//
// The window mode and the pad button mapping are the shell's [video]
// fullscreen and [input] bindings. The title's options change them through
// shell/game.

typedef enum {
    SETTINGS_MUSIC_OFF = 0,
    SETTINGS_MUSIC_WAVE = 1,
    SETTINGS_MUSIC_MIDI = 2,
} SETTINGS_MUSIC;

typedef struct {
    int32_t lives; // the lives a game starts with, from 0 to 4
    int32_t bombs; // the bombs a game starts with, from 0 to 3
    // The difficulty that the difficulty select starts on, from 0 to 5: 4
    // is Extra and 5 Phantasm.
    int32_t difficulty;
    int32_t music; // a SETTINGS_MUSIC
    bool sound; // plays sound effects
    // Focuses while a pad holds shoot.
    bool shoot_focuses;
} TH07_SETTINGS;

extern TH07_SETTINGS g_Settings;
extern const CONFIG_SCHEMA g_SettingsSchema;
