#include "../replay/th06/runner/compare.h"

#include "test.h"

#include "core/base/memory.h"

#include "stg/chain.h"

#include <string.h>

typedef struct {
    ARENA arena;
    READER input;
    CASE_READER *reader;
    RUNNER_COMPARE *compare;
    uint8_t *bytes;
    size_t size;
    FILE *output;
} FIXTURE;

typedef struct {
    RUNNER_COMPARE *compare;
    int32_t calcs;
} RESTART_FIXTURE;

static int32_t m_Breaks;

void RunnerDebug_Break(void)
{
    m_Breaks++;
}

static void M_WriteCase(
    FIXTURE *const fixture, const char *const *const starts,
    const uint32_t *const start, const int32_t start_count,
    const char *const *const names, const int32_t count,
    const uint32_t *const rows, const int32_t row_count)
{
    *fixture = (FIXTURE) {};
    WRITER output;
    Writer_OpenBuffer(&output, &g_HeapAllocator);
    const CASE_HEADER header = {
        .game_id = "th06",
        .info = "synthetic runner test",
        .start_names = { .items = (const char **)starts, .count = start_count },
        .start = { .items = (uint32_t *)start, .count = start_count },
        .calc_names = { .items = (const char **)names, .count = count },
    };
    CASE_WRITER *const writer = Case_OpenWriter(&output, &header);
    for (int32_t i = 0; i < row_count; i++) {
        Case_WriteRow(writer, rows + i * count);
    }
    CHECK(Result_Ignore(Case_CloseWriter(writer)));
    fixture->bytes = Writer_TakeBuffer(&output, &fixture->size);
    CHECK(Result_Ignore(Writer_Close(&output)));
}

static RESULT M_Open(
    FIXTURE *const fixture, const RUNNER_OPTIONS *const options)
{
    Arena_Init(&fixture->arena, "runner comparison test", 1048576);
    Reader_OpenBuffer(&fixture->input, fixture->bytes, fixture->size);
    MUST(Case_OpenReader(&fixture->input, &fixture->arena, &fixture->reader));
    return RunnerCompare_Open(fixture->reader, options, &fixture->compare);
}

static RESULT M_Close(FIXTURE *const fixture, RUNNER_REPORT *const report)
{
    const RESULT result = fixture->compare != nullptr
        ? RunnerCompare_Close(fixture->compare, report)
        : Case_CloseReader(fixture->reader);
    Reader_Close(&fixture->input);
    Arena_Free(&fixture->arena);
    Mem_Free(fixture->bytes);
    return result;
}

static CHAIN_STATUS M_Restart(void *const arg)
{
    RESTART_FIXTURE *const fixture = arg;
    fixture->calcs++;
    return CHAIN_RESTART;
}

static bool M_CalcEnd(void *const arg)
{
    RESTART_FIXTURE *const fixture = arg;
    uint32_t start[TH06_START_COUNT] = {};
    uint32_t calc[TH06_CALC_COUNT] = {};
    calc[TH06_CALC_rank] = (uint32_t)fixture->calcs;
    return RunnerCompare_Calc(fixture->compare, start, calc);
}

TEST(matches_reordered_named_columns_and_checks_start_once)
{
    static const char *const starts[] = { "rank", "score" };
    static const uint32_t initial[] = { 17, 100 };
    static const char *const names[] = { "rank", "buttons", "score" };
    static const uint32_t rows[] = { 17, 1, 100, 18, 3, 200, 19, 0, 300 };
    FIXTURE fixture;
    M_WriteCase(&fixture, starts, initial, 2, names, 3, rows, 3);
    const RUNNER_OPTIONS options = {};
    CHECK(Result_Ignore(M_Open(&fixture, &options)));
    for (int32_t i = 0; i < 3; i++) {
        uint32_t start[TH06_START_COUNT] = {};
        uint32_t actual[TH06_CALC_COUNT] = {};
        start[TH06_START_rank] = i == 0 ? 17 : 99;
        start[TH06_START_score] = i == 0 ? 100 : 999;
        actual[TH06_CALC_rank] = rows[i * 3];
        actual[TH06_CALC_buttons] = rows[i * 3 + 1];
        actual[TH06_CALC_score] = rows[i * 3 + 2];
        CHECK_EQ_INT(RunnerCompare_Calc(fixture.compare, start, actual), i < 2);
    }
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Close(&fixture, &report)));
    CHECK(!report.different);
    CHECK_EQ_INT(report.rows, 3);
    CHECK_EQ_INT(report.start_columns, 2);
    CHECK_EQ_INT(report.calc_columns, 3);
    CHECK(!report.complete_schema);
}

TEST(compares_all_current_columns_and_float_bits)
{
    static const char *const starts[] = { TH06_START_NAMES };
    static const char *const names[] = { TH06_CALC_NAMES };
    uint32_t start[TH06_START_COUNT] = {};
    uint32_t actual[TH06_CALC_COUNT] = {};
    actual[TH06_CALC_player_x] = 0x80000000;
    FIXTURE fixture;
    M_WriteCase(
        &fixture, starts, start, TH06_START_COUNT, names, TH06_CALC_COUNT,
        actual, 1);
    const RUNNER_OPTIONS options = {};
    CHECK(Result_Ignore(M_Open(&fixture, &options)));
    actual[TH06_CALC_player_x] = 0;
    CHECK(!RunnerCompare_Calc(fixture.compare, start, actual));
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Close(&fixture, &report)));
    CHECK(report.complete_schema);
    CHECK(report.different);
    CHECK_EQ_INT(report.differences, 1);
}

