#include "th07/settings.h"

#define M_LIVES 2
#define M_BOMBS 3
#define M_DIFFICULTY 1
#define M_EFFECTS_QUALITY 2

static const char *const m_MusicNames[] = { "off", "wave", "midi", nullptr };

static const CONFIG_OPTION m_Options[] = {
    CONFIG_INT(TH07_SETTINGS, "game", "lives", lives, M_LIVES, 0, 4),
    CONFIG_INT(TH07_SETTINGS, "game", "bombs", bombs, M_BOMBS, 0, 3),
    CONFIG_INT(
        TH07_SETTINGS, "game", "difficulty", difficulty, M_DIFFICULTY, 0, 5),
    CONFIG_ENUM(
        TH07_SETTINGS, "game", "music", music, SETTINGS_MUSIC_WAVE,
        m_MusicNames),
    CONFIG_BOOL(TH07_SETTINGS, "game", "sound", sound, true),
    CONFIG_BOOL(TH07_SETTINGS, "game", "show_fps", show_fps, true),
    CONFIG_BOOL(TH07_SETTINGS, "game", "shoot_focuses", shoot_focuses, true),
    CONFIG_BOOL(TH07_SETTINGS, "game", "slowdown", slowdown, false),
    CONFIG_INT(
        TH07_SETTINGS, "game", "effects_quality", effects_quality,
        M_EFFECTS_QUALITY, 0, 2),
    CONFIG_BOOL(
        TH07_SETTINGS, "fixes", "replay_desync", fix_replay_desync, true),
    CONFIG_BOOL(TH07_SETTINGS, "fixes", "input_lag", fix_input_lag, false),
};

const CONFIG_SCHEMA g_SettingsSchema =
    CONFIG_SCHEMA_OF(TH07_SETTINGS, m_Options);

TH07_SETTINGS g_Settings = {
    .lives = M_LIVES,
    .bombs = M_BOMBS,
    .difficulty = M_DIFFICULTY,
    .music = SETTINGS_MUSIC_WAVE,
    .sound = true,
    .show_fps = true,
    .shoot_focuses = true,
    .effects_quality = M_EFFECTS_QUALITY,
    .fix_replay_desync = true,
};
