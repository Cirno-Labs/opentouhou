#include "th01/settings.h"

#include "core/io/writer.h"
#include "core/vfs.h"

#include "test.h"
#include "test_dir.h"

#include <string.h>

static const uint8_t m_Original[] = { 'R', 'E', 'I', 'I', 'D', 'E',
                                      'N', 3,   0,   5,   4 };
static const TH01_SETTINGS m_Selected = {
    .difficulty = SETTINGS_DIFFICULTY_HARD,
    .music = SETTINGS_MUSIC_FM,
    .bombs = 2,
    .lives_extra = 1,
};

static void M_CheckSettings(
    const TH01_SETTINGS actual, const TH01_SETTINGS expected)
{
    CHECK_EQ_INT(actual.difficulty, expected.difficulty);
    CHECK_EQ_INT(actual.music, expected.music);
    CHECK_EQ_INT(actual.bombs, expected.bombs);
    CHECK_EQ_INT(actual.lives_extra, expected.lives_extra);
    CHECK_EQ_INT(actual.fix_debouncing, expected.fix_debouncing);
}

static TH01_SETTINGS M_Defaults(void)
{
    TH01_SETTINGS settings;
    Config_SetDefaults(&g_SettingsSchema, &settings);
    return settings;
}

static INI *M_Parse(const char *const text)
{
    INI *ini;
    CHECK(Result_Log(
        Ini_Parse(text, strlen(text), &g_HeapAllocator, &ini),
        LOG_LEVEL_ERROR));
    return ini;
}

static VFS *M_Mount(const char *const directory)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Log(
        Vfs_MountDir(vfs, "", directory, 0, false), LOG_LEVEL_ERROR));
    return vfs;
}

static RESULT M_Read(
    const uint8_t *const bytes, const size_t size, TH01_SETTINGS *const out)
{
    char *const directory = TestDir_Prepare("record");
    char *const path = TestDir_Join(directory, "REIIDEN.CFG");
    TestDir_Write(path, bytes, size);
    VFS *const vfs = M_Mount(directory);
    VFS *const user_files = Vfs_Create(&g_HeapAllocator);
    const RESULT result = Settings_ImportOriginal(vfs, user_files, out);
    Vfs_Destroy(user_files);
    Vfs_Destroy(vfs);
    Mem_Free(path);
    Mem_Free(directory);
    return result;
}

TEST(defaults_match_the_original_options)
{
    const TH01_SETTINGS settings = M_Defaults();
    M_CheckSettings(
        settings,
        (TH01_SETTINGS) {
            .difficulty = SETTINGS_DIFFICULTY_NORMAL,
            .music = SETTINGS_MUSIC_FM,
            .bombs = 1,
            .lives_extra = 2,
            .fix_debouncing = true,
        });
    M_CheckSettings(g_Settings, settings);
    CHECK_EQ_INT(settings.lives_extra + 3, 5);
    CHECK_EQ_INT(settings.lives_extra + 2, 4);
}

