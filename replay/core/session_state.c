#include "session_state.h"

#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

struct SESSION_STATE {
    const char *id;
    int32_t start_count;
    int32_t calc_count;
    void (*start)(uint32_t *row);
    void (*calc)(uint32_t *row);
    CASE_NAMES columns;
    uint32_t *row;
};

static const char *const m_Columns[] = {
    "session_frame", "scene",    "requested_scene", "buttons", "rng_seed",
    "rng_generated", "gameplay", "score_valid",     "score",   "shown_score",
};

SESSION_STATE *SessionState_Create(const SESSION_STATE_GAME *const game)
{
    ASSERT(game != nullptr && game->id != nullptr);
    ASSERT(game->start_count >= 0 && game->calc_count >= 0);
    ASSERT(game->start_count == 0 || game->start != nullptr);
    ASSERT(game->calc_count == 0 || game->calc != nullptr);
    ASSERT(
        game->start_count
        <= INT32_MAX - (int32_t)ARRAY_SIZE(m_Columns) - game->calc_count);
    SESSION_STATE *const state = Mem_AllocZ(sizeof(*state));
    state->id = game->id;
    state->start_count = game->start_count;
    state->calc_count = game->calc_count;
    state->start = game->start;
    state->calc = game->calc;
    Array_Init(&state->columns, &g_HeapAllocator);
    for (size_t i = 0; i < ARRAY_SIZE(m_Columns); i++) {
        Array_Append(&state->columns, m_Columns[i]);
    }
    for (int32_t i = 0; i < game->start_count; i++) {
        Array_Append(
            &state->columns, Str_Format("start.%s", game->start_names[i]));
    }
    for (int32_t i = 0; i < game->calc_count; i++) {
        Array_Append(
            &state->columns, Str_Format("calc.%s", game->calc_names[i]));
    }
    state->row =
        Mem_AllocArrZ((size_t)state->columns.count, sizeof(*state->row));
    return state;
}

void SessionState_Free(SESSION_STATE *const state)
{
    if (state == nullptr) {
        return;
    }
    for (int32_t i = (int32_t)ARRAY_SIZE(m_Columns); i < state->columns.count;
         i++) {
        Mem_Free((void *)state->columns.items[i]);
    }
    Array_Free(&state->columns);
    Mem_Free(state->row);
    Mem_Free(state);
}

const CASE_NAMES *SessionState_Columns(const SESSION_STATE *const state)
{
    return &state->columns;
}

const uint32_t *SessionState_Sample(
    SESSION_STATE *const state, const SESSION_POINT *const point)
{
    uint32_t *const row = state->row;
    row[0] = point->frame;
    row[1] = point->scene;
    row[2] = point->requested_scene;
    row[3] = point->buttons;
    row[4] = point->rng_seed;
    row[5] = point->rng_generated;
    row[6] = point->gameplay;
    row[7] = point->score_valid;
    row[8] = point->score_valid ? point->score : 0;
    row[9] = point->score_valid ? point->shown_score : 0;
    uint32_t *const start = row + ARRAY_SIZE(m_Columns);
    if (point->gameplay) {
        if (state->start_count != 0) {
            state->start(start);
        }
        if (state->calc_count != 0) {
            state->calc(start + state->start_count);
        }
    } else {
        memset(
            start, 0,
            (size_t)(state->start_count + state->calc_count) * sizeof(*start));
    }
    return row;
}

CASE_WRITER *SessionState_OpenWriter(
    const SESSION_STATE *const state, WRITER *const output)
{
    CASE_HEADER header = { .info = "input-driven session",
                           .calc_names = state->columns };
    snprintf(header.game_id, sizeof(header.game_id), "%s", state->id);
    return Case_OpenWriter(output, &header);
}
