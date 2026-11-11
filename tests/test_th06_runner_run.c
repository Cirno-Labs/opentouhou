#include "../replay/th06/runner.h"

#include "test.h"
#include "test_dir.h"

#include "core/app.h"
#include "core/base/memory.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/replay.h"
#include "th06/scene.h"
#include "th06/screens/main.h"

CHAIN g_Chain;
GAME_MANAGER g_GameManager;

static SCENE m_Current;
static SCENE m_Requested;
static int32_t m_Ticks;
static int32_t m_InitCalls;
static int32_t m_ShutdownCalls;
static int32_t m_Actual;
static int32_t m_LastStage;
static bool m_Finish;
static bool m_Reset;
static int32_t m_SetUpStage;
static int32_t m_TitleRemoved;

RESULT Game_Init(void)
{
    m_InitCalls++;
    Chain_Init(&g_Chain);
    g_GameManager = (GAME_MANAGER) {};
    m_Current = SCENE_BOOT;
    m_Requested = SCENE_NONE;
    m_Ticks = 0;
    m_Actual = 0;
    return OK;
}

bool Game_Tick(const TH06_INPUT *const input)
{
    CHECK_EQ_INT(Buttons_ToWord(input), 0);
    m_Ticks++;
    if (m_Ticks == 1) {
        m_Current = SCENE_TITLE;
        return true;
    }
    if (m_Reset) {
        Chain_Init(&g_Chain);
        return true;
    }
    g_GameManager.run.stage = 1;
    m_Current = m_Requested == SCENE_REPLAY_END ? SCENE_TITLE : SCENE_GAMEPLAY;
    if (m_Current == SCENE_TITLE) {
        m_Requested = SCENE_TITLE;
    } else if (m_Finish && m_Actual == 1) {
        m_Requested = SCENE_REPLAY_END;
    }
    if (g_Chain.on_calc_end != nullptr) {
        g_Chain.on_calc_end(g_Chain.on_calc_end_arg);
    }
    m_Actual++;
    return true;
}

void Game_Shutdown(void)
{
    m_ShutdownCalls++;
}

void Scene_Request(const SCENE scene)
{
    m_Requested = scene;
}

SCENE Scene_Get(void)
{
    return m_Current;
}

SCENE Scene_GetRequested(void)
{
    return m_Requested;
}

RESULT Replay_Read(READER *const reader, REPLAY_FILE **const out)
{
    REPLAY_FILE *const file = Mem_AllocZ(sizeof(REPLAY_FILE));
    file->stages[1] = Mem_AllocZ(sizeof(REPLAY_STAGE));
    file->stages[1]->lives = 3;
    file->stages[1]->bombs = 2;
    if (m_LastStage == 2) {
        file->stages[2] = Mem_AllocZ(sizeof(REPLAY_STAGE));
    }
    *out = file;
    return OK;
}

void Replay_Free(REPLAY_FILE *const file)
{
    Mem_Free(file->stages[1]);
    Mem_Free(file->stages[2]);
    Mem_Free(file);
}

void Replay_Summarize(const REPLAY_FILE *const file, REPLAY_SUMMARY *const out)
{
    *out = (REPLAY_SUMMARY) {
        .lives = file->stages[1]->lives,
        .bombs = file->stages[1]->bombs,
    };
}

RESULT TitleScene_SetUpReplay(
    const REPLAY_SUMMARY *const summary, const char *const path,
    const int32_t stage)
{
    m_SetUpStage = stage;
    g_GameManager.player.lives = (int8_t)summary->lives;
    g_GameManager.player.bombs = (int8_t)summary->bombs;
    Scene_Request(SCENE_GAMEPLAY);
    return OK;
}

void TitleScene_Remove(void)
{
    m_TitleRemoved++;
}

void RunnerState_Start(uint32_t row[TH06_START_COUNT])
{
    memset(row, 0, TH06_START_COUNT * sizeof(uint32_t));
    row[TH06_START_rank] = 17;
}

