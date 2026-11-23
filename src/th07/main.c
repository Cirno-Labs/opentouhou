#include "th07/main.h"

#include "th07/anm.h"
#include "th07/buttons.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/replay.h"
#include "th07/screens/title.h"
#include "th07/screens/result/records.h"
#include "th07/screens/result/saves.h"
#include "th07/settings.h"
#include "th07/sound.h"
#include "th07/strings.h"

#include "stg/note.h"
#include "stg/replay_files.h"
#include "stg/translation.h"

#include "shell/game.h"
#include "core/patch.h"
#include "core/vfs.h"
#include "core/base/log.h"
#include "core/base/memory.h"

#include "version.h"

#include <time.h>

static bool m_GameInitialized;

static void M_SetDate(void)
{
    char date[6] = "01/01";
    const time_t now = time(nullptr);
    const struct tm *const local = localtime(&now);
    if (local != nullptr) {
        strftime(date, sizeof(date), "%m/%d", local);
    }
    ResultScreen_SetDate(date);
}

// Keeps the demo replays of the title for playback, as the saves keep the
// user's replays. The original plays no demo whose file is missing.
static void M_KeepDemos(void)
{
    for (int32_t i = 0; i < TITLE_DEMO_COUNT; i++) {
        const char *const path = g_DemoReplayPaths[i];
        READER reader;
        if (!Result_Log(
                Vfs_Open(Shell_GetFiles(), path, VFS_IN_MEMORY, &reader),
                LOG_LEVEL_WARNING)) {
            continue;
        }
        const size_t size = Reader_Remaining(&reader);
        uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
        if (Result_Log(Reader_Result(&reader), LOG_LEVEL_WARNING)) {
            ReplayFiles_Add(path, data, size);
        } else if (data != nullptr) {
            Allocator_Free(&g_HeapAllocator, data, size);
        }
        Reader_Close(&reader);
    }
}

static RESULT M_Init(void)
{
    MUST(Files_Mount());
    Files_SetPatches(Shell_GetPatches());
    Translation_Init(Shell_GetPatches(), &g_TranslationGame);
    if (Patch_ChangesGameplay(Shell_GetPatches())) {
        LOG_INFO("The patches change gameplay, so replays are not saved");
        ResultScreen_RefuseReplays(true);
    }
    MUST(Anm_InitText(Translation_GetFont()));
    Note_Init(Shell_GetPatches());
    Result_Log(SoundPlayer_Init(), LOG_LEVEL_WARNING);
    ResultScreen_OpenSaves();
    M_KeepDemos();
    GameManager_Create(&g_GameManager);
    ResultScreen_LoadPlayStats();
    m_GameInitialized = true;
    MUST(Game_Init());
    // Seeds the gameplay RNG from the clock once, outside the ticks, as the
    // original seeds it at start. Replays restore their own seeds.
    Rng_SetSeed(&g_Rng, (uint16_t)time(nullptr));
    return OK;
}

static bool M_Tick(const void *const input)
{
    M_SetDate();
    const CHAIN_RUN warmup = Game_Warmup(input);
    bool running;
    if (warmup == CHAIN_RUN_RESET) {
        Game_Shutdown();
        running = Result_Log(Game_Init(), LOG_LEVEL_ERROR);
    } else {
        running = warmup != CHAIN_RUN_QUIT && Game_Tick(input);
    }
    Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_WARNING);
    ResultScreen_ScanReplays();
    SoundPlayer_Update();
    return running;
}

static void M_Shutdown(void)
{
    if (m_GameInitialized) {
        // The original saves score.dat as it exits.
        ResultScreen_SaveScore();
        Game_Shutdown();
        m_GameInitialized = false;
    }
    Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_WARNING);
    ResultScreen_CloseSaves();
    ReplayFiles_Clear();
    SoundPlayer_Shutdown();
    Note_Shutdown();
    Anm_ShutdownText();
    Translation_Shutdown();
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
