#include "tests/test.h"
#include "tests/test_dir.h"
#include "th02/settings.h"

#include "core/vfs.h"

static void M_Check(const TH02_SETTINGS *actual, const TH02_SETTINGS *expected)
{
    CHECK_EQ_INT(actual->difficulty, expected->difficulty);
    CHECK_EQ_INT(actual->music, expected->music);
    CHECK_EQ_INT(actual->bombs, expected->bombs);
    CHECK_EQ_INT(actual->lives, expected->lives);
    CHECK_EQ_INT(actual->reduce_effects, expected->reduce_effects);
    CHECK_EQ_INT(
        actual->fix_bomb_transition_overread,
        expected->fix_bomb_transition_overread);
}

static TH02_SETTINGS M_Defaults(void)
{
    TH02_SETTINGS settings = {};
    Config_SetDefaults(&g_Th02SettingsSchema, &settings);
    return settings;
}

static RESULT M_Read(const uint8_t *bytes, size_t size, TH02_SETTINGS *out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    const RESULT result = Settings_ReadOriginal(&reader, out);
    Reader_Close(&reader);
    return result;
}

TEST(schema_defaults_and_native_ini_round_trip_preserve_options_and_fixes)
{
    const TH02_SETTINGS defaults = M_Defaults();
    const TH02_SETTINGS stock = { 1, 1, 3, 2, false, true };
    M_Check(&defaults, &stock);
    CHECK(defaults.fix_bomb_transition_overread);
    M_Check(&g_Th02Settings, &stock);
    const TH02_SETTINGS selected = { 3, 2, 0, 4, true, false };
    const char *text = "[video]\nfullscreen = true\n";
    INI *ini;
    CHECK(Result_Ignore(Ini_Parse(text, strlen(text), &g_HeapAllocator, &ini)));
    Config_Save(&g_Th02SettingsSchema, ini, &selected);
    CHECK_EQ_STR(Ini_Get(ini, "game", "difficulty"), "lunatic");
    CHECK_EQ_STR(Ini_Get(ini, "game", "music"), "midi");
    CHECK_EQ_STR(Ini_Get(ini, "game", "lives"), "4");
    CHECK_EQ_STR(Ini_Get(ini, "game", "bombs"), "0");
    CHECK_EQ_STR(Ini_Get(ini, "game", "reduce_effects"), "true");
    CHECK_EQ_STR(Ini_Get(ini, "video", "fullscreen"), "true");
    CHECK_EQ_STR(Ini_Get(ini, "fixes", "bomb_transition_overread"), "false");
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Ini_Write(ini, &writer);
    size_t size;
    uint8_t *bytes = Writer_TakeBuffer(&writer, &size);
    INI *restored;
    CHECK(Result_Ignore(
        Ini_Parse((const char *)bytes, size, &g_HeapAllocator, &restored)));
    TH02_SETTINGS loaded;
    Config_Load(&g_Th02SettingsSchema, restored, &loaded);
    M_Check(&loaded, &selected);
    CHECK(!loaded.fix_bomb_transition_overread);
    Ini_Free(restored);
    Mem_Free(bytes);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    Ini_Free(ini);
    text =
        "[game]\ndifficulty = extra\nmusic = invalid\nbombs = 4\nlives = "
        "-1\nreduce_effects = invalid\n";
    CHECK(Result_Ignore(Ini_Parse(text, strlen(text), &g_HeapAllocator, &ini)));
    Config_Load(&g_Th02SettingsSchema, ini, &loaded);
    M_Check(&loaded, &stock);
    Ini_Free(ini);
}

TEST(original_import_accepts_valid_options_without_using_transient_bytes)
{
    uint8_t bytes[11] = {};
    for (uint8_t rank = 0; rank < 4; rank++) {
        for (uint8_t music = 0; music < 3; music++) {
            for (uint8_t bombs = 0; bombs < 4; bombs++) {
                for (uint8_t lives = 0; lives < 5; lives++) {
                    for (uint8_t reduce = 0; reduce < 2; reduce++) {
                        bytes[0] = rank;
                        bytes[1] = music;
                        bytes[2] = bombs;
                        bytes[3] = lives;
                        bytes[4] = reduce;
                        bytes[5] = rank * 17;
                        bytes[6] = music * 63;
                        bytes[7] = 0xff;
                        TH02_SETTINGS settings;
                        CHECK(Result_Ignore(
                            M_Read(bytes, sizeof(bytes), &settings)));
                        const TH02_SETTINGS expected = { rank,        music,
                                                         bombs,       lives,
                                                         reduce != 0, true };
                        M_Check(&settings, &expected);
                    }
                }
            }
        }
    }
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    TH02_SETTINGS settings;
    CHECK(Result_Ignore(Settings_ReadOriginal(&reader, &settings)));
    CHECK_EQ_INT(Reader_Tell(&reader), 8);
    CHECK_EQ_INT(Reader_Remaining(&reader), 3);
    Reader_Skip(&reader, 4);
    const TH02_SETTINGS before = settings;
    CHECK(!Result_Ignore(Settings_ReadOriginal(&reader, &settings)));
    M_Check(&settings, &before);
    Reader_Close(&reader);
}

