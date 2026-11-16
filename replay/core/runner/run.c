#include "../runner.h"

#include "compare.h"
#include "shell_null.h"

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/io/reader.h"

#include <inttypes.h>
#include <string.h>

typedef struct {
    const RUNNER_GAME *game;
    RUNNER_COMPARE *compare;
    RUNNER_REPLAY replay;
    uint32_t *start_row;
    uint32_t *calc_row;
    uint64_t calcs; // compared, from the first in play
    uint64_t run_calcs; // every calc, compared or not
    uint32_t tick_calcs; // calcs so far in the tick being run
    uint64_t restarted_calcs;
    int64_t last_stage_calcs;
    bool reached_end;
    bool returned_to_title;
    bool extra_calc_checked; // END_WITH_PLAY: the calc past the last row ran
    bool extra_calc_in_play;
} M_RUN;

// Whether the run has produced every calc that its end rule needs.
static bool M_Finished(const M_RUN *const run)
{
    if (!RunnerCompare_IsDone(run->compare)) {
        return false;
    }
    return run->game->end == RUNNER_END_AT_TITLE || run->extra_calc_checked;
}

static bool M_CalcEnd(void *const arg)
{
    M_RUN *const run = arg;
    const RUNNER_GAME *const game = run->game;
    const bool was_done = RunnerCompare_IsDone(run->compare);
    run->run_calcs++;
    RUNNER_POINT point;
    game->observe(&point);
    if (game->end == RUNNER_END_WITH_PLAY && run->calcs == 0
        && !point.in_play) {
        return true;
    }
    if (run->calcs == 0) {
        game->fill_start(run->start_row);
    }
    game->fill_calc(run->calc_row);
    if (run->tick_calcs > 0) {
        run->restarted_calcs++;
    }
    run->tick_calcs++;
    run->calcs++;

    if (game->end == RUNNER_END_AT_TITLE) {
        if (point.replay_ending && point.stage == run->replay.last_stage) {
            run->reached_end = true;
        }
        if (run->reached_end && point.at_title) {
            run->returned_to_title = true;
        }
    } else if (point.stage == run->replay.last_stage) {
        run->last_stage_calcs++;
    }

    if (was_done) {
        run->extra_calc_checked = true;
        run->extra_calc_in_play = point.in_play;
        return false;
    }
    return RunnerCompare_Calc(run->compare, run->start_row, run->calc_row);
}

static RESULT M_StartReplay(
    const RUNNER_GAME *const game, const char *const path, M_RUN *const run)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const size_t size = Reader_Remaining(&reader);
    uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
    RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    if (IS_OK(result)) {
        result = game->start_replay(path, data, size, &run->replay);
    }
    if (data != nullptr) {
        Allocator_Free(&g_HeapAllocator, data, size);
    }
    return result;
}

static RESULT M_RunLoop(
    const RUNNER_GAME *const game, const char *const replay_path,
    M_RUN *const run, const uint8_t *const input)
{
    const bool first = game->tick(input);
    FAIL_IF(!first, "game stopped before the title scene");
    MUST(M_StartReplay(game, replay_path, run));
    game->chain->on_calc_end = M_CalcEnd;
    game->chain->on_calc_end_arg = run;
    while (!M_Finished(run)) {
        const uint64_t before = run->run_calcs;
        run->tick_calcs = 0;
        const bool running = game->tick(input);
        FAIL_IF(
            !running && !M_Finished(run), "game stopped before the case ended");
        FAIL_IF(
            before == run->run_calcs && !M_Finished(run),
            "game did not complete a calc");
        FAIL_IF(
            game->chain->on_calc_end != M_CalcEnd,
            "game reset during replay comparison");
        if (game->after_tick != nullptr) {
            MUST(game->after_tick());
        }
    }
    return OK;
}

static RESULT M_RunGame(
    const RUNNER_GAME *const game, const char *const replay_path,
    M_RUN *const run, bool *const initialized)
{
    *initialized = true;
    RunnerShell_SetInputSize(game->input_schema->size);
    MUST(game->init());
    uint8_t *const input = Mem_AllocZ(game->input_schema->size);
    const RESULT result = M_RunLoop(game, replay_path, run, input);
    Mem_Free(input);
    return result;
}

RESULT Runner_Run(
    const RUNNER_GAME *const game, const char *const case_path,
    const char *const replay_path, const RUNNER_OPTIONS *const options,
    RUNNER_REPORT *const report)
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
        result = RunnerCompare_Open(reader, game, options, &compare);
    }
    bool initialized = false;
    M_RUN run = {
        .game = game,
        .compare = compare,
        .start_row = compare != nullptr
            ? Mem_AllocArr((size_t)game->start_count, sizeof(uint32_t))
            : nullptr,
        .calc_row = compare != nullptr
            ? Mem_AllocArr((size_t)game->calc_count, sizeof(uint32_t))
            : nullptr,
    };
    if (IS_OK(result)) {
        if (RunnerCompare_IsDone(compare)) {
            result = FAIL("case has no calc rows");
        } else {
            result = M_RunGame(game, replay_path, &run, &initialized);
        }
    }
    if (initialized) {
        game->chain->on_calc_end = nullptr;
        game->chain->on_calc_end_arg = nullptr;
        game->shutdown();
    }
    game->release_replay();
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
    report->restarted_calcs = run.restarted_calcs;
    report->last_stage_calcs = run.last_stage_calcs;
    report->complete_replay = run.reached_end && run.returned_to_title;
    Mem_Free(run.start_row);
    Mem_Free(run.calc_row);
    Reader_Close(&file);
    Arena_Free(&arena);
    if (IS_OK(result)) {
        if (report->different) {
            result =
                FAIL("first difference at calc %" PRIu64, report->first_diff);
        } else if (!report->complete_schema) {
            result = FAIL("matching prefix uses an incomplete column schema");
        } else if (game->end == RUNNER_END_AT_TITLE) {
            if (!report->complete_replay) {
                result =
                    FAIL("matching prefix ended before full replay coverage");
            }
        } else if (run.extra_calc_in_play) {
            result = FAIL("case ended inside play");
        } else if (
            report->last_stage_calcs > 0
            && report->last_stage_calcs != run.replay.last_stage_inputs
            && report->last_stage_calcs != run.replay.last_stage_inputs + 1) {
            result = FAIL(
                "last stage ran %" PRId64 " calcs, want %" PRId32
                " or %" PRId32,
                report->last_stage_calcs, run.replay.last_stage_inputs,
                run.replay.last_stage_inputs + 1);
        }
    }
    return result;
}
