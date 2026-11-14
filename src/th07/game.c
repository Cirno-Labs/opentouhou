#include "th07/game.h"

#include "core/base/log.h"

CHAIN g_Chain;
RNG g_Rng;

RESULT Game_Init(void)
{
    Chain_Init(&g_Chain);
    return OK;
}

bool Game_Tick(void)
{
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
}
