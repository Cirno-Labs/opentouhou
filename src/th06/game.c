#include "th06/game.h"

CHAIN g_Chain;
BUTTONS g_Buttons;
TH06_INPUT g_Input;

RESULT Game_Init(void)
{
    Chain_Init(&g_Chain);
    return OK;
}

bool Game_Tick(const TH06_INPUT *const input)
{
    g_Input = *input;
    Chain_RunDraw(&g_Chain);
    return Chain_RunCalc(&g_Chain) != CHAIN_RUN_QUIT;
}

void Game_Shutdown(void)
{
    Chain_Release(&g_Chain);
}
