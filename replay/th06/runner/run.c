#include "run.h"

#include "compare.h"
#include "state.h"

#include "core/base/arena.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/replay.h"
#include "th06/scene.h"
#include "th06/screens/main.h"

#include <inttypes.h>
#include <string.h>

typedef struct {
    RUNNER_COMPARE *compare;
    uint64_t calcs;
    int32_t last_stage;
    bool reached_end;
    bool returned_to_title;
} M_RUN;

static bool M_CalcEnd(void *const arg)
{
    M_RUN *const run = arg;
    uint32_t start[TH06_START_COUNT];
    uint32_t calc[TH06_CALC_COUNT];
    RunnerState_Start(start);
    RunnerState_Calc(calc);
    run->calcs++;
    if (Scene_GetRequested() == SCENE_REPLAY_END
        && g_GameManager.run.stage == run->last_stage) {
        run->reached_end = true;
    }
    if (run->reached_end && Scene_Get() == SCENE_TITLE) {
        run->returned_to_title = true;
    }
    return RunnerCompare_Calc(run->compare, start, calc);
}

static RESULT M_StartReplay(const char *const path, int32_t *const last_stage)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const size_t size = Reader_Remaining(&reader);
    uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
    RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    MUST(result);
    Replay_AddFile(path, data, size);
    READER buffer;
    Reader_OpenBuffer(&buffer, data, size, path);
    REPLAY_FILE *file = nullptr;
    result = Replay_Read(&buffer, &file);
    Reader_Close(&buffer);
    if (data != nullptr) {
        Allocator_Free(&g_HeapAllocator, data, size);
    }
    MUST(result);
    int32_t first_stage = 0;
    for (int32_t i = 1; i <= 7; i++) {
        if (file->stages[i] != nullptr) {
            if (first_stage == 0) {
                first_stage = i;
            }
            *last_stage = i;
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
    return OK;
}

static RESULT M_RunGame(
    const char *const replay_path, M_RUN *const run, bool *const initialized)
{
    *initialized = true;
    MUST(Game_Init());
    const TH06_INPUT no_input = {};
    FAIL_IF(!Game_Tick(&no_input), "game stopped before the title scene");
    MUST(M_StartReplay(replay_path, &run->last_stage));
    g_Chain.on_calc_end = M_CalcEnd;
    g_Chain.on_calc_end_arg = run;
    while (!RunnerCompare_IsDone(run->compare)) {
        const uint64_t before = run->calcs;
        const bool running = Game_Tick(&no_input);
        FAIL_IF(
            !running && !RunnerCompare_IsDone(run->compare),
            "game stopped before the case ended");
        FAIL_IF(
            before == run->calcs && !RunnerCompare_IsDone(run->compare),
            "game did not complete a calc");
        FAIL_IF(
            g_Chain.on_calc_end != M_CalcEnd,
            "game reset during replay comparison");
    }
    return OK;
}

RESULT Runner_Run(
    const char *const case_path, const char *const replay_path,
    const RUNNER_OPTIONS *const options, RUNNER_REPORT *const report)
{
    *report = (RUNNER_REPORT) {};
    READER file;
    MUST(Reader_OpenFile(&file, case_path));
    const size_t size = Reader_Size(&file);
    if (size > (SIZE_MAX - 1048576) / 32) {
        Reader_Close(&file);
        return FAIL("case exceeds its size range");
    }
    ARENA arena;
    Arena_Init(&arena, "replay case", size * 32 + 1048576);
    CASE_READER *reader = nullptr;
    RUNNER_COMPARE *compare = nullptr;
    RESULT result = Case_OpenReader(&file, &arena, &reader);
    if (IS_OK(result)) {
        result = RunnerCompare_Open(reader, options, &compare);
    }
    bool initialized = false;
    M_RUN run = { .compare = compare };
    if (IS_OK(result)) {
        if (RunnerCompare_IsDone(compare)) {
            result = FAIL("case has no calc rows");
        } else {
            result = M_RunGame(replay_path, &run, &initialized);
        }
    }
    if (initialized) {
        g_Chain.on_calc_end = nullptr;
        g_Chain.on_calc_end_arg = nullptr;
        Game_Shutdown();
    }
    Replay_ClearFiles();
    if (compare != nullptr) {
        RESULT closed = RunnerCompare_Close(compare, report);
        if (IS_OK(result)) {
            result = closed;
        } else {
            Result_Ignore(closed);
        }
    } else if (reader != nullptr) {
        Result_Ignore(Case_CloseReader(reader));
    }
    report->complete_replay = run.reached_end && run.returned_to_title;
    Reader_Close(&file);
    Arena_Free(&arena);
    if (IS_OK(result)) {
        if (report->different) {
            result =
                FAIL("first difference at calc %" PRIu64, report->first_diff);
        } else if (!report->complete_schema) {
            result = FAIL("matching prefix uses an incomplete column schema");
        } else if (!report->complete_replay) {
            result = FAIL("matching prefix ended before full replay coverage");
        }
    }
    return result;
}
