#include "shell/game.h"
#include "core/base/log.h"
#include "core/patch.h"

#include "th06/buttons.h"
#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/note.h"
#include "th06/screens/main.h"
#include "th06/replay.h"
#include "th06/screens/result.h"
#include "th06/settings.h"
#include "th06/sound.h"
#include "th06/translation.h"

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
    MUST(Files_Load(TITLE_DEMO_PATH, &reader));
    const size_t size = Reader_Remaining(&reader);
    uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    MUST(result, TITLE_DEMO_PATH);
    Replay_AddFile(TITLE_DEMO_PATH, data, size);
    if (data != nullptr) {
        Allocator_Free(&g_HeapAllocator, data, size);
    }
    return OK;
}

static RESULT M_Init(void)
{
    MUST(Files_Init(Shell_GetDataDir()));
    Files_SetPatches(Shell_GetPatches());
    Translation_Init(Shell_GetPatches());
    if (Patch_ChangesGameplay(Shell_GetPatches())) {
        LOG_INFO("The patches change gameplay, so replays are not saved");
        ResultScreen_RefuseReplays(true);
    }
    int32_t font_count;
    const char *const *const fonts = Translation_GetFontFiles(&font_count);
    MUST(Anm_InitText(Translation_GetFont(), fonts, font_count));
    Note_Init(Shell_GetPatches());
    Result_Log(SoundPlayer_Init(), LOG_LEVEL_WARNING);
    ResultScreen_OpenSaves(Shell_GetUserDir());
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
    Replay_ClearFiles();
    SoundPlayer_Shutdown();
    Note_Shutdown();
    Anm_ShutdownText();
    Translation_Shutdown();
    Files_Shutdown();
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