TEST(native_ini_round_trip_keeps_all_options_and_other_sections)
{
    INI *const ini = M_Parse("[video]\nfullscreen = true\n");
    Config_Save(&g_SettingsSchema, ini, &m_Selected);
    CHECK_EQ_STR(Ini_Get(ini, "game", "difficulty"), "hard");
    CHECK_EQ_STR(Ini_Get(ini, "game", "music"), "fm");
    CHECK_EQ_STR(Ini_Get(ini, "game", "bombs"), "2");
    CHECK_EQ_STR(Ini_Get(ini, "game", "lives_extra"), "1");
    CHECK_EQ_STR(Ini_Get(ini, "fixes", "debouncing"), "false");
    CHECK_EQ_STR(Ini_Get(ini, "video", "fullscreen"), "true");
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Ini_Write(ini, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    INI *restored;
    CHECK(Result_Log(
        Ini_Parse((const char *)bytes, size, &g_HeapAllocator, &restored),
        LOG_LEVEL_ERROR));
    TH01_SETTINGS settings;
    Config_Load(&g_SettingsSchema, restored, &settings);
    M_CheckSettings(settings, m_Selected);
    Ini_Free(restored);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    Ini_Free(ini);
}

TEST(native_ini_rejects_invalid_ranges_and_names)
{
    static const char *const invalid[] = {
        ("[game]\ndifficulty = extra\nmusic = midi\nbombs = -1\nlives_extra = "
         "-1\n"),
        "[game]\ndifficulty = 256\nmusic = none\nbombs = 6\nlives_extra = 5\n",
        "[game]\nbombs = many\nlives_extra = 2more\n",
    };
    for (size_t i = 0; i < ARRAY_SIZE(invalid); i++) {
        INI *const ini = M_Parse(invalid[i]);
        TH01_SETTINGS settings = m_Selected;
        Config_Load(&g_SettingsSchema, ini, &settings);
        M_CheckSettings(settings, M_Defaults());
        Ini_Free(ini);
    }
}

TEST(native_ini_accepts_the_original_option_boundaries)
{
    static const char *const texts[] = {
        "[game]\ndifficulty = EASY\nmusic = off\nbombs = 0\nlives_extra = 0\n",
        "[game]\ndifficulty = lunatic\nmusic = FM\nbombs = 5\nlives_extra = "
        "4\n",
    };
    static const TH01_SETTINGS expected[] = {
        { SETTINGS_DIFFICULTY_EASY, SETTINGS_MUSIC_OFF, 0, 0, true },
        { SETTINGS_DIFFICULTY_LUNATIC, SETTINGS_MUSIC_FM, 5, 4, true },
    };
    for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
        INI *const ini = M_Parse(texts[i]);
        TH01_SETTINGS settings;
        Config_Load(&g_SettingsSchema, ini, &settings);
        M_CheckSettings(settings, expected[i]);
        Ini_Free(ini);
    }
}

TEST(original_record_maps_its_bytes_in_order)
{
    TH01_SETTINGS settings = m_Selected;
    CHECK(Result_Log(
        M_Read(m_Original, sizeof(m_Original), &settings), LOG_LEVEL_ERROR));
    M_CheckSettings(
        settings,
        (TH01_SETTINGS) { SETTINGS_DIFFICULTY_LUNATIC, SETTINGS_MUSIC_OFF, 5, 4,
                          false });
}

TEST(original_record_accepts_every_valid_option_combination)
{
    uint8_t bytes[sizeof(m_Original)];
    memcpy(bytes, m_Original, sizeof(bytes));
    for (uint8_t rank = 0; rank < 4; rank++) {
        for (uint8_t music = 0; music < 2; music++) {
            for (uint8_t bombs = 0; bombs <= 5; bombs++) {
                for (uint8_t extra = 0; extra < 5; extra++) {
                    bytes[7] = rank;
                    bytes[8] = music;
                    bytes[9] = bombs;
                    bytes[10] = extra;
                    TH01_SETTINGS settings = {};
                    CHECK(
                        Result_Ignore(M_Read(bytes, sizeof(bytes), &settings)));
                    M_CheckSettings(
                        settings,
                        (TH01_SETTINGS) { rank, music, bombs, extra, false });
                }
            }
        }
    }
}

TEST(original_record_rejects_all_truncations_without_changing_output)
{
    for (size_t size = 0; size < sizeof(m_Original); size++) {
        TH01_SETTINGS settings = m_Selected;
        CHECK(!Result_Ignore(M_Read(m_Original, size, &settings)));
        M_CheckSettings(settings, m_Selected);
    }
}

TEST(original_record_rejects_each_bad_magic_byte)
{
    for (int32_t i = 0; i < 7; i++) {
        uint8_t bytes[sizeof(m_Original)];
        memcpy(bytes, m_Original, sizeof(bytes));
        bytes[i] ^= 0x20;
        TH01_SETTINGS settings = m_Selected;
        CHECK(!Result_Ignore(M_Read(bytes, sizeof(bytes), &settings)));
        M_CheckSettings(settings, m_Selected);
    }
}

