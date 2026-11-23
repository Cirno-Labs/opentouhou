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
    // Stores credit_lives_extra: the menu shows this + 3, while the
    // game's remaining-lives counter starts at this + 2.
    int32_t lives_extra;
    // Acts once per confirm press where the original acts on a held key:
    // the music room starts a track once, not on every frame, and the
    // Continue prompt ignores the press still held from the name entry.
    bool fix_debouncing;
} TH01_SETTINGS;

// Holds the original game options and optional fixes. shell/game loads and
// saves these in the [game] and [fixes] sections of the user directory's
// config.ini; shutdown must leave them intact.
extern TH01_SETTINGS g_Settings;
extern const CONFIG_SCHEMA g_SettingsSchema;

// Reads the eleven-byte REIIDEN.CFG record into the game options of out.
// Accepts trailing bytes, as the original does, but rejects invalid option
// ranges, which the original uses as unchecked indices.
RESULT Settings_ReadOriginal(READER *reader, TH01_SETTINGS *out);

// Imports REIIDEN.CFG from vfs when user_files has no config.ini, compared
// without ASCII case; an existing config.ini wins even when empty. Leaves
// settings unchanged if REIIDEN.CFG is absent or invalid.
RESULT Settings_ImportOriginal(
    VFS *vfs, const VFS *user_files, TH01_SETTINGS *settings);