TEST(invalid_original_records_leave_output_unchanged)
{
    const TH02_SETTINGS before = { 2, 0, 1, 4, true, false };
    const uint8_t original[] = { 1, 1, 3, 2, 0, 0x00, 0x48, 0 };
    for (size_t size = 0; size < sizeof(original); size++) {
        TH02_SETTINGS settings = before;
        CHECK(!Result_Ignore(M_Read(original, size, &settings)));
        M_Check(&settings, &before);
    }
    const uint8_t invalid_start[] = { 4, 3, 4, 5, 2 };
    for (size_t field = 0; field < 5; field++) {
        for (uint16_t value = invalid_start[field]; value < 256; value++) {
            uint8_t bytes[sizeof(original)];
            memcpy(bytes, original, sizeof(bytes));
            bytes[field] = value;
            TH02_SETTINGS settings = before;
            CHECK(!Result_Ignore(M_Read(bytes, sizeof(bytes), &settings)));
            M_Check(&settings, &before);
        }
    }
}

TEST(first_import_reads_only_originals_and_native_config_always_takes_priority)
{
    const char *const names[] = { nullptr, "config.ini", "CONFIG.INI" };
    const uint8_t bytes[] = { 3, 2, 1, 4, 1, 0, 0, 0xff };
    for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
        char *original_dir = TestDir_Prepare("settings-original");
        char *user_dir = TestDir_Prepare("settings-user");
        char *original_path = TestDir_Join(original_dir, "HUUMA.CFG");
        TestDir_Write(original_path, bytes, sizeof(bytes));
        if (names[i] != nullptr) {
            char *native_path = TestDir_Join(user_dir, names[i]);
            TestDir_Write(native_path, "", 0);
            Mem_Free(native_path);
        }
        VFS *original = Vfs_Create(&g_HeapAllocator);
        VFS *user = Vfs_Create(&g_HeapAllocator);
        CHECK(
            Result_Ignore(Vfs_MountDir(original, "", original_dir, 0, false)));
        CHECK(Result_Ignore(Vfs_MountDir(user, "", user_dir, 0, false)));
        TH02_SETTINGS settings = M_Defaults();
        CHECK(
            Result_Ignore(Settings_ImportOriginal(original, user, &settings)));
        const TH02_SETTINGS expected = names[i] == nullptr
            ? (TH02_SETTINGS) { 3, 2, 1, 4, true, true }
            : M_Defaults();
        M_Check(&settings, &expected);
        READER reader;
        CHECK(Result_Ignore(Reader_OpenFile(&reader, original_path)));
        uint8_t after[sizeof(bytes)];
        Reader_ReadBytes(&reader, after, sizeof(after));
        CHECK(Result_Ignore(Reader_Result(&reader)));
        CHECK_EQ_INT(Reader_Size(&reader), sizeof(bytes));
        CHECK(memcmp(after, bytes, sizeof(bytes)) == 0);
        Reader_Close(&reader);
        CHECK(!Vfs_Exists(original, "config.ini"));
        Vfs_Destroy(user);
        Vfs_Destroy(original);
        Mem_Free(original_path);
        Mem_Free(user_dir);
        Mem_Free(original_dir);
    }
    VFS *empty = Vfs_Create(&g_HeapAllocator);
    TH02_SETTINGS settings = M_Defaults();
    const TH02_SETTINGS before = settings;
    CHECK(Result_Ignore(Settings_ImportOriginal(empty, empty, &settings)));
    M_Check(&settings, &before);
    Vfs_Destroy(empty);
}

TEST(invalid_optional_import_reports_failure_without_changing_settings)
{
    char *const directory = TestDir_Prepare("invalid-settings");
    char *const path = TestDir_Join(directory, "huuma.cfg");
    const uint8_t bytes[] = { 1, 1, 3, 2, 2, 0, 0, 0 };
    TestDir_Write(path, bytes, sizeof(bytes));
    VFS *const original = Vfs_Create(&g_HeapAllocator);
    VFS *const user = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Ignore(Vfs_MountDir(original, "", directory, 0, false)));
    TH02_SETTINGS settings = M_Defaults();
    const TH02_SETTINGS before = settings;
    CHECK(!Result_Ignore(Settings_ImportOriginal(original, user, &settings)));
    M_Check(&settings, &before);
    Vfs_Destroy(user);
    Vfs_Destroy(original);
    Mem_Free(path);
    Mem_Free(directory);
}