TEST(original_record_rejects_signed_and_upper_range_errors)
{
    static const uint8_t first_invalid[] = { 4, 2, 6, 5 };
    for (size_t option = 0; option < ARRAY_SIZE(first_invalid); option++) {
        const uint8_t invalid[] = { first_invalid[option], 0x80, 0xff };
        for (size_t i = 0; i < ARRAY_SIZE(invalid); i++) {
            uint8_t bytes[sizeof(m_Original)];
            memcpy(bytes, m_Original, sizeof(bytes));
            bytes[7 + option] = invalid[i];
            TH01_SETTINGS settings = m_Selected;
            CHECK(!Result_Ignore(M_Read(bytes, sizeof(bytes), &settings)));
            M_CheckSettings(settings, m_Selected);
        }
    }
}

TEST(original_record_accepts_trailing_bytes)
{
    uint8_t bytes[sizeof(m_Original) + 3] = {};
    memcpy(bytes, m_Original, sizeof(m_Original));
    TH01_SETTINGS settings = m_Selected;
    CHECK(Result_Log(M_Read(bytes, sizeof(bytes), &settings), LOG_LEVEL_ERROR));
    M_CheckSettings(
        settings,
        (TH01_SETTINGS) { SETTINGS_DIFFICULTY_LUNATIC, SETTINGS_MUSIC_OFF, 5, 4,
                          false });
}

TEST(import_of_a_missing_original_keeps_native_defaults)
{
    char *const directory = TestDir_Prepare("missing");
    VFS *const vfs = M_Mount(directory);
    TH01_SETTINGS settings = M_Defaults();
    CHECK(Result_Ignore(Settings_ImportOriginal(vfs, vfs, &settings)));
    M_CheckSettings(settings, M_Defaults());
    Vfs_Destroy(vfs);
    Mem_Free(directory);
}

TEST(first_launch_import_reads_original_without_writing_it)
{
    char *const directory = TestDir_Prepare("first");
    char *const path = TestDir_Join(directory, "reiiden.cfg");
    TestDir_Write(path, m_Original, sizeof(m_Original));
    VFS *const vfs = M_Mount(directory);
    VFS *const user_files = Vfs_Create(&g_HeapAllocator);
    TH01_SETTINGS settings = M_Defaults();
    CHECK(Result_Log(
        Settings_ImportOriginal(vfs, user_files, &settings), LOG_LEVEL_ERROR));
    M_CheckSettings(settings, (TH01_SETTINGS) { 3, 0, 5, 4, true });
    READER reader;
    CHECK(Result_Ignore(Reader_OpenFile(&reader, path)));
    CHECK_EQ_INT(Reader_Size(&reader), sizeof(m_Original));
    for (size_t i = 0; i < sizeof(m_Original); i++) {
        CHECK_EQ_INT(Reader_ReadU8(&reader), m_Original[i]);
    }
    Reader_Close(&reader);
    char *const config = TestDir_Join(directory, "config.ini");
    CHECK(!File_Exists(config));
    Mem_Free(config);
    Vfs_Destroy(user_files);
    Vfs_Destroy(vfs);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(existing_native_config_blocks_import_even_when_empty_or_uppercase)
{
    static const char *const names[] = { "config.ini", "CONFIG.INI" };
    for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
        char *const directory = TestDir_Prepare(names[i]);
        char *const original = TestDir_Join(directory, "REIIDEN.CFG");
        char *const native = TestDir_Join(directory, names[i]);
        TestDir_Write(original, m_Original, sizeof(m_Original));
        TestDir_Write(native, "", 0);
        VFS *const vfs = M_Mount(directory);
        TH01_SETTINGS settings = m_Selected;
        CHECK(Result_Ignore(Settings_ImportOriginal(vfs, vfs, &settings)));
        M_CheckSettings(settings, m_Selected);
        Vfs_Destroy(vfs);
        Mem_Free(native);
        Mem_Free(original);
        Mem_Free(directory);
    }
}

TEST(invalid_original_keeps_settings_unchanged)
{
    char *const directory = TestDir_Prepare("failure");
    char *const original = TestDir_Join(directory, "REIIDEN.CFG");
    TestDir_Write(original, m_Original, sizeof(m_Original) - 1);
    VFS *const vfs = M_Mount(directory);
    TH01_SETTINGS settings = m_Selected;
    CHECK(!Result_Ignore(Settings_ImportOriginal(vfs, vfs, &settings)));
    M_CheckSettings(settings, m_Selected);
    Vfs_Destroy(vfs);
    Mem_Free(original);
    Mem_Free(directory);
}
