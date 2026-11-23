#include "th06/main.h"

#include "shell/game.h"
#include "core/base/log.h"
#include "core/patch.h"
#include "core/vfs.h"

#include "stg/note.h"
#include "stg/replay_files.h"
#include "stg/translation.h"

#include "th06/buttons.h"
#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/scene.h"
#include "th06/screens/title.h"
#include "th06/replay.h"
#include "th06/screens/result.h"
#include "th06/settings.h"
#include "th06/sound.h"
#include "th06/strings.h"

#include "version.h"

#include <time.h>

static bool m_GameInitialized;

static void M_SetDate(void)
{
    char date[9] = "01/01/00";
    const time_t now = time(nullptr);
    const struct tm *const local = localtime(&now);
    if (local != nullptr) {
        strftime(date, sizeof(date), "%m/%d/%y", local);
    }
    ResultScreen_SetDate(date);
}

// Keeps the demo replay of the title for playback, as the saves keep the
// user's replays.
static RESULT M_KeepDemo(void)
{
    READER reader;
    MUST(Vfs_Open(Shell_GetFiles(), TITLE_DEMO_PATH, VFS_IN_MEMORY, &reader));
    const size_t size = Reader_Remaining(&reader);
    uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    MUST(result, TITLE_DEMO_PATH);
    ReplayFiles_Add(TITLE_DEMO_PATH, data, size);
    return OK;
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
    Result_Log(M_KeepDemo(), LOG_LEVEL_WARNING);
    m_GameInitialized = true;
    MUST(Game_Init());
    // Seeds the gameplay RNG from the clock once, outside the ticks, as the
    // original seeds it at start. Replays restore their own seeds.
    Rng_SetSeed(&g_Rng, (uint16_t)time(nullptr));
    g_AsciiText.capture_background = true;
    return OK;
}

static bool M_Tick(const void *const input)
{
    M_SetDate();
    if (Scene_Get() == SCENE_BOOT) {
        if (!Result_Log(
                Anm_LoadSurface(0, "data/title/th06logo.jpg"),
                LOG_LEVEL_ERROR)) {
            return false;
        }
        Anm_CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
        Anm_ReleaseSurface(0);
    }
    const bool running = Game_Tick(input);
    Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_WARNING);
    ResultScreen_ScanReplays();
    SoundPlayer_Update();
    return running;
}

static void M_Shutdown(void)
{
    if (m_GameInitialized) {
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
    .id = "th06",
    .title = "東方紅魔郷 ~ the Embodiment of Scarlet Devil",
    .version = GAME_VERSION,
    .logical_width = 640,
    .logical_height = 480,
    .input_schema = &g_InputSchema,
    .settings_schema = &g_SettingsSchema,
    .settings = &g_Settings,
    .patch_game = &g_PatchGame,
    .data_files = g_FilesArchives,
    .data_file_count = FILES_ARCHIVE_COUNT,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
