#include "tests/test.h"
#include "tests/test_dir.h"

#include "core/config.h"
#include "core/ini.h"
#include "shell/game.h"
#include "shell/game/entry.h"

typedef struct {
    int32_t value;
} SETTINGS;

typedef struct {
    bool unused;
} INPUT;

static const CONFIG_OPTION m_Options[] = {
    CONFIG_INT(SETTINGS, "game", "value", value, 1, 0, 10),
};
static const CONFIG_SCHEMA m_SettingsSchema =
    CONFIG_SCHEMA_OF(SETTINGS, m_Options);
static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(INPUT, unused, "z", nullptr, nullptr),
};
static const INPUT_SCHEMA m_InputSchema = INPUT_SCHEMA_OF(INPUT, m_Actions, 1);
static SETTINGS m_Settings;
static char *m_Path;
static int m_Ticks;

static void M_CheckDisk(const int value, const char *const manual)
{
    INI *ini;
    CHECK(
        Result_Log(Ini_Load(m_Path, &g_HeapAllocator, &ini), LOG_LEVEL_ERROR));
    SETTINGS settings;
    Config_Load(&m_SettingsSchema, ini, &settings);
    CHECK_EQ_INT(settings.value, value);
    CHECK_EQ_STR(Ini_Get(ini, "manual", "keep"), manual);
    Ini_Free(ini);
}

static RESULT M_Init(void)
{
    return OK;
}

static bool M_Tick(const void *const input)
{
    M_CheckDisk(m_Ticks == 0 ? 1 : 2, "edited");
    m_Settings.value = m_Ticks == 0 ? 2 : 1;
    m_Ticks++;
    Shell_RequestSaveSettings();
    Shell_RequestSaveSettings();
    M_CheckDisk(m_Ticks == 1 ? 1 : 2, "edited");
    return false;
}

static void M_Shutdown(void)
{
}

static const GAME_DESC m_Desc = {
    .id = "test",
    .title = "Settings save test",
    .version = "0",
    .logical_width = 16,
    .logical_height = 16,
    .input_schema = &m_InputSchema,
    .settings_schema = &m_SettingsSchema,
    .settings = &m_Settings,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};

TEST(save_runs_after_the_tick_and_tracks_each_successful_snapshot)
{
    char *const dir = TestDir_Prepare("queued_settings");
    m_Path = TestDir_Join(dir, "config.ini");
    const char initial[] = "[game]\nvalue = 1\n[manual]\nkeep = initial\n";
    TestDir_Write(m_Path, initial, strlen(initial));
    char *argv[] = { "test", "--data", dir, "--user-dir", dir, nullptr };
    CHECK_EQ_INT(Shell_Init(&m_Desc, 5, argv), SHELL_CONTINUE);
    CHECK_EQ_INT(m_Settings.value, 1);
    INI *ini;
    CHECK(
        Result_Log(Ini_Load(m_Path, &g_HeapAllocator, &ini), LOG_LEVEL_ERROR));
    Ini_Set(ini, "manual", "keep", "edited");
    CHECK(Result_Log(Ini_Save(ini, m_Path), LOG_LEVEL_ERROR));
    Ini_Free(ini);
    for (int tick = 1; tick <= 2; tick++) {
        SHELL_STATUS status = SHELL_CONTINUE;
        for (int frame = 0; frame < 100 && status == SHELL_CONTINUE; frame++) {
            status = Shell_Iterate();
            SDL_Delay(2);
        }
        CHECK_EQ_INT(status, SHELL_SUCCESS);
        CHECK_EQ_INT(m_Ticks, tick);
        M_CheckDisk(tick == 1 ? 2 : 1, "edited");
    }
    Shell_Quit();
    M_CheckDisk(1, "edited");
    Mem_Free(m_Path);
    Mem_Free(dir);
}
