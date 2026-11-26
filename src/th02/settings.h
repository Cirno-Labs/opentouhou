#pragma once

// The game's options as config.ini holds them. Without config.ini, the
// original's eight-byte huuma.cfg is imported read-only.

#include "core/config.h"
#include "core/io/reader.h"
#include "core/vfs/types.h"

typedef enum {
    TH02_DIFFICULTY_EASY,
    TH02_DIFFICULTY_NORMAL,
    TH02_DIFFICULTY_HARD,
    TH02_DIFFICULTY_LUNATIC,
} TH02_DIFFICULTY;

typedef enum {
    TH02_MUSIC_OFF,
    TH02_MUSIC_FM,
    TH02_MUSIC_MIDI,
} TH02_MUSIC;

typedef struct {
    int32_t difficulty, music, bombs, lives;
    bool reduce_effects, fix_bomb_transition_overread;
} TH02_SETTINGS;

// The config.ini schema; lives holds spare lives, shown as lives + 1.
extern const CONFIG_SCHEMA g_Th02SettingsSchema;
extern TH02_SETTINGS g_Th02Settings;

// Reads the original eight-byte settings, ignoring its resident segment
// and debug byte.
RESULT Settings_ReadOriginal(READER *reader, TH02_SETTINGS *out);

// Imports the original settings read-only when config.ini is absent; an
// empty config.ini counts as present.
RESULT Settings_ImportOriginal(
    VFS *original_files, const VFS *user_files, TH02_SETTINGS *settings);
