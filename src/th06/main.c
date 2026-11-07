#include "core/app.h"

static APP_RESULT M_Init(void)
{
    return APP_RESULT_OK;
}

static bool M_Tick(const TICK_INPUT *const input, FRAME *const frame)
{
    return true;
}

static void M_Shutdown(void)
{
}

const GAME_DESC g_GameDesc = {
    .id = "th06",
    .title = "東方紅魔郷 ~ the Embodiment of Scarlet Devil",
    .logical_width = 640,
    .logical_height = 480,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
