#include "native.h"

#include "harness.h"
#include "native_state.h"

#include "th06/game.h"
#include "th06/main.h"
#include "th06/scene.h"
#include "th06/screens/main.h"

// Runs th06 on the native backend as th06native.

static bool M_Idle(void *const context)
{
    return Scene_Get() == SCENE_TITLE && TitleScene_IsIdle();
}

static void M_Seed(void *const context)
{
    Rng_SetSeed(&g_Rng, 0);
}

static bool M_CalcEnd(void *const arg)
{
    Driver_EndCalc(arg);
    return true;
}

// Hooks the calcs before each tick, since a reset of the game clears them.
static void M_Hook(void *const context, DRIVER *const driver)
{
    g_Chain.on_calc_end = M_CalcEnd;
    g_Chain.on_calc_end_arg = driver;
}

int main(int argc, char **argv)
{
    const NATIVE_PORT port = {
        .port = {
            .game = &g_Th06Harness,
            .sample = Th06Native_Sample,
            .idle = M_Idle,
            .seed = M_Seed,
        },
        .game = &g_GameDesc,
        .hook = M_Hook,
    };
    return Native_Main(&port, argc, argv);
}
