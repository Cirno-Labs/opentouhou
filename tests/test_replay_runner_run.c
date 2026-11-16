#include "../replay/core/runner.h"

#include "test.h"
#include "test_dir.h"

#include "shell/game.h"
#include "core/base/memory.h"

#include <string.h>

enum { START_rank, START_COUNT };
enum { CALC_rank, CALC_counter, CALC_COUNT };

static const RUNNER_FIELD m_StartFields[] = { { "rank", RUNNER_I32 } };
static const RUNNER_FIELD m_CalcFields[] = {
    { "rank", RUNNER_I32 },
    { "counter", RUNNER_U32 },
};

typedef enum {
    M_SCENE_NONE,
    M_SCENE_TITLE,
    M_SCENE_GAMEPLAY,
    M_SCENE_REPLAY_END
} M_SCENE;

static CHAIN m_Chain;
static M_SCENE m_Current;
static M_SCENE m_Requested;
static int32_t m_Ticks;
static int32_t m_InitCalls;
static int32_t m_ShutdownCalls;
static int32_t m_ReleaseCalls;
static int32_t m_Actual;
static int32_t m_Stage;
static int32_t m_LastStage;
static int32_t m_N;
static int32_t m_SetUpStage;
static bool m_Finish;
static bool m_Reset;
static int32_t m_StopAfterTick;
static int32_t m_CalcsPerTick;
static int32_t m_PlayCalcs;
static int32_t m_LeadCalcs; // WITH_PLAY: title calcs before play
static bool m_ExtraSameStage;
static int32_t m_PlayStage; // WITH_PLAY: 0 plays the last stage
static RUNNER_END m_Mode;
static INPUT_SCHEMA m_Schema;

static RESULT M_Init(void)
{
    m_InitCalls++;
    Chain_Init(&m_Chain);
    m_Current = M_SCENE_NONE;
    m_Requested = M_SCENE_NONE;
    m_Ticks = 0;
    m_Actual = 0;
    m_Stage = 0;
    return OK;
}

static void M_RunCalc(void)
{
    if (m_Chain.on_calc_end != nullptr) {
        m_Chain.on_calc_end(m_Chain.on_calc_end_arg);
    }
}

static bool M_Tick(const void *const input)
{
    CHECK(((const uint8_t *)input)[0] == 0);
    m_Ticks++;
    if (m_Ticks == 1) {
        m_Current = M_SCENE_TITLE;
        return true;
    }
    if (m_Reset) {
        Chain_Init(&m_Chain);
        return true;
    }
    if (m_Mode == RUNNER_END_WITH_PLAY) {
        if (m_StopAfterTick != 0 && m_Ticks > m_StopAfterTick) {
            return false;
        }
        if (m_LeadCalcs > 0) {
            m_LeadCalcs--;
            M_RunCalc();
            return true;
        }
        for (int32_t i = 0; i < m_CalcsPerTick; i++) {
            if (m_Actual >= m_PlayCalcs + 1) {
                break;
            }
            m_Actual++;
            m_Stage = (m_Actual <= m_PlayCalcs || m_ExtraSameStage)
                ? (m_PlayStage != 0 ? m_PlayStage : m_LastStage)
                : m_LastStage + 1;
            m_Current =
                m_Actual <= m_PlayCalcs ? M_SCENE_GAMEPLAY : M_SCENE_TITLE;
            M_RunCalc();
        }
        return true;
    }
    m_Stage = m_LastStage;
    m_Current =
        m_Requested == M_SCENE_REPLAY_END ? M_SCENE_TITLE : M_SCENE_GAMEPLAY;
    if (m_Current == M_SCENE_TITLE) {
        m_Requested = M_SCENE_TITLE;
    } else if (m_Finish && m_Actual == 1) {
        m_Requested = M_SCENE_REPLAY_END;
    }
    M_RunCalc();
    m_Actual++;
    return true;
}

static void M_Shutdown(void)
{
    m_ShutdownCalls++;
}

static RESULT M_StartReplay(
    const char *const path, const uint8_t *const data, const size_t size,
    RUNNER_REPLAY *const out)
{
    (void)path;
    (void)data;
    (void)size;
    m_SetUpStage = 0;
    m_Requested = M_SCENE_GAMEPLAY;
    out->last_stage = m_LastStage;
    out->last_stage_inputs = m_N;
    return OK;
}

static void M_ReleaseReplay(void)
{
    m_ReleaseCalls++;
}

static void M_FillStart(uint32_t *const row)
{
    memset(row, 0, START_COUNT * sizeof(uint32_t));
    row[START_rank] = 17;
}

static void M_FillCalc(uint32_t *const row)
{
    memset(row, 0, CALC_COUNT * sizeof(uint32_t));
    row[CALC_rank] = 17;
    row[CALC_counter] = (uint32_t)m_Actual;
}

