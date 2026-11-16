#include "compare.h"

#include "debug.h"

#include "core/base/memory.h"
#include "core/base/utils.h"

#include <inttypes.h>
#include <string.h>

#define M_CONTEXT_ROWS 3

struct RUNNER_COMPARE {
    CASE_READER *reader;
    const RUNNER_GAME *game;
    RUNNER_OPTIONS options;
    RUNNER_REPORT report;
    int32_t *start_map;
    int32_t *calc_map;
    const uint32_t *expected;
    uint32_t input_context[M_CONTEXT_ROWS];
    int32_t buttons_column;
    bool done;
};

static int32_t M_FindField(
    const RUNNER_FIELD *const fields, const int32_t count,
    const char *const name)
{
    for (int32_t i = 0; i < count; i++) {
        if (strcmp(fields[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int32_t M_MapColumns(
    RUNNER_COMPARE *const compare, const CASE_NAMES *const names,
    const RUNNER_FIELD *const fields, const int32_t count, int32_t *const map,
    const char *const table)
{
    int32_t compared = 0;
    for (int32_t i = 0; i < names->count; i++) {
        const int32_t field = M_FindField(fields, count, names->items[i]);
        map[i] = field;
        if (map[i] >= 0) {
            compared++;
        } else {
            compare->report.skipped_columns++;
            if (compare->options.output != nullptr) {
                fprintf(
                    compare->options.output, "skipped %s column %s (unknown)\n",
                    table, names->items[i]);
            }
        }
    }
    return compared;
}

static void M_PrintValue(
    FILE *const output, const RUNNER_KIND kind, const uint32_t value)
{
    if (kind == RUNNER_F32) {
        float number;
        memcpy(&number, &value, sizeof(number));
        fprintf(output, "%.9g [0x%08" PRIx32 "]", (double)number, value);
    } else if (kind == RUNNER_I32) {
        int32_t number;
        memcpy(&number, &value, sizeof(number));
        fprintf(output, "%" PRId32, number);
    } else if (kind == RUNNER_HASH) {
        fprintf(output, "0x%08" PRIx32, value);
    } else {
        fprintf(output, "%" PRIu32, value);
    }
}

static void M_PrintDifference(
    RUNNER_COMPARE *const compare, const RUNNER_FIELD *const field,
    const char *const prefix, const uint32_t expected, const uint32_t actual)
{
    FILE *const output = compare->options.output;
    if (compare->report.differences == 0 && output != nullptr) {
        fprintf(
            output, "first difference at calc %" PRIu64 "\n",
            compare->report.rows);
    }
    compare->report.differences++;
    if (output != nullptr) {
        fprintf(output, "  %s%s: expected ", prefix, field->name);
        M_PrintValue(output, field->kind, expected);
        fputs(", got ", output);
        M_PrintValue(output, field->kind, actual);
        fputc('\n', output);
    }
}

static void M_CompareTable(
    RUNNER_COMPARE *const compare, const uint32_t *const expected,
    const uint32_t *const actual, const int32_t *const map, const int32_t count,
    const RUNNER_FIELD *const fields, const char *const prefix)
{
    for (int32_t i = 0; i < count; i++) {
        const int32_t field = map[i];
        if (field >= 0 && expected[i] != actual[field]) {
            M_PrintDifference(
                compare, &fields[field], prefix, expected[i], actual[field]);
        }
    }
}

static void M_PrintInputs(RUNNER_COMPARE *const compare, const uint32_t current)
{
    FILE *const output = compare->options.output;
    if (output == nullptr || compare->buttons_column < 0) {
        return;
    }
    const uint64_t row = compare->report.rows;
    const uint64_t first = row < M_CONTEXT_ROWS ? 0 : row - M_CONTEXT_ROWS;
    fputs("  expected input around the differing calc:\n", output);
    for (uint64_t i = first; i < row; i++) {
        fprintf(
            output, "    calc %" PRIu64 ": 0x%04" PRIx32 "\n", i,
            compare->input_context[i % M_CONTEXT_ROWS]);
    }
    fprintf(output, "  > calc %" PRIu64 ": 0x%04" PRIx32 "\n", row, current);
    for (int32_t i = 1; i <= M_CONTEXT_ROWS; i++) {
        const uint32_t *const next = Case_ReadRow(compare->reader);
        if (next == nullptr) {
            break;
        }
        fprintf(
            output, "    calc %" PRIu64 ": 0x%04" PRIx32 "\n",
            row + (uint64_t)i, next[compare->buttons_column]);
    }
}

RESULT RunnerCompare_Open(
    CASE_READER *const reader, const RUNNER_GAME *const game,
    const RUNNER_OPTIONS *const options, RUNNER_COMPARE **const out)
{
    *out = nullptr;
    const CASE_HEADER *const header = Case_GetHeader(reader);
    FAIL_IF(
        strcmp(header->game_id, game->id) != 0, "case is for %s, not %s",
        header->game_id, game->id);
    RUNNER_COMPARE *const compare = Mem_AllocZ(sizeof(RUNNER_COMPARE));
    compare->reader = reader;
    compare->game = game;
    compare->options = *options;
    compare->start_map =
        Mem_AllocArr((size_t)header->start_names.count, sizeof(int32_t));
    compare->calc_map =
        Mem_AllocArr((size_t)header->calc_names.count, sizeof(int32_t));
    compare->report.start_columns = M_MapColumns(
        compare, &header->start_names, game->start_fields, game->start_count,
        compare->start_map, "START");
    compare->report.calc_columns = M_MapColumns(
        compare, &header->calc_names, game->calc_fields, game->calc_count,
        compare->calc_map, "calc");
    compare->report.complete_schema =
        compare->report.start_columns == game->start_count
        && compare->report.calc_columns == game->calc_count;
    if (compare->report.calc_columns == 0) {
        Mem_Free(compare->start_map);
        Mem_Free(compare->calc_map);
        Mem_Free(compare);
        return FAIL("case has no implemented calc columns");
    }
    compare->buttons_column = Case_FindColumn(&header->calc_names, "buttons");
    compare->expected = Case_ReadRow(reader);
    compare->done = compare->expected == nullptr;
    *out = compare;
    return OK;
}

bool RunnerCompare_Calc(
    RUNNER_COMPARE *const compare, const uint32_t *const start,
    const uint32_t *const calc)
{
    if (compare->done) {
        return false;
    }
    const CASE_HEADER *const header = Case_GetHeader(compare->reader);
    const uint32_t current = compare->buttons_column >= 0
        ? compare->expected[compare->buttons_column]
        : 0;
    FILE *const output = compare->options.output;
    const uint32_t before = compare->report.differences;
    if (compare->options.has_break_at
        && compare->options.break_at == compare->report.rows) {
        RunnerDebug_Break();
    }
    if (output != nullptr) {
        fflush(output);
    }
    if (compare->report.rows == 0) {
        M_CompareTable(
            compare, header->start.items, start, compare->start_map,
            header->start_names.count, compare->game->start_fields, "START.");
    }
    M_CompareTable(
        compare, compare->expected, calc, compare->calc_map,
        header->calc_names.count, compare->game->calc_fields, "");
    if (compare->report.differences != before) {
        compare->report.different = true;
        compare->report.first_diff = compare->report.rows;
        if (compare->options.break_on_diff) {
            if (output != nullptr) {
                fflush(output);
            }
            RunnerDebug_Break();
        }
        M_PrintInputs(compare, current);
        compare->done = true;
    } else {
        compare->input_context[compare->report.rows % M_CONTEXT_ROWS] = current;
        compare->expected = Case_ReadRow(compare->reader);
        compare->done = compare->expected == nullptr;
    }
    compare->report.rows++;
    return !compare->done;
}

bool RunnerCompare_IsDone(const RUNNER_COMPARE *const compare)
{
    return compare->done;
}

RESULT RunnerCompare_Close(
    RUNNER_COMPARE *const compare, RUNNER_REPORT *const report)
{
    *report = compare->report;
    const RESULT result = Case_CloseReader(compare->reader);
    Mem_Free(compare->start_map);
    Mem_Free(compare->calc_map);
    Mem_Free(compare);
    return result;
}