void RunnerState_Calc(uint32_t row[TH06_CALC_COUNT])
{
    memset(row, 0, TH06_CALC_COUNT * sizeof(uint32_t));
    row[TH06_CALC_rank] = 17;
    row[TH06_CALC_stage_frames] = (uint32_t)m_Actual;
}

static RESULT M_Run(const int32_t rows, RUNNER_REPORT *const report)
{
    m_InitCalls = 0;
    m_ShutdownCalls = 0;
    char *const directory = TestDir_Prepare("run");
    char *const replay = TestDir_Join(directory, "input.rpy");
    char *const case_path = TestDir_Join(directory, "input.otrc");
    WRITER output;
    CHECK(Result_Ignore(Writer_OpenFile(&output, replay)));
    CHECK(Result_Ignore(Writer_Close(&output)));
    CHECK(Result_Ignore(Writer_OpenFile(&output, case_path)));
    const char *starts[] = { TH06_START_NAMES };
    const char *calcs[] = { TH06_CALC_NAMES };
    uint32_t start[TH06_START_COUNT] = {};
    start[TH06_START_rank] = 17;
    const CASE_HEADER header = {
        .game_id = "th06",
        .info = "synthetic driver test",
        .start_names = { .items = starts, .count = TH06_START_COUNT },
        .start = { .items = start, .count = TH06_START_COUNT },
        .calc_names = { .items = calcs, .count = TH06_CALC_COUNT },
    };
    CASE_WRITER *const writer = Case_OpenWriter(&output, &header);
    for (int32_t i = 0; i < rows; i++) {
        uint32_t row[TH06_CALC_COUNT] = {};
        row[TH06_CALC_rank] = 17;
        row[TH06_CALC_stage_frames] = (uint32_t)i;
        Case_WriteRow(writer, row);
    }
    CHECK(Result_Ignore(Case_CloseWriter(writer)));
    CHECK(Result_Ignore(Writer_Close(&output)));
    const RUNNER_OPTIONS options = {};
    const RESULT result = Runner_Run(case_path, replay, &options, report);
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
    m_TitleRemoved = 0;
    m_SetUpStage = -1;
    RUNNER_REPORT report;
    CHECK(Result_Ignore(M_Run(3, &report)));
    CHECK_EQ_INT(m_SetUpStage, 0);
    CHECK_EQ_INT(m_TitleRemoved, 1);
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
    CHECK(!Result_Ignore(M_Run(1, &report)));
    CHECK(!report.different);
    CHECK(!report.complete_replay);
    CHECK_EQ_INT(report.rows, 1);
    CHECK_EQ_INT(m_ShutdownCalls, 1);
}

TEST(ending_before_the_last_recorded_stage_is_incomplete)
{
    m_Finish = true;
    m_Reset = false;
    m_LastStage = 2;
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Run(3, &report)));
    CHECK(!report.different);
    CHECK(!report.complete_replay);
}

TEST(reset_that_clears_the_observer_fails_without_looping)
{
    m_Finish = false;
    m_Reset = true;
    m_LastStage = 1;
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Run(3, &report)));
    CHECK_EQ_INT(report.rows, 0);
    CHECK_EQ_INT(m_Ticks, 2);
    CHECK_EQ_INT(m_ShutdownCalls, 1);
}

TEST(empty_case_does_not_initialize_gameplay)
{
    m_Finish = true;
    m_Reset = false;
    m_LastStage = 1;
    RUNNER_REPORT report;
    CHECK(!Result_Ignore(M_Run(0, &report)));
    CHECK_EQ_INT(m_InitCalls, 0);
    CHECK_EQ_INT(m_ShutdownCalls, 0);
}

TEST(the_runner_reports_full_speed_every_half_second)
{
    int32_t measured = 0;
    for (int32_t i = 1; i <= 3 * APP_TICK_RATE / 2; i++) {
        float rate = 0.0f;
        if (App_GetTickRate(&rate)) {
            measured++;
            CHECK_EQ_INT(i % (APP_TICK_RATE / 2), 0);
            CHECK(rate == (float)APP_TICK_RATE);
        }
    }
    CHECK_EQ_INT(measured, 3);
}
