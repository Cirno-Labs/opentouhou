#include "core/app.h"

#include "th06/buttons.h"
#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/screens/main.h"
#include "th06/replay.h"
#include "th06/screens/result.h"
#include "th06/settings.h"
#include "th06/sound.h"

#include "version.h"

#include <time.h>

static bool m_GameInitialized;

static void M_SetClock(void)
{
    // TODO: count the frames as the original's frame rate display does.
    // Until then, the result screen and replays show no slowdown.
    RESULT_SCREEN_CLOCK clock = {
        .date = "01/01/00",
        .achieved_frames = 1.0f,
        .expected_frames = 1.0f,
    };
    const time_t now = time(nullptr);
    const struct tm *const local = localtime(&now);
    if (local != nullptr) {
        strftime(clock.date, sizeof(clock.date), "%m/%d/%y", local);
    }
    ResultScreen_SetClock(&clock);
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
    MUST(Files_Init(App_GetDataDir()));
    Files_SetPatches(App_GetPatches());
    MUST(Anm_InitText());
    Result_Log(SoundPlayer_Init(), LOG_LEVEL_WARNING);
    ResultScreen_OpenSaves(App_GetUserDir());
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
    M_SetClock();
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
    Anm_ShutdownText();
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
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
