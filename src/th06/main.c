#include "core/app.h"

#include "th06/buttons.h"
#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/sound.h"

static bool m_GameInitialized;

static RESULT M_Init(void)
{
    MUST(Files_Init(App_GetDataDir()));
    MUST(Anm_InitText());
    Result_Log(SoundPlayer_Init(), LOG_LEVEL_WARNING);
    m_GameInitialized = true;
    MUST(Game_Init());
    g_AsciiText.capture_background = true;
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
