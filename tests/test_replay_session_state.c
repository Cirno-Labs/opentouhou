#include "session_state.h"

#include "test.h"

#include "core/base/memory.h"

static int32_t m_StartCalls;
static int32_t m_CalcCalls;

static void M_Start(uint32_t *const row)
{
    m_StartCalls++;
    row[0] = 0x80000000u;
}

static void M_Calc(uint32_t *const row)
{
    m_CalcCalls++;
    row[0] = 0x7fc01234u;
}

static SESSION_STATE *M_State(void)
{
    static const char *const start[] = { "value" };
    static const char *const calc[] = { "value" };
    const SESSION_STATE_GAME game = { .id = "th07",
                                      .start_names = start,
                                      .start_count = 1,
                                      .calc_names = calc,
                                      .calc_count = 1,
                                      .start = M_Start,
                                      .calc = M_Calc };
    m_StartCalls = 0;
    m_CalcCalls = 0;
    return SessionState_Create(&game);
}

TEST(menu_rows_do_not_read_gameplay_statistics)
{
    SESSION_STATE *const state = M_State();
    const CASE_NAMES *const columns = SessionState_Columns(state);
    const int32_t start = Case_FindColumn(columns, "start.value");
    const int32_t calc = Case_FindColumn(columns, "calc.value");
    CHECK(start >= 0 && calc >= 0 && start != calc);
    SESSION_POINT point = { .frame = 0,
                            .scene = 1,
                            .requested_scene = 1,
                            .rng_seed = 123,
                            .rng_generated = 4 };
    const uint32_t *row = SessionState_Sample(state, &point);
    CHECK(row[start] == 0 && row[calc] == 0);
    CHECK(m_StartCalls == 0 && m_CalcCalls == 0);
    CHECK(row[Case_FindColumn(columns, "rng_seed")] == 123);
    point.gameplay = true;
    row = SessionState_Sample(state, &point);
    CHECK(row[start] == 0x80000000u && row[calc] == 0x7fc01234u);
    CHECK(m_StartCalls == 1 && m_CalcCalls == 1);
    point.gameplay = false;
    row = SessionState_Sample(state, &point);
    CHECK(row[start] == 0 && row[calc] == 0);
    CHECK(m_StartCalls == 1 && m_CalcCalls == 1);
    SessionState_Free(state);
}

TEST(scores_are_sampled_after_gameplay_without_gameplay_callbacks)
{
    SESSION_STATE *const state = M_State();
    const CASE_NAMES *const columns = SessionState_Columns(state);
    const int32_t score = Case_FindColumn(columns, "score");
    const int32_t shown = Case_FindColumn(columns, "shown_score");
    CHECK(score >= 0 && shown >= 0);
    SESSION_POINT point = { .score_valid = true,
                            .score = 123456,
                            .shown_score = 123400 };
    const uint32_t *row = SessionState_Sample(state, &point);
    CHECK(row[score] == 123456 && row[shown] == 123400);
    CHECK(m_StartCalls == 0 && m_CalcCalls == 0);
    point.score_valid = false;
    row = SessionState_Sample(state, &point);
    CHECK(row[score] == 0 && row[shown] == 0);
    SessionState_Free(state);
}

TEST(state_traces_keep_multiple_calcs_of_one_input_frame)
{
    SESSION_STATE *const state = M_State();
    WRITER output;
    Writer_OpenBuffer(&output, &g_HeapAllocator, "session state");
    CASE_WRITER *const writer = SessionState_OpenWriter(state, &output);
    SESSION_POINT point = { .scene = 1 };
    Case_WriteRow(writer, SessionState_Sample(state, &point));
    point.gameplay = true;
    Case_WriteRow(writer, SessionState_Sample(state, &point));
    point.frame = 1;
    point.gameplay = false;
    Case_WriteRow(writer, SessionState_Sample(state, &point));
    CHECK(Result_Ignore(Case_CloseWriter(writer)));
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&output, &size);
    CHECK(Result_Ignore(Writer_Close(&output)));
    READER input;
    Reader_OpenBuffer(&input, data, size, "session state");
    ARENA arena;
    Arena_Init(&arena, "session state", 1048576);
    CASE_READER *reader;
    CHECK(Result_Ignore(Case_OpenReader(&input, &arena, &reader)));
    const CASE_HEADER *const header = Case_GetHeader(reader);
    CHECK_EQ_STR(header->game_id, "th07");
    CHECK(header->start.count == 0);
    const int32_t frame = Case_FindColumn(&header->calc_names, "session_frame");
    const int32_t start = Case_FindColumn(&header->calc_names, "start.value");
    const int32_t calc = Case_FindColumn(&header->calc_names, "calc.value");
    CHECK(frame >= 0 && start >= 0 && calc >= 0);
    const uint32_t *row = Case_ReadRow(reader);
    CHECK(row != nullptr && row[frame] == 0 && row[start] == 0);
    row = Case_ReadRow(reader);
    CHECK(row != nullptr && row[frame] == 0 && row[start] == 0x80000000u);
    CHECK(row[calc] == 0x7fc01234u);
    row = Case_ReadRow(reader);
    CHECK(row != nullptr && row[frame] == 1 && row[start] == 0);
    CHECK(Case_ReadRow(reader) == nullptr);
    CHECK(Result_Ignore(Case_CloseReader(reader)));
    Reader_Close(&input);
    Arena_Free(&arena);
    Allocator_Free(&g_HeapAllocator, data, size);
    SessionState_Free(state);
}