static void M_Observe(RUNNER_POINT *const out)
{
    *out = (RUNNER_POINT) {
        .stage = m_Stage,
        .in_play = m_Current == M_SCENE_GAMEPLAY,
        .replay_ending = m_Requested == M_SCENE_REPLAY_END,
        .at_title = m_Current == M_SCENE_TITLE,
    };
}

static RESULT M_OpenData(const char *const data_dir)
{
    (void)data_dir;
    return OK;
}

static void M_CloseData(void)
{
}

static const RUNNER_GAME m_Game = {
    .id = "test",
    .program = "testrunner",
    .start_fields = m_StartFields,
    .start_count = START_COUNT,
    .calc_fields = m_CalcFields,
    .calc_count = CALC_COUNT,
    .input_schema = &m_Schema,
    .chain = &m_Chain,
    .open_data = M_OpenData,
    .close_data = M_CloseData,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
    .start_replay = M_StartReplay,
    .release_replay = M_ReleaseReplay,
    .fill_start = M_FillStart,
    .fill_calc = M_FillCalc,
    .observe = M_Observe,
};

static RESULT M_Run(
    const RUNNER_END end, const int32_t rows, const uint32_t *const counters,
    RUNNER_REPORT *const report)
{
    m_InitCalls = 0;
    m_ShutdownCalls = 0;
    m_ReleaseCalls = 0;
    m_Mode = end;
    memset(&m_Schema, 0, sizeof(m_Schema));
    m_Schema.size = 1;
    m_Schema.players = 1;
    RUNNER_GAME game = m_Game;
    game.end = end;
    char *const directory = TestDir_Prepare("run");
    char *const replay = TestDir_Join(directory, "input.rpy");
    char *const case_path = TestDir_Join(directory, "input.otrc");
    WRITER output;
    CHECK(Result_Ignore(Writer_OpenFile(&output, replay)));
    CHECK(Result_Ignore(Writer_Close(&output)));
    CHECK(Result_Ignore(Writer_OpenFile(&output, case_path)));
    const char *starts[] = { "rank" };
    const char *calcs[] = { "rank", "counter" };
    uint32_t start[START_COUNT] = { [START_rank] = 17 };
    const CASE_HEADER header = {
        .game_id = "test",
        .info = "synthetic driver test",
        .start_names = { .items = starts, .count = START_COUNT },
        .start = { .items = start, .count = START_COUNT },
        .calc_names = { .items = calcs, .count = CALC_COUNT },
    };
    CASE_WRITER *const writer = Case_OpenWriter(&output, &header);
    for (int32_t i = 0; i < rows; i++) {
        uint32_t row[CALC_COUNT] = { [CALC_rank] = 17,
                                     [CALC_counter] = counters != nullptr
                                         ? counters[i]
                                         : (uint32_t)i };
        Case_WriteRow(writer, row);
    }
    CHECK(Result_Ignore(Case_CloseWriter(writer)));
    CHECK(Result_Ignore(Writer_Close(&output)));
    const RUNNER_OPTIONS options = {};
    const RESULT result =
        Runner_Run(&game, case_path, replay, &options, report);
    Mem_Free(case_path);
    Mem_Free(replay);
    Mem_Free(directory);
    return result;
}

TEST(full_coverage_uses_replay_input_and_post_first_calc_start)
{
    m_Finish = true;
    m_Reset = false;
    m_LastStage = 1;
    m_ReleaseCalls = 0;
    m_SetUpStage = -1;
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Run(RUNNER_END_AT_TITLE, 3, nullptr, &report)));
    CHECK_EQ_INT(m_SetUpStage, 0);
    CHECK_EQ_INT(m_ReleaseCalls, 1);
    CHECK(report.complete_schema);
    CHECK(report.complete_replay);
    CHECK_EQ_INT(report.rows, 3);
    CHECK_EQ_INT(m_InitCalls, 1);
    CHECK_EQ_INT(m_ShutdownCalls, 1);
}

TEST(clean_eof_before_replay_end_is_a_matching_prefix_failure)
{
    m_Finish = true;
    m_Reset = false;
    m_LastStage = 1;
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Run(RUNNER_END_AT_TITLE, 1, nullptr, &report)));
    CHECK(!report.different);
    CHECK(!report.complete_replay);
    CHECK_EQ_INT(report.rows, 1);
    CHECK_EQ_INT(m_ShutdownCalls, 1);
}

TEST(reset_that_clears_the_observer_fails_without_looping)
{
    m_Finish = false;
    m_Reset = true;
    m_LastStage = 1;
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Run(RUNNER_END_AT_TITLE, 3, nullptr, &report)));
    CHECK_EQ_INT(report.rows, 0);
    CHECK_EQ_INT(m_Ticks, 2);
    CHECK_EQ_INT(m_ShutdownCalls, 1);
    m_Reset = false;
}

