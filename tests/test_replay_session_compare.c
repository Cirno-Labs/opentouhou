#include "../replay/core/session_compare.h"

#include "test.h"

#include "core/base/memory.h"

static const RUNNER_FIELD m_Fields[] = { { "value", RUNNER_U32 } };
static uint32_t m_Value;
static uint32_t m_StartCalls;

static void M_Start(uint32_t *const row)
{
    m_StartCalls++;
    *row = 42;
}

static void M_Calc(uint32_t *const row)
{
    *row = m_Value;
}

static const RUNNER_GAME m_Game = {
    .id = "test",
    .start_fields = m_Fields,
    .start_count = 1,
    .calc_fields = m_Fields,
    .calc_count = 1,
    .fill_start = M_Start,
    .fill_calc = M_Calc,
};

typedef struct {
    READER input;
    uint8_t *bytes;
    size_t size;
    SESSION_COMPARE *compare;
} FIXTURE;

static RESULT M_Open(
    FIXTURE *const fixture, const uint32_t count, const bool complete,
    const bool truncated)
{
    memset(fixture, 0, sizeof(*fixture));
    m_StartCalls = 0;
    WRITER output;
    Writer_OpenBuffer(&output, &g_HeapAllocator, nullptr);
    const char *names[] = { "value" };
    uint32_t start[] = { 42 };
    const CASE_HEADER header = {
        .game_id = "test",
        .info = "synthetic scripted session",
        .start_names = { .items = names, .count = complete ? 1 : 0 },
        .start = { .items = start, .count = complete ? 1 : 0 },
        .calc_names = { .items = names, .count = 1 },
    };
    CASE_WRITER *const writer = Case_OpenWriter(&output, &header);
    for (uint32_t i = 1; i <= count; i++) {
        Case_WriteRow(writer, &i);
    }
    MUST(Case_CloseWriter(writer));
    fixture->bytes = Writer_TakeBuffer(&output, &fixture->size);
    MUST(Writer_Close(&output));
    Reader_OpenBuffer(
        &fixture->input, fixture->bytes, fixture->size - (truncated ? 1 : 0),
        nullptr);
    return SessionCompare_Open(&fixture->input, &m_Game, &fixture->compare);
}

static RESULT M_Close(FIXTURE *const fixture, RUNNER_REPORT *const report)
{
    const RESULT result = SessionCompare_Close(fixture->compare, report);
    Reader_Close(&fixture->input);
    Mem_Free(fixture->bytes);
    return result;
}

TEST(skips_title_calcs_and_samples_start_only_once)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 3, true, false)));
    m_Value = 999;
    SessionCompare_Calc(fixture.compare, false, true);
    for (m_Value = 1; m_Value <= 3; m_Value++) {
        SessionCompare_Calc(fixture.compare, m_Value == 1, false);
    }
    SessionCompare_Calc(fixture.compare, false, true);
    SessionCompare_Calc(fixture.compare, true, false);
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Close(&fixture, &report)));
    CHECK_EQ_INT(report.rows, 3);
    CHECK_EQ_INT(m_StartCalls, 1);
    CHECK(!report.complete_replay);
}

TEST(rejects_extra_gameplay_calcs_after_case_eof)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 2, true, false)));
    for (m_Value = 1; m_Value <= 3; m_Value++) {
        SessionCompare_Calc(fixture.compare, true, false);
    }
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
    CHECK_EQ_INT(report.rows, 2);
}

TEST(rejects_a_script_ending_before_case_eof)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 3, true, false)));
    m_Value = 1;
    SessionCompare_Calc(fixture.compare, true, false);
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
}

TEST(rejects_leaving_gameplay_before_case_eof)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 3, true, false)));
    m_Value = 1;
    SessionCompare_Calc(fixture.compare, true, false);
    SessionCompare_Calc(fixture.compare, false, true);
    m_Value = 2;
    SessionCompare_Calc(fixture.compare, true, false);
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
    CHECK_EQ_INT(report.rows, 1);
}

TEST(rejects_sessions_without_gameplay)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 1, true, false)));
    SessionCompare_Calc(fixture.compare, false, false);
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
}

TEST(rejects_empty_cases)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 0, true, false)));
    SessionCompare_Calc(fixture.compare, true, false);
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
}

TEST(rejects_a_matching_incomplete_schema)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 1, false, false)));
    m_Value = 1;
    SessionCompare_Calc(fixture.compare, true, false);
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
    CHECK(!report.complete_schema);
}

TEST(rejects_a_difference_and_preserves_its_row_number)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 3, true, false)));
    m_Value = 1;
    SessionCompare_Calc(fixture.compare, true, false);
    m_Value = 99;
    SessionCompare_Calc(fixture.compare, true, false);
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
    CHECK(report.different);
    CHECK_EQ_INT(report.first_diff, 1);
}

TEST(rejects_truncated_compressed_streams)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 3, true, true)));
    for (m_Value = 1; m_Value <= 3; m_Value++) {
        SessionCompare_Calc(fixture.compare, true, false);
    }
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Close(&fixture, &report)));
}

static CHAIN_STATUS M_Restart(void *const arg)
{
    m_Value++;
    return m_Value < 3 ? CHAIN_RESTART : CHAIN_CONTINUE;
}

static bool M_CalcEnd(void *const arg)
{
    SessionCompare_Calc(arg, true, false);
    return true;
}

TEST(compares_restarted_calcs_without_stopping_the_chain)
{
    FIXTURE fixture;
    CHECK(Result_Ignore(M_Open(&fixture, 3, true, false)));
    CHAIN chain;
    Chain_Init(&chain);
    CHAIN_ELEM element;
    Chain_InitElem(&element, M_Restart, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&chain, &element, 0)));
    chain.on_calc_end = M_CalcEnd;
    chain.on_calc_end_arg = fixture.compare;
    m_Value = 0;
    CHECK_EQ_INT(Chain_RunCalc(&chain), CHAIN_RUN_CONTINUE);
    CHECK_EQ_INT(m_Value, 3);
    Chain_Release(&chain);
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Close(&fixture, &report)));
    CHECK_EQ_INT(report.rows, 3);
}
