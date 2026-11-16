#include "state.h"

#include "../../core/runner.h"
#include "../fields.h"

#include "core/io/reader.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/replay.h"
#include "th06/scene.h"
#include "th06/screens/main.h"

static const RUNNER_FIELD m_StartFields[] = { TH06_START_FIELDS(RUNNER_FIELD) };
static const RUNNER_FIELD m_CalcFields[] = { TH06_CALC_FIELDS(RUNNER_FIELD) };

static bool M_Tick(const void *const input)
{
    return Game_Tick(input);
}

static RESULT M_StartReplay(
    const char *const path, const uint8_t *const data, const size_t size,
    RUNNER_REPLAY *const out)
{
    Replay_AddFile(path, data, size);
    READER buffer;
    Reader_OpenBuffer(&buffer, data, size, path);
    REPLAY_FILE *file = nullptr;
    RESULT result = Replay_Read(&buffer, &file);
    Reader_Close(&buffer);
    MUST(result);
    int32_t first_stage = 0;
    int32_t last_stage = 0;
    for (int32_t i = 1; i <= 7; i++) {
        if (file->stages[i] != nullptr) {
            if (first_stage == 0) {
                first_stage = i;
            }
            last_stage = i;
        }
    }
    if (first_stage == 0) {
        Replay_Free(file);
        return FAIL("replay has no stage");
    }
    REPLAY_SUMMARY summary;
    Replay_Summarize(file, &summary);
    Replay_Free(file);
    MUST(TitleScene_SetUpReplay(&summary, path, first_stage - 1));
    TitleScene_Remove();
    out->last_stage = last_stage;
    out->last_stage_inputs = 0;
    return OK;
}

static void M_Observe(RUNNER_POINT *const out)
{
    *out = (RUNNER_POINT) {
        .stage = g_GameManager.run.stage,
        .in_play = g_GameManager.mode.playing,
        .replay_ending = Scene_GetRequested() == SCENE_REPLAY_END,
        .at_title = Scene_Get() == SCENE_TITLE,
    };
}

const RUNNER_GAME g_RunnerGame = {
    .id = "th06",
    .program = "th06replay",
    .start_fields = m_StartFields,
    .start_count = TH06_START_COUNT,
    .calc_fields = m_CalcFields,
    .calc_count = TH06_CALC_COUNT,
    .input_schema = &g_InputSchema,
    .end = RUNNER_END_AT_TITLE,
    .chain = &g_Chain,
    .open_data = Files_Init,
    .close_data = Files_Shutdown,
    .init = Game_Init,
    .tick = M_Tick,
    .shutdown = Game_Shutdown,
    .start_replay = M_StartReplay,
    .release_replay = Replay_ClearFiles,
    .fill_start = RunnerState_Start,
    .fill_calc = RunnerState_Calc,
    .observe = M_Observe,
};
