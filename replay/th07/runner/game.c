#include "state.h"

#include "../../core/runner.h"
#include "../fields.h"
#include "replay.h"

#include "core/io/reader.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screens/main.h"
#include "th07/settings.h"

#include <stdio.h>
#include <string.h>

static const RUNNER_FIELD m_StartFields[] = { TH07_START_FIELDS(RUNNER_FIELD) };
static const RUNNER_FIELD m_CalcFields[] = { TH07_CALC_FIELDS(RUNNER_FIELD) };

// Creates GameManager before the game, as th07's main does once at start.
static RESULT M_Init(void)
{
    g_Settings.fix_replay_desync = false;
    GameManager_Create(&g_GameManager);
    return Game_Init();
}

static bool M_Tick(const void *const input)
{
    return Game_Tick(input);
}

static const char *M_Basename(const char *const path)
{
    const char *name = strrchr(path, '/');
    if (name == nullptr) {
        name = strrchr(path, '\\');
    }
    return name != nullptr ? name + 1 : path;
}

static RESULT M_StartReplay(
    const char *const path, const uint8_t *const data, const size_t size,
    RUNNER_REPLAY *const out)
{
    char scratch_path[0x200];
    snprintf(
        scratch_path, sizeof(scratch_path), RUNNER_SCRATCH_PREFIX "%s",
        M_Basename(path));
    Replay_AddFile(scratch_path, data, size);
    READER buffer;
    Reader_OpenBuffer(&buffer, data, size, path);
    REPLAY_FILE *file = nullptr;
    const RESULT result = Replay_Read(&buffer, &file);
    Reader_Close(&buffer);
    MUST(result);
    int32_t first_slot = 0;
    int32_t last_slot = 0;
    for (int32_t i = 1; i <= 7; i++) {
        if (file->stages[i] != nullptr) {
            if (first_slot == 0) {
                first_slot = i;
            }
            last_slot = i;
        }
    }
    if (first_slot == 0) {
        Replay_Free(file);
        return FAIL("replay has no stage");
    }
    REPLAY_SUMMARY summary;
    Replay_Summarize(file, &summary);
    // Playback never reaches the final stage's two closing empty frames.
    const int32_t last_stage_inputs = file->stages[last_slot]->frame_count - 2;
    const int32_t difficulty = file->difficulty;
    Replay_Free(file);
    MUST(TitleScene_SetUpReplay(&summary, scratch_path, first_slot - 1, 0));
    TitleScene_Remove();
    out->last_stage = RunnerReplay_GetStage(last_slot, difficulty);
    out->last_stage_inputs = last_stage_inputs;
    return OK;
}

static void M_Observe(RUNNER_POINT *const out)
{
    *out = (RUNNER_POINT) {
        .stage = g_GameManager.run.stage,
        .in_play = Scene_Get() == SCENE_GAMEPLAY && g_GameManager.mode.playing
            && !g_GameManager.mode.in_demo,
        .replay_ending = Scene_GetRequested() == SCENE_REPLAY_END,
        .at_title = Scene_Get() == SCENE_TITLE,
    };
}

const RUNNER_GAME g_RunnerGame = {
    .id = "th07",
    .program = "th07replay",
    .start_fields = m_StartFields,
    .start_count = TH07_START_COUNT,
    .calc_fields = m_CalcFields,
    .calc_count = TH07_CALC_COUNT,
    .input_schema = &g_InputSchema,
    .end = RUNNER_END_WITH_PLAY,
    .chain = &g_Chain,
    .open_data = Files_Init,
    .close_data = Files_Shutdown,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = Game_Shutdown,
    .start_replay = M_StartReplay,
    .release_replay = Replay_ClearFiles,
    .fill_start = RunnerState_Start,
    .fill_calc = RunnerState_Calc,
    .observe = M_Observe,
};
