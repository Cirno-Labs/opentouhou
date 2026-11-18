#include "session_compare.h"

#include "runner/compare.h"

#include "core/base/memory.h"

#include <inttypes.h>

struct SESSION_COMPARE {
    ARENA arena;
    CASE_READER *reader;
    RUNNER_COMPARE *compare;
    const RUNNER_GAME *game;
    uint32_t *start;
    uint32_t *calc;
    bool started;
    bool ended;
    bool incomplete;
    bool extra;
};

RESULT SessionCompare_Open(
    READER *const input, const RUNNER_GAME *const game,
    SESSION_COMPARE **const out)
{
    const size_t size = Reader_Size(input);
    FAIL_IF(size > (SIZE_MAX - 1048576) / 32, "case exceeds its size range");
    SESSION_COMPARE *const state = Mem_AllocZ(sizeof(*state));
    Arena_Init(&state->arena, "session case", size * 32 + 1048576);
    state->game = game;
    RESULT result = Case_OpenReader(input, &state->arena, &state->reader);
    if (IS_OK(result)) {
        const RUNNER_OPTIONS options = { .output = stderr };
        result =
            RunnerCompare_Open(state->reader, game, &options, &state->compare);
    }
    if (!IS_OK(result)) {
        if (state->reader != nullptr) {
            Result_Ignore(Case_CloseReader(state->reader));
        }
        Arena_Free(&state->arena);
        Mem_Free(state);
        return result;
    }
    state->start = Mem_AllocArr((size_t)game->start_count, sizeof(uint32_t));
    state->calc = Mem_AllocArr((size_t)game->calc_count, sizeof(uint32_t));
    *out = state;
    return OK;
}

void SessionCompare_Calc(
    SESSION_COMPARE *const state, const bool ready, const bool ended)
{
    if (state->ended || (!state->started && !ready)) {
        return;
    }
    if (ended) {
        state->ended = true;
        state->incomplete = !RunnerCompare_IsDone(state->compare);
        return;
    }
    if (RunnerCompare_IsDone(state->compare)) {
        state->extra = true;
        return;
    }
    if (!state->started) {
        state->game->fill_start(state->start);
    }
    state->game->fill_calc(state->calc);
    SessionCompare_Row(state, state->start, state->calc);
}

void SessionCompare_Row(
    SESSION_COMPARE *const state, const uint32_t *const start,
    const uint32_t *const calc)
{
    if (RunnerCompare_IsDone(state->compare)) {
        state->extra = true;
        return;
    }
    state->started = true;
    RunnerCompare_Calc(state->compare, start, calc);
}

RESULT SessionCompare_Close(
    SESSION_COMPARE *const state, RUNNER_REPORT *const report)
{
    const bool incomplete =
        state->incomplete || !RunnerCompare_IsDone(state->compare);
    const bool started = state->started;
    const bool extra = state->extra;
    RESULT result = RunnerCompare_Close(state->compare, report);
    Mem_Free(state->start);
    Mem_Free(state->calc);
    Arena_Free(&state->arena);
    Mem_Free(state);
    MUST(result);
    FAIL_IF(!started, "session did not record a gameplay calc");
    FAIL_IF(
        report->different, "first difference at calc %" PRIu64,
        report->first_diff);
    FAIL_IF(!report->complete_schema, "case uses an incomplete column schema");
    FAIL_IF(incomplete, "session ended before the case rows were matched");
    FAIL_IF(extra, "session recorded more calcs than the case");
    return OK;
}
