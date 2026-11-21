#include "th01/settings.h"

#include "core/vfs/open.h"

#include <string.h>

#define M_DIFFICULTY SETTINGS_DIFFICULTY_NORMAL
#define M_MUSIC SETTINGS_MUSIC_FM
#define M_BOMBS 1
#define M_LIVES_EXTRA 2

static const char *const m_DifficultyNames[] = { "easy", "normal", "hard",
                                                 "lunatic", nullptr };
static const char *const m_MusicNames[] = { "off", "fm", nullptr };

static const CONFIG_OPTION m_Options[] = {
    CONFIG_ENUM(
        TH01_SETTINGS, "game", "difficulty", difficulty, M_DIFFICULTY,
        m_DifficultyNames),
    CONFIG_ENUM(TH01_SETTINGS, "game", "music", music, M_MUSIC, m_MusicNames),
    CONFIG_INT(TH01_SETTINGS, "game", "bombs", bombs, M_BOMBS, 0, 5),
    CONFIG_INT(
        TH01_SETTINGS, "game", "lives_extra", lives_extra, M_LIVES_EXTRA, 0, 4),
    CONFIG_BOOL(
        TH01_SETTINGS, "fixes", "music_restart", fix_music_restart, true),
};

const CONFIG_SCHEMA g_SettingsSchema =
    CONFIG_SCHEMA_OF(TH01_SETTINGS, m_Options);

TH01_SETTINGS g_Settings = {
    .difficulty = M_DIFFICULTY,
    .music = M_MUSIC,
    .bombs = M_BOMBS,
    .lives_extra = M_LIVES_EXTRA,
    .fix_music_restart = true,
};

RESULT Settings_ReadOriginal(READER *const reader, TH01_SETTINGS *const out)
{
    char magic[7];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    TH01_SETTINGS settings = *out;
    settings.difficulty = Reader_ReadI8(reader);
    settings.music = Reader_ReadI8(reader);
    settings.bombs = Reader_ReadI8(reader);
    settings.lives_extra = Reader_ReadI8(reader);
    MUST(Reader_Result(reader));
    FAIL_IF(
        memcmp(magic, "REIIDEN", sizeof(magic)) != 0,
        "Invalid REIIDEN.CFG signature");
    FAIL_IF(
        settings.difficulty < SETTINGS_DIFFICULTY_EASY
            || settings.difficulty > SETTINGS_DIFFICULTY_LUNATIC,
        "Invalid REIIDEN.CFG difficulty: %d", settings.difficulty);
    FAIL_IF(
        settings.music < SETTINGS_MUSIC_OFF
            || settings.music > SETTINGS_MUSIC_FM,
        "Invalid REIIDEN.CFG music mode: %d", settings.music);
    FAIL_IF(
        settings.bombs < 0 || settings.bombs > 5,
        "Invalid REIIDEN.CFG bombs: %d", settings.bombs);
    FAIL_IF(
        settings.lives_extra < 0 || settings.lives_extra > 4,
        "Invalid REIIDEN.CFG extra lives: %d", settings.lives_extra);
    *out = settings;
    return OK;
}

RESULT Settings_ImportOriginal(
    VFS *const vfs, const VFS *const user_files, TH01_SETTINGS *const settings)
{
    if (!Vfs_Exists(vfs, "REIIDEN.CFG")
        || Vfs_Exists(user_files, "config.ini")) {
        return OK;
    }
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.CFG", VFS_STREAM, &reader));
    const RESULT result = Settings_ReadOriginal(&reader, settings);
    Reader_Close(&reader);
    return result;
}