TEST(reports_every_first_calc_difference_with_input_context)
{
    static const char *const starts[] = { "rank" };
    static const uint32_t initial[] = { 17 };
    static const char *const names[] = { "score", "buttons", "rank" };
    static const uint32_t rows[] = { 100, 1, 17, 200, 2, 17,
                                     300, 3, 17, 400, 4, 17 };
    FIXTURE fixture;
    M_WriteCase(&fixture, starts, initial, 1, names, 3, rows, 4);
    fixture.output = tmpfile();
    CHECK(fixture.output != nullptr);
    const RUNNER_OPTIONS options = { .output = fixture.output,
                                     .break_on_diff = true,
                                     .has_break_at = true,
                                     .break_at = 0 };
    m_Breaks = 0;
    CHECK(Result_Ignore(M_Open(&fixture, &options)));
    uint32_t start[TH06_START_COUNT] = {};
    uint32_t actual[TH06_CALC_COUNT] = {};
    actual[TH06_CALC_buttons] = 1;
    CHECK(!RunnerCompare_Calc(fixture.compare, start, actual));
    CHECK_EQ_INT(m_Breaks, 2);
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Close(&fixture, &report)));
    CHECK_EQ_INT(report.rows, 1);
    CHECK_EQ_INT(report.differences, 3);
    CHECK_EQ_INT(report.first_diff, 0);
    rewind(fixture.output);
    char text[2048] = {};
    CHECK(fread(text, 1, sizeof(text) - 1, fixture.output) > 0);
    CHECK(strstr(text, "first difference at calc 0") != nullptr);
    CHECK(strstr(text, "START.rank") != nullptr);
    CHECK(strstr(text, "score: expected 100, got 0") != nullptr);
    CHECK(strstr(text, "calc 3: 0x0004") != nullptr);
    fclose(fixture.output);
}

TEST(skips_unknown_columns_but_compares_implemented_columns)
{
    static const char *const names[] = { "future_column", "rank" };
    static const uint32_t rows[] = { 999, 16 };
    FIXTURE fixture;
    M_WriteCase(&fixture, nullptr, nullptr, 0, names, 2, rows, 1);
    const RUNNER_OPTIONS options = {};
    CHECK(Result_Ignore(M_Open(&fixture, &options)));
    uint32_t start[TH06_START_COUNT] = {};
    uint32_t actual[TH06_CALC_COUNT] = {};
    actual[TH06_CALC_rank] = 17;
    CHECK(!RunnerCompare_Calc(fixture.compare, start, actual));
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Close(&fixture, &report)));
    CHECK_EQ_INT(report.skipped_columns, 1);
    CHECK_EQ_INT(report.differences, 1);
}

TEST(rejects_cases_without_any_comparable_calc_column)
{
    static const char *const names[] = { "unknown" };
    static const uint32_t rows[] = { 1 };
    FIXTURE fixture;
    M_WriteCase(&fixture, nullptr, nullptr, 0, names, 1, rows, 1);
    const RUNNER_OPTIONS options = {};
    CHECK(!Result_Ignore(M_Open(&fixture, &options)));
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Close(&fixture, &report)));
}

TEST(reports_a_truncated_stream_when_comparison_closes)
{
    static const char *const names[] = { "rank" };
    uint32_t rows[100] = {};
    FIXTURE fixture;
    M_WriteCase(&fixture, nullptr, nullptr, 0, names, 1, rows, 100);
    fixture.size--;
    const RUNNER_OPTIONS options = {};
    CHECK(Result_Ignore(M_Open(&fixture, &options)));
    uint32_t start[TH06_START_COUNT] = {};
    uint32_t actual[TH06_CALC_COUNT] = {};
    for (int32_t i = 0; i < 101 && !RunnerCompare_IsDone(fixture.compare);
         i++) {
        RunnerCompare_Calc(fixture.compare, start, actual);
    }
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
}

TEST(stops_before_the_next_restart_when_the_case_ends)
{
    static const char *const names[] = { "rank" };
    static const uint32_t rows[] = { 1, 2 };
    FIXTURE fixture;
    M_WriteCase(&fixture, nullptr, nullptr, 0, names, 1, rows, 2);
    const RUNNER_OPTIONS options = {};
    CHECK(Result_Ignore(M_Open(&fixture, &options)));
    CHAIN chain;
    Chain_Init(&chain);
    RESTART_FIXTURE restart = { .compare = fixture.compare };
    CHAIN_ELEM element;
    Chain_InitElem(&element, M_Restart, &restart);
    CHECK(Result_Ignore(Chain_AddCalc(&chain, &element, 0)));
    chain.on_calc_end = M_CalcEnd;
    chain.on_calc_end_arg = &restart;
    CHECK_EQ_INT(Chain_RunCalc(&chain), CHAIN_RUN_CONTINUE);
    CHECK_EQ_INT(restart.calcs, 2);
    Chain_Release(&chain);
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Close(&fixture, &report)));
    CHECK(!report.different);
    CHECK_EQ_INT(report.rows, 2);
}
