#include "core/app.h"

#include "th06/buttons.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"

static RESULT M_Init(void)
{
    MUST(Files_Init(App_GetDataDir()));
    MUST(Game_Init());
    g_AsciiText.capture_background = true;
    return OK;
}

static bool M_Tick(const void *const input)
{
    return Game_Tick(input);
}

static void M_Shutdown(void)
{
    Game_Shutdown();
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
