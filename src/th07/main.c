#include "th07/main.h"

#include "th07/files.h"
#include "th07/game.h"
#include "th07/settings.h"

#include "version.h"

#include <time.h>

static bool m_GameInitialized;

// Has no actions: oth07 reads no input.
static const INPUT_SCHEMA m_InputSchema = { .players = 1 };

static RESULT M_Init(void)
{
    MUST(Files_Init(Shell_GetDataDir()));
    Files_SetPatches(Shell_GetPatches());
    m_GameInitialized = true;
    MUST(Game_Init());
    // Seeds the gameplay RNG from the clock once, outside the ticks, as the
    // original seeds it at start. Replays restore their own seeds.
    Rng_SetSeed(&g_Rng, (uint16_t)time(nullptr));
    return OK;
}

static bool M_Tick(const void *const input)
{
    return Game_Tick();
}

static void M_Shutdown(void)
{
    if (m_GameInitialized) {
        Game_Shutdown();
        m_GameInitialized = false;
    }
    Files_Shutdown();
}

const GAME_DESC g_GameDesc = {
    .id = "th07",
    .title = "東方妖々夢 ~ Perfect Cherry Blossom",
    .version = GAME_VERSION,
    .logical_width = 640,
    .logical_height = 480,
    .input_schema = &m_InputSchema,
    .settings_schema = &g_SettingsSchema,
    .settings = &g_Settings,
    .patch_game = &g_PatchGame,
    .data_files = g_FilesArchives,
    .data_file_count = FILES_ARCHIVE_COUNT,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
