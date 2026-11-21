#include "native.h"

#include "files.h"
#include "harness.h"
#include "native_state.h"

#include "th01/main.h"

#include "core/base/utils.h"

// Runs th01 on the native backend as th01native.

static bool M_Idle(void *const context)
{
    TH01_VIEW view;
    Th01_View(&view);
    return view.scene == TH01_SCENE_OPENING
        && view.opening->phase == OPENING_MAIN;
}

// Does nothing: OP's frame_rand starts at 0, and gameplay seeds its RNG
// from it.
static void M_Seed(void *const context)
{
}

static void M_VSync(void *const arg)
{
    const TH01_NATIVE *const native = arg;
    Driver_EndCalc(native->driver);
}

static void M_Hook(void *const context, DRIVER *const driver)
{
    TH01_NATIVE *const native = context;
    native->driver = driver;
    native->buttons = Driver_Buttons(driver, 0);
    g_Th01Hooks = (TH01_HOOKS) { .on_vsync = M_VSync, .arg = native };
}

int main(int argc, char **argv)
{
    TH01_NATIVE native = {};
    const NATIVE_PORT port = {
        .port = {
            .game = &g_Th01Harness,
            .context = &native,
            .sample = Th01Native_Sample,
            .idle = M_Idle,
            .seed = M_Seed,
            .files = g_Th01Files,
            .file_count = ARRAY_SIZE(g_Th01Files),
        },
        .game = &g_GameDesc,
        .hook = M_Hook,
    };
    return Native_Main(&port, argc, argv);
}
