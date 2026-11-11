#include "core/app.h"

#include "th06/buttons.h"
#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/screens/result.h"
#include "th06/sound.h"

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

static RESULT M_Init(void)
{
    MUST(Files_Init(App_GetDataDir()));
    MUST(Anm_InitText());
    Result_Log(SoundPlayer_Init(), LOG_LEVEL_WARNING);
    ResultScreen_OpenSaves(App_GetUserDir());
    m_GameInitialized = true;
    MUST(Game_Init());
    g_AsciiText.capture_background = true;
    return OK;
}

static bool M_Tick(const void *const input)
{
    M_SetClock();
    const bool running = Game_Tick(input);
    Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_WARNING);
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
    SoundPlayer_Shutdown();
    Anm_ShutdownText();
    Files_Shutdown();
}

const GAME_DESC g_GameDesc = {
    .id = "th06",
    .title = "東方紅魔郷 ~ the Embodiment of Scarlet Devil",
    .logical_width = 640,
    .logical_height = 480,
    .input_schema = &g_InputSchema,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
