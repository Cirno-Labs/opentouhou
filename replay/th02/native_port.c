#include "replay/harness/native.h"
#include "replay/harness/state.h"
#include "replay/th02/gameplay_sample.h"
#include "replay/th02/harness.h"
#include "replay/th02/start.h"
#include "th02/main.h"
#include "th02/opening.h"
#include "th02/settings.h"
#include "th02/video.h"
#include "th02/vsync_irq.h"

#include "shell/game/files.h"

#include <string.h>

typedef struct {
    DRIVER *driver;
    uint32_t buttons;
    bool started;
    ARRAY(TH02_CLOCK_SAMPLE) clocks;
} TH02_NATIVE;

static bool M_Idle(void *context)
{
    TH02_VIEW view;
    Th02_View(&view);
    if (view.scene != TH02_SCENE_OPENING) {
        return false;
    }
    const OPENING_SAMPLE opening = Opening_Sample();
    return opening.in_menu && !opening.menu.in_options;
}

static void M_Seed(void *context)
{
}

static void M_Configure(void *context)
{
    g_Th02Settings.fix_bomb_transition_overread = false;
}

// Supplies the date and clock samples that the game's own input schema
// does not cover, from the driver's script or measurement.
static void M_PrepareInput(
    void *const context, DRIVER *const driver, void *const input)
{
    TH02_INPUT *const raw = input;
    raw->date = Driver_Date(driver, raw->date);
    TH02_NATIVE *const native = context;
    for (size_t i = 0; i < raw->clock_count; i++) {
        Driver_RecordClock(
            driver, raw->clocks[i].key, raw->clocks[i].microseconds);
    }
    size_t count;
    const SCRIPT_CLOCK *const clocks = Driver_Clocks(driver, &count);
    Array_Clear(&native->clocks);
    for (size_t i = 0; i < count; i++) {
        Array_Append(
            &native->clocks,
            ((TH02_CLOCK_SAMPLE) { .key = clocks[i].key,
                                   .microseconds = clocks[i].microseconds }));
    }
    raw->clocks = native->clocks.items;
    raw->clock_count = count;
}

static void M_Start(void *const context, TH02_SESSION *const session)
{
    TH02_NATIVE *const native = context;
    if (native->started) {
        return;
    }
    native->started = true;
    size_t count;
    const START_VALUE *const starts = Driver_Starts(native->driver, &count);
    Th02Start_Apply(starts, count, session);
}

static void M_BossHp(
    void *const context, int16_t *const damage, const int16_t threshold)
{
    const TH02_NATIVE *const native = context;
    size_t count;
    const START_VALUE *const starts = Driver_Starts(native->driver, &count);
    Th02Start_ApplyBoss(starts, count, damage, threshold);
}

static void M_SampleMain(const TH02_VIEW *const view, uint32_t *const row)
{
    TH02_GAMEPLAY_SAMPLE sample;
    Th02_SampleGameplay(&sample);
    row[TH02_COLUMN_stage] = view->stage;
    row[TH02_COLUMN_score] = (uint32_t)sample.score;
    row[TH02_COLUMN_score_lives] = (uint32_t)sample.lives;
    row[TH02_COLUMN_score_bombs] = (uint32_t)sample.bombs;
    row[TH02_COLUMN_power] = sample.power;
    row[TH02_COLUMN_rng] = sample.rng;
    row[TH02_COLUMN_rng_cursor] = sample.rng_cursor;
    row[TH02_COLUMN_player_x] = (uint32_t)sample.player_x;
    row[TH02_COLUMN_player_y] = (uint32_t)sample.player_y;
}

static void M_SampleVideo(const bool ending, uint32_t *const row)
{
    const uint8_t *const pages[2] = { g_Video.pages[0], g_Video.pages[1] };
    row[TH02_COLUMN_vram_hash] =
        Th02_HashGraphics(pages, ending ? SIZE_MAX : Th02_LoadedTileSources());
    row[TH02_COLUMN_palette_hash] = State_HashBytes(
        STATE_HASH_INIT, g_Video.palette, sizeof(g_Video.palette));
}

static void M_Sample(void *const context, uint32_t *const row)
{
    const TH02_NATIVE *const native = context;
    memset(row, 0, TH02_COLUMN_COUNT * sizeof(*row));
    row[TH02_COLUMN_font_profile] = TH02_FONT_PROFILE_SHINONOME;
    row[TH02_COLUMN_buttons] = native->buttons;
    TH02_VIEW view;
    Th02_View(&view);
    if (view.scene != TH02_SCENE_OPENING) {
        row[TH02_COLUMN_program] = view.ending_program ? 4 : 3;
        if (view.ending_program) {
            row[TH02_COLUMN_count1] = g_VSync.count1;
            row[TH02_COLUMN_count2] = g_VSync.count2;
        } else {
            M_SampleMain(&view, row);
        }
        M_SampleVideo(view.ending_program, row);
        return;
    }
    const OPENING_SAMPLE opening = Opening_Sample();
    if (opening.logo) {
        row[TH02_COLUMN_program] = 1;
        row[TH02_COLUMN_logo_frame] = (uint32_t)opening.logo_frame;
        return;
    }
    row[TH02_COLUMN_program] = 2;
    row[TH02_COLUMN_count1] = g_VSync.count1;
    row[TH02_COLUMN_count2] = g_VSync.count2;
    row[TH02_COLUMN_frame] = (uint32_t)view.frame;
    row[TH02_COLUMN_selection] = (uint32_t)(int32_t)opening.menu.selection;
    row[TH02_COLUMN_in_options] = opening.menu.in_options;
    row[TH02_COLUMN_idle] = opening.menu.idle_frame;
    row[TH02_COLUMN_main_ready] = opening.menu.main_ready;
    row[TH02_COLUMN_main_input] = opening.menu.main_input;
    row[TH02_COLUMN_options_ready] = opening.menu.options_ready;
    row[TH02_COLUMN_options_input] = opening.menu.options_input;
    row[TH02_COLUMN_difficulty] = (uint32_t)g_Th02Settings.difficulty;
    row[TH02_COLUMN_music] = (uint32_t)g_Th02Settings.music;
    row[TH02_COLUMN_bombs] = (uint32_t)g_Th02Settings.bombs;
    row[TH02_COLUMN_lives] = (uint32_t)g_Th02Settings.lives;
    row[TH02_COLUMN_reduce_effects] = view.reduce_effects;
}

static void M_VSync(void *const arg)
{
    const TH02_NATIVE *const native = arg;
    Driver_EndCalc(native->driver);
}

static void M_Hook(void *const context, DRIVER *const driver)
{
    TH02_NATIVE *const native = context;
    native->driver = driver;
    native->buttons = Driver_Buttons(driver, 0);
    g_Th02Hooks.on_vsync = M_VSync;
    g_Th02Hooks.on_start = M_Start;
    g_Th02Hooks.on_boss = M_BossHp;
    g_Th02Hooks.arg = native;
}

int main(int argc, char **argv)
{
    TH02_NATIVE native = {};
    Array_Init(&native.clocks, &g_HeapAllocator);
    const NATIVE_PORT port = {
        .port = { .game = &g_Th02Harness,
                  .context = &native,
                  .sample = M_Sample,
                  .idle = M_Idle,
                  .seed = M_Seed },
        .game = &g_GameDesc,
        .configure = M_Configure,
        .prepare_input = M_PrepareInput,
        .hook = M_Hook,
    };
    const int result = Native_Main(&port, argc, argv);
    Array_Free(&native.clocks);
    return result == 0 && Th02_Failed() ? 2 : result;
}
