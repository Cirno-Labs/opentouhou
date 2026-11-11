#include "th06/game.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/game_manager.h"
#include "th06/scene.h"

#include "core/base/log.h"
#include "core/base/memory.h"

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;
TH06_INPUT g_Input;

RESULT Game_Init(void)
{
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    MUST(Scene_Register());
    return AsciiText_Register();
}

bool Game_Tick(const TH06_INPUT *const input)
{
    g_Input = *input;
    Chain_RunDraw(&g_Chain);
    const CHAIN_RUN run = Chain_RunCalc(&g_Chain);
    Result_Log(Anm_DoPendingCopy(), LOG_LEVEL_WARNING);
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
