#include "th07/main.h"

#include "th07/buttons.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/settings.h"
#include "th07/sound.h"

#include "version.h"

#include <time.h>

static bool m_GameInitialized;

static RESULT M_Init(void)
{
    MUST(Files_Mount());
    Files_SetPatches(Shell_GetPatches());
    Result_Log(SoundPlayer_Init(), LOG_LEVEL_WARNING);
    m_GameInitialized = true;
    MUST(Game_Init());
    // Seeds the gameplay RNG from the clock once, outside the ticks, as the
    // original seeds it at start. Replays restore their own seeds.
    Rng_SetSeed(&g_Rng, (uint16_t)time(nullptr));
    return OK;
}

static bool M_Tick(const void *const input)
{
    const bool running = Game_Tick(input);
    SoundPlayer_Update();
    return running;
}

static void M_Shutdown(void)
{
    if (m_GameInitialized) {
        Game_Shutdown();
        m_GameInitialized = false;
    }
    SoundPlayer_Shutdown();
}

const GAME_DESC g_GameDesc = {
    .id = "th07",
    .title = "東方妖々夢 ~ Perfect Cherry Blossom",
    .version = GAME_VERSION,
    .logical_width = 640,
    .logical_height = 480,
    .input_schema = &g_InputSchema,
    .settings_schema = &g_SettingsSchema,
    .settings = &g_Settings,
    .patch_game = &g_PatchGame,
    .data_files = g_FilesData,
    .data_file_count = FILES_DATA_COUNT,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
