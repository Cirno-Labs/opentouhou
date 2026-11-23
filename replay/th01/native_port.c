#include "native.h"

#include "files.h"
#include "harness.h"
#include "native_state.h"

#include "th01/main.h"
#include "th01/settings.h"

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

// Applies the case's start values to the session of the first Start, as
// the dosbox port does to the resident block that REIIDEN reads.
static void M_Start(void *const arg)
{
    TH01_NATIVE *const native = arg;
    if (native->started) {
        return;
    }
    native->started = true;
    size_t count;
    const START_VALUE *const starts = Driver_Starts(native->driver, &count);
    bool scene_set = false;
    uint16_t scene_continues = 0;
    for (size_t i = 0; i < count; i++) {
        const uint32_t value = starts[i].value;
        switch (starts[i].field) {
        case TH01_START_stage:
            g_Session.stage_id = (int16_t)value;
            break;
        case TH01_START_route:
            g_Session.route = (uint8_t)value;
            break;
        case TH01_START_difficulty:
            g_Session.settings.difficulty = (int32_t)value;
            break;
        case TH01_START_lives:
            g_Session.lives = (int16_t)value;
            break;
        case TH01_START_bombs:
            g_Session.bombs = (int16_t)value;
            break;
        case TH01_START_score:
            g_Session.score = value;
            break;
        case TH01_START_continues:
            g_Session.continues_total = (int32_t)value;
            break;
        case TH01_START_pellet_speed:
            g_Session.pellet_speed = (int16_t)value;
            break;
        case TH01_START_boss_hp:
            break;
        case TH01_START_scene_continues:
            scene_set = true;
            scene_continues = (uint16_t)value;
            break;
        }
    }
    const size_t scene = (size_t)g_Session.stage_id / 5;
    if (scene_set && scene < ARRAY_SIZE(g_Session.continues_per_scene)) {
        g_Session.continues_per_scene[scene] = scene_continues;
    }
}

// Applies the case's boss_hp, as the dosbox port does after each boss
// setup's HP store.
static void M_Boss(void *const arg, int16_t *const hp)
{
    TH01_NATIVE *const native = arg;
    size_t count;
    const START_VALUE *const starts = Driver_Starts(native->driver, &count);
    for (size_t i = 0; i < count; i++) {
        if (starts[i].field == TH01_START_boss_hp) {
            *hp = (int16_t)starts[i].value;
        }
    }
}

static void M_Hook(void *const context, DRIVER *const driver)
{
    TH01_NATIVE *const native = context;
    native->driver = driver;
    native->buttons = Driver_Buttons(driver, 0);
    g_Th01Hooks = (TH01_HOOKS) { .on_vsync = M_VSync,
                                 .on_start = M_Start,
                                 .on_boss = M_Boss,
                                 .arg = native };
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
