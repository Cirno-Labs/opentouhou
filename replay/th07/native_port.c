#include "native.h"

#include "harness.h"
#include "native_state.h"

#include "th07/game.h"
#include "th07/main.h"
#include "th07/scene.h"
#include "th07/screens/main.h"
#include "th07/settings.h"

// Runs th07 on the native backend as th07native.

static bool M_Idle(void *const context)
{
    return Scene_Get() == SCENE_TITLE && TitleScene_IsIdle();
}

static void M_Seed(void *const context)
{
    Rng_SetSeed(&g_Rng, 0);
}

// Turns off the fixes, which retail 1.00b lacks.
static void M_Configure(void *const context)
{
    g_Settings.fix_replay_desync = false;
    g_Settings.fix_input_lag = false;
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
            .game = &g_Th07Harness,
            .sample = Th07Native_Sample,
            .idle = M_Idle,
            .seed = M_Seed,
        },
        .game = &g_GameDesc,
        .configure = M_Configure,
        .hook = M_Hook,
    };
    return Native_Main(&port, argc, argv);
}
