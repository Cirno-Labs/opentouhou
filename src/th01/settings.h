#pragma once

#include "core/config.h"
#include "core/io/reader.h"
#include "core/vfs/types.h"

typedef enum {
    SETTINGS_DIFFICULTY_EASY,
    SETTINGS_DIFFICULTY_NORMAL,
    SETTINGS_DIFFICULTY_HARD,
    SETTINGS_DIFFICULTY_LUNATIC,
} SETTINGS_DIFFICULTY;

typedef enum {
    SETTINGS_MUSIC_OFF,
    SETTINGS_MUSIC_FM,
} SETTINGS_MUSIC;

typedef struct {
    int32_t difficulty;
    int32_t music;
    int32_t bombs;
    // Extra lives: the menu shows this + 3; remaining lives start at this + 2.
    int32_t lives_extra;
    // Acts once per confirm press where the original acts on a held key.
    bool fix_debouncing;
} TH01_SETTINGS;

// Holds the game options and optional fixes, kept in the [game] and [fixes]
// sections of the user directory's config.ini.
extern TH01_SETTINGS g_Settings;
extern const CONFIG_SCHEMA g_SettingsSchema;

// Imports REIIDEN.CFG when user_files has no config.ini; an existing config.ini
// wins even when empty. Keeps settings when REIIDEN.CFG is absent or invalid.
RESULT Settings_ImportOriginal(
    VFS *vfs, const VFS *user_files, TH01_SETTINGS *settings);