TEST(empty_case_does_not_initialize_gameplay)
{
    m_Finish = true;
    m_Reset = false;
    m_LastStage = 1;
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Run(RUNNER_END_AT_TITLE, 0, nullptr, &report)));
    CHECK_EQ_INT(m_InitCalls, 0);
    CHECK_EQ_INT(m_ShutdownCalls, 0);
}

TEST(with_play_passes_at_exactly_n_calcs_on_the_last_stage)
{
    static const uint32_t counters[] = { 1, 2, 3, 4, 5 };
    m_Reset = false;
    m_LastStage = 1;
    m_N = 5;
    m_PlayCalcs = 5;
    m_ExtraSameStage = false;
    m_StopAfterTick = 0;
    m_CalcsPerTick = 1;
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Run(RUNNER_END_WITH_PLAY, 5, counters, &report)));
    CHECK_EQ_INT(report.last_stage_calcs, 5);
}

TEST(with_play_skips_the_calcs_before_play)
{
    static const uint32_t counters[] = { 1, 2, 3, 4, 5 };
    m_Reset = false;
    m_LastStage = 1;
    m_N = 5;
    m_PlayCalcs = 5;
    m_LeadCalcs = 3;
    m_ExtraSameStage = false;
    m_StopAfterTick = 0;
    m_CalcsPerTick = 1;
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Run(RUNNER_END_WITH_PLAY, 5, counters, &report)));
    CHECK_EQ_INT(report.rows, 5);
    CHECK_EQ_INT(report.last_stage_calcs, 5);
}

TEST(with_play_passes_when_play_ends_before_the_last_stage)
{
    static const uint32_t counters[] = { 1, 2, 3 };
    m_Reset = false;
    m_LastStage = 3;
    m_PlayStage = 2;
    m_N = 5;
    m_PlayCalcs = 3;
    m_ExtraSameStage = false;
    m_StopAfterTick = 0;
    m_CalcsPerTick = 1;
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Run(RUNNER_END_WITH_PLAY, 3, counters, &report)));
    CHECK_EQ_INT(report.last_stage_calcs, 0);
    m_PlayStage = 0;
}

TEST(with_play_passes_at_n_plus_one_calcs_on_the_last_stage)
{
    static const uint32_t counters[] = { 1, 2, 3, 4, 5 };
    m_Reset = false;
    m_LastStage = 1;
    m_N = 5;
    m_PlayCalcs = 5;
    m_ExtraSameStage = true;
    m_StopAfterTick = 0;
    m_CalcsPerTick = 1;
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Run(RUNNER_END_WITH_PLAY, 5, counters, &report)));
    CHECK_EQ_INT(report.last_stage_calcs, 6);
}

TEST(with_play_fails_past_n_plus_one_calcs_on_the_last_stage)
{
    static const uint32_t counters[] = { 1, 2, 3, 4, 5 };
    m_Reset = false;
    m_LastStage = 1;
    m_N = 3;
    m_PlayCalcs = 5;
    m_ExtraSameStage = true;
    m_StopAfterTick = 0;
    m_CalcsPerTick = 1;
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Run(RUNNER_END_WITH_PLAY, 5, counters, &report)));
    CHECK_EQ_INT(report.last_stage_calcs, 6);
}

TEST(with_play_fails_when_the_game_stops_early)
{
    static const uint32_t counters[] = { 1, 2, 3, 4, 5 };
    m_Reset = false;
    m_LastStage = 1;
    m_N = 5;
    m_PlayCalcs = 5;
    m_ExtraSameStage = false;
    m_StopAfterTick = 3;
    m_CalcsPerTick = 1;
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Run(RUNNER_END_WITH_PLAY, 5, counters, &report)));
    m_StopAfterTick = 0;
}

TEST(with_play_counts_restarted_calcs_within_a_tick)
{
    static const uint32_t counters[] = { 1, 2, 3, 4, 5, 6 };
    m_Reset = false;
    m_LastStage = 1;
    m_N = 6;
    m_PlayCalcs = 6;
    m_ExtraSameStage = false;
    m_StopAfterTick = 0;
    m_CalcsPerTick = 2;
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Run(RUNNER_END_WITH_PLAY, 6, counters, &report)));
    CHECK_EQ_INT(report.restarted_calcs, 3);
    m_CalcsPerTick = 1;
}

TEST(the_runner_reports_full_speed_every_half_second)
{
    int32_t measured = 0;
    for (int32_t i = 1; i <= 3 * SHELL_TICK_RATE / 2; i++) {
        float rate = 0.0f;
        if (Shell_GetTickRate(&rate)) {
            measured++;
            CHECK_EQ_INT(i % (SHELL_TICK_RATE / 2), 0);
            CHECK(rate == (float)SHELL_TICK_RATE);
        }
    }
    CHECK_EQ_INT(measured, 3);
}
