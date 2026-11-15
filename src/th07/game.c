#include "th07/game.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/scene.h"

#include "core/base/log.h"
#include "core/base/memory.h"

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;
float g_GameSpeed = 1.0f;
TH07_INPUT g_Input;

RESULT Game_Init(void)
{
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    g_GameSpeed = 1.0f;
    MUST(Scene_Register());
    MUST(Anm_Load(0, "data/text.anm", 0x700));
    return AsciiText_Register();
}

bool Game_Tick(const TH07_INPUT *const input)
{
    g_Input = *input;
    Chain_RunDraw(&g_Chain);
    const CHAIN_RUN run = Chain_RunCalc(&g_Chain);
    if (run == CHAIN_RUN_RESET) {
        Game_Shutdown();
        return Result_Log(Game_Init(), LOG_LEVEL_ERROR);
    }
    return run != CHAIN_RUN_QUIT;
}

void Game_Shutdown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
}
