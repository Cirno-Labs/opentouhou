// SPDX-License-Identifier: GPL-3.0-only

#include "th06/settings.h"

#define M_LIVES 2
#define M_BOMBS 3
#define M_DIFFICULTY 1

static const char *const m_MusicNames[] = { "off", "wave", "midi", nullptr };

static const CONFIG_OPTION m_Options[] = {
    CONFIG_INT(TH06_SETTINGS, "game", "lives", lives, M_LIVES, 0, 4),
    CONFIG_INT(TH06_SETTINGS, "game", "bombs", bombs, M_BOMBS, 0, 3),
    CONFIG_INT(
        TH06_SETTINGS, "game", "difficulty", difficulty, M_DIFFICULTY, 0, 4),
    CONFIG_ENUM(
        TH06_SETTINGS, "game", "music", music, SETTINGS_MUSIC_WAVE,
        m_MusicNames),
    CONFIG_BOOL(TH06_SETTINGS, "game", "sound", sound, true),
};

const CONFIG_SCHEMA g_SettingsSchema =
    CONFIG_SCHEMA_OF(TH06_SETTINGS, m_Options);

TH06_SETTINGS g_Settings = {
    .lives = M_LIVES,
    .bombs = M_BOMBS,
    .difficulty = M_DIFFICULTY,
    .music = SETTINGS_MUSIC_WAVE,
    .sound = true,
};
