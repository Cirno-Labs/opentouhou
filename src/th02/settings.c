#include "th02/settings.h"

#include "core/vfs/open.h"

static const char *const m_DifficultyNames[] = {
    "easy", "normal", "hard", "lunatic", nullptr,
};
static const char *const m_MusicNames[] = { "off", "fm", "midi", nullptr };

static const CONFIG_OPTION m_Options[] = {
    CONFIG_ENUM(
        TH02_SETTINGS, "game", "difficulty", difficulty, TH02_DIFFICULTY_NORMAL,
        m_DifficultyNames),
    CONFIG_ENUM(
        TH02_SETTINGS, "game", "music", music, TH02_MUSIC_FM, m_MusicNames),
    CONFIG_INT(TH02_SETTINGS, "game", "bombs", bombs, 3, 0, 3),
    CONFIG_INT(TH02_SETTINGS, "game", "lives", lives, 2, 0, 4),
    CONFIG_BOOL(TH02_SETTINGS, "game", "reduce_effects", reduce_effects, false),
    CONFIG_BOOL(
        TH02_SETTINGS, "fixes", "bomb_transition_overread",
        fix_bomb_transition_overread, true),
};

const CONFIG_SCHEMA g_Th02SettingsSchema =
    CONFIG_SCHEMA_OF(TH02_SETTINGS, m_Options);

TH02_SETTINGS g_Th02Settings = {
    .difficulty = TH02_DIFFICULTY_NORMAL,
    .music = TH02_MUSIC_FM,
    .bombs = 3,
    .lives = 2,
    .fix_bomb_transition_overread = true,
};

RESULT Settings_ReadOriginal(READER *const reader, TH02_SETTINGS *const out)
{
    TH02_SETTINGS settings;
    Config_SetDefaults(&g_Th02SettingsSchema, &settings);
    settings.difficulty = Reader_ReadI8(reader);
    settings.music = Reader_ReadI8(reader);
    settings.bombs = Reader_ReadI8(reader);
    settings.lives = Reader_ReadI8(reader);
    const uint8_t reduce_effects = Reader_ReadU8(reader);
    Reader_Skip(reader, 3);
    MUST(Reader_Result(reader));
    FAIL_IF(
        settings.difficulty < TH02_DIFFICULTY_EASY
            || settings.difficulty > TH02_DIFFICULTY_LUNATIC,
        "Invalid huuma.cfg difficulty: %d", settings.difficulty);
    FAIL_IF(
        settings.music < TH02_MUSIC_OFF || settings.music > TH02_MUSIC_MIDI,
        "Invalid huuma.cfg music mode: %d", settings.music);
    FAIL_IF(
        settings.bombs < 0 || settings.bombs > 3, "Invalid huuma.cfg bombs: %d",
        settings.bombs);
    FAIL_IF(
        settings.lives < 0 || settings.lives > 4,
        "Invalid huuma.cfg spare lives: %d", settings.lives);
    FAIL_IF(
        reduce_effects > 1, "Invalid huuma.cfg effect flag: %u",
        reduce_effects);
    settings.reduce_effects = reduce_effects != 0;
    *out = settings;
    return OK;
}

RESULT Settings_ImportOriginal(
    VFS *const original_files, const VFS *const user_files,
    TH02_SETTINGS *const settings)
{
    if (Vfs_Exists(user_files, "config.ini")
        || !Vfs_Exists(original_files, "huuma.cfg")) {
        return OK;
    }
    READER reader;
    MUST(Vfs_Open(original_files, "huuma.cfg", VFS_STREAM, &reader));
    const RESULT result = Settings_ReadOriginal(&reader, settings);
    Reader_Close(&reader);
    return result;
}
