#include "th07/game.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/scene.h"
#include "th07/screens/result/saves.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/gfx.h"
#include "shell/game.h"
#include "stg/note.h"

RNG g_Rng;
float g_GameSpeed = 1.0f;
TH07_INPUT g_Input;

RESULT Game_Init(void)
{
    Gfx_SetPersistentScreen(true);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init();
    g_GameSpeed = 1.0f;
    MUST(Scene_Register());
    MUST(Anm_Load(0, "data/text.anm", 0x700));
    return AsciiText_Register();
}

CHAIN_RUN Game_Warmup(const TH07_INPUT *const input)
{
    if (Scene_Get() != SCENE_BOOT) {
        return CHAIN_RUN_CONTINUE;
    }
    g_Input = *input;
    for (int32_t frame = 0; frame < 30; frame++) {
        const CHAIN_RUN run = Chain_RunCalc();
        if (run != CHAIN_RUN_CONTINUE) {
            return run;
        }
    }
    return CHAIN_RUN_CONTINUE;
}

bool Game_Tick(const TH07_INPUT *const input)
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
