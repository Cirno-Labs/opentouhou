#include "th06/game.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/game_manager.h"
#include "th06/scene.h"
#include "th06/screens/result.h"
#include "th06/time.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/gfx.h"
#include "shell/game.h"
#include "stg/note.h"

RNG g_Rng;
TH06_INPUT g_Input;

RESULT Game_Init(void)
{
    GameTime_Resume();
    Gfx_SetPersistentScreen(true);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init();
    Buttons_ReadSharedFocus();
    GameManager_Create(&g_GameManager);
    MUST(Scene_Register());
    MUST(Anm_Load(0, "data/text.anm", 0x700));
    return AsciiText_Register();
}

bool Game_Tick(const TH06_INPUT *const input)
{
    g_Input = *input;
    float tick_rate;
    if (Shell_GetTickRate(&tick_rate)) {
        ResultScreen_CountFrames(tick_rate);
        AsciiText_SetFrameRate(tick_rate);
    }
    Chain_RunDraw();
    Note_Draw();
    const CHAIN_RUN run = Chain_RunCalc();
    GameTime_Resume();
    Result_Log(Anm_DoPendingCopy(), LOG_LEVEL_WARNING);
    if (run == CHAIN_RUN_RESET) {
        Game_Shutdown();
        return Result_Log(Game_Init(), LOG_LEVEL_ERROR);
    }
    return run != CHAIN_RUN_QUIT;
}

void Game_Shutdown(void)
{
    Chain_Release();
    Anm_Shutdown();
}
