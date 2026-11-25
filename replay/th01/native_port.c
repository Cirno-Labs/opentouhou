#include "native.h"

#include "files.h"
#include "harness.h"
#include "native_state.h"
#include "start.h"

#include "th01/main.h"
#include "th01/opening.h"
#include "th01/settings.h"

#include "core/base/utils.h"

// Runs th01 on the native backend as th01native.

static bool M_Idle(void *const context)
{
    return g_Th01Scene == TH01_SCENE_OPENING && g_Opening.phase == OPENING_MAIN;
}

// Does nothing: OP's frame_rand starts at 0, and gameplay seeds its RNG
// from it.
static void M_Seed(void *const context)
{
}

// Turns off the fixes, which the original lacks.
static void M_Configure(void *const context)
{
    g_Settings.fix_debouncing = false;
}

static void M_VSync(void *const arg)
{
    const TH01_NATIVE *const native = arg;
    Driver_EndCalc(native->driver);
}

static void M_Start(void *const arg)
{
    TH01_NATIVE *const native = arg;
    if (native->started) {
        return;
    }
    native->started = true;
    size_t count;
    const START_VALUE *const starts = Driver_Starts(native->driver, &count);
    Th01Start_Apply(starts, count);
}

static void M_Boss(void *const arg, int16_t *const hp)
{
    TH01_NATIVE *const native = arg;
    size_t count;
    const START_VALUE *const starts = Driver_Starts(native->driver, &count);
    Th01Start_ApplyBoss(starts, count, hp);
}

static void M_Hook(void *const context, DRIVER *const driver)
{
    TH01_NATIVE *const native = context;
    native->driver = driver;
    native->buttons = Driver_Buttons(driver, 0);
    Th01Hooks_Set((TH01_HOOKS) { .on_vsync = M_VSync,
                                 .on_start = M_Start,
                                 .on_boss = M_Boss,
                                 .arg = native });
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
        .configure = M_Configure,
        .hook = M_Hook,
    };
    return Native_Main(&port, argc, argv);
}
