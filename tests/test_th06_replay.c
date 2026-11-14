#include "th06/replay.h"

#include "test.h"

#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/replay/format.h"

#include "core/base/memory.h"

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;
GAME_MANAGER g_GameManager;

static uint8_t *m_Bytes;
static size_t m_Size;
static int32_t m_Loads;
static bool m_Dialogue;
static bool m_Skippable;
static int32_t m_Calcs;
static int32_t m_CalcEnds;
static CHAIN_ELEM m_InputElem;
static CHAIN_ELEM m_ObserverElem;
static TH06_INPUT m_Input;

RESULT Files_Load(const char *const path, READER *const out)
{
    m_Loads++;
    Reader_OpenBuffer(out, m_Bytes, m_Size, nullptr);
    return OK;
}

bool Gui_IsMsgRunning(void)
{
    return m_Dialogue;
}

bool Gui_IsMsgSkippable(void)
{
    return m_Skippable;
}

static CHAIN_STATUS M_Input(void *const arg)
{
    Buttons_Update(&g_Buttons, &m_Input);
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Observe(void *const arg)
{
    m_Calcs++;
    return CHAIN_CONTINUE;
}

static bool M_CalcEnd(void *const arg)
{
    m_CalcEnds++;
    return true;
}

static void M_SetUp(void)
{
    Replay_Discard();
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    Replay_ClearFiles();
    Mem_Free(m_Bytes);
    m_Bytes = nullptr;
    m_Size = 0;
    m_Loads = 0;
    m_Dialogue = false;
    m_Skippable = false;
    m_Calcs = 0;
    m_CalcEnds = 0;
    m_Input = (TH06_INPUT) {};
    g_Buttons = (BUTTONS) {};
    g_GameManager = (GAME_MANAGER) {
        .run = { .stage = 1, .difficulty = 1, .rng_seed = 0x1234 },
        .player = { .lives = 3,
                    .bombs = 2,
                    .power = 64,
                    .point_items_total = 55,
                    .power_item_combo = 6 },
        .rank = { .value = 19 },
        .mode = { .playing = true, .in_demo = true },
    };
    Rng_SetSeed(&g_Rng, 0x4321);
}

static void M_Fixture(void)
{
    REPLAY_FRAME frames[] = {
        { 0, 0 }, { 0, 0x11 }, { 2, 0x100 }, { 4, 0 }, { 9999999, 0 },
    };
    REPLAY_STAGE first = {
        .score = 7654321,
        .rng_seed = 0x1234,
        .point_items_total = 55,
        .power = 64,
        .lives = 3,
        .bombs = 2,
        .rank = 19,
        .power_item_combo = 6,
        .frames = frames,
        .frame_count = 5,
    };
    REPLAY_STAGE second = first;
    second.score = 12345678;
    second.rng_seed = 0xabcd;
    REPLAY_FILE file = { .shot = 3,
                         .difficulty = 2,
                         .stages = { [1] = &first, [2] = &second } };
    const REPLAY_SAVE_INFO info = { "Test", "11/10/26", 60.0f, 60.0f };
    REPLAY_FORMAT_SAVE prepared;
    CHECK(Result_Ignore(ReplayFormat_Prepare(&info, &prepared)));
    const uint8_t random[4] = { 123, 4, 5, 6 };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Ignore(ReplayFormat_Write(&writer, &file, &prepared, random)));
    m_Bytes = Writer_TakeBuffer(&writer, &m_Size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    Replay_AddFile("synthetic.rpy", m_Bytes, m_Size);
}

static REPLAY_FILE *M_Save(void)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    const REPLAY_SAVE_INFO info = { "Player", "11/10/26", 60.0f, 60.0f };
    CHECK(Result_Ignore(Replay_Save(&writer, &info)));
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    REPLAY_FILE *file = nullptr;
    CHECK(Result_Log(Replay_Read(&reader, &file), LOG_LEVEL_ERROR));
    Mem_Free(bytes);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    return file;
}

TEST(recording_keeps_input_changes_and_masks_live_controls)
{
    M_SetUp();
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, nullptr)));
    g_Buttons.word = 0x1009;
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    g_Buttons.word = 0;
    Chain_RunCalc(&g_Chain);
    REPLAY_FILE *const file = M_Save();
    if (file == nullptr) {
        return;
    }
    const REPLAY_STAGE *const stage = file->stages[1];
    CHECK_EQ_INT(stage->frame_count, 5);
    CHECK_EQ_INT(stage->frames[0].buttons, 0);
    CHECK_EQ_INT(stage->frames[1].frame, 0);
    CHECK_EQ_INT(stage->frames[1].buttons, 1);
    CHECK_EQ_INT(stage->frames[2].frame, 2);
    CHECK_EQ_INT(stage->frames[2].buttons, 0);
    CHECK_EQ_INT(stage->frames[3].frame, 3);
    CHECK_EQ_INT(stage->frames[4].frame, 9999999);
    CHECK_EQ_INT(stage->rng_seed, 0x1234);
    CHECK_EQ_INT(stage->point_items_total, 55);
    CHECK_EQ_INT(stage->power_item_combo, 6);
    CHECK(g_Chain.calc.next == nullptr);
    CHECK(g_Chain.draw.next == nullptr);
    Replay_Free(file);
}

TEST(recording_stops_its_frame_counter_while_not_playing)
{
    M_SetUp();
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, nullptr)));
    g_GameManager.mode.playing = false;
    g_Buttons.word = 1;
    for (int32_t i = 0; i < 20; i++) {
        Chain_RunCalc(&g_Chain);
    }
    g_GameManager.mode.playing = true;
    Chain_RunCalc(&g_Chain);
    REPLAY_FILE *const file = M_Save();
    if (file == nullptr) {
        return;
    }
    CHECK_EQ_INT(file->stages[1]->frames[1].frame, 0);
    CHECK_EQ_INT(file->stages[1]->frames[2].frame, 1);
    Replay_Free(file);
}

TEST(next_stage_records_previous_score_and_save_keeps_both_scores)
{
    M_SetUp();
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, nullptr)));
    g_Buttons.word = 1;
    Chain_RunCalc(&g_Chain);
    Replay_EndStage();
    g_GameManager.score.value = 111;
    g_GameManager.run.stage = 2;
    g_GameManager.run.rng_seed = 0xabcd;
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, nullptr)));
    g_GameManager.score.shown = 222;
    g_GameManager.score.value = 333;
    REPLAY_FILE *const file = M_Save();
    if (file == nullptr) {
        return;
    }
    CHECK_EQ_INT(file->score, 222);
    CHECK_EQ_INT(file->stages[1]->score, 111);
    CHECK_EQ_INT(file->stages[2]->score, 333);
    CHECK_EQ_INT(file->stages[2]->rng_seed, 0xabcd);
    Replay_Free(file);
}

TEST(saving_repeats_stage_termination_and_advances_rng_four_times)
{
    M_SetUp();
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, nullptr)));
    Chain_RunCalc(&g_Chain);
    Replay_EndStage();
    RNG expected = g_Rng;
    Rng_U16InRange(&expected, 128);
    for (int32_t i = 0; i < 3; i++) {
        Rng_U16InRange(&expected, 256);
    }
    REPLAY_FILE *const file = M_Save();
    if (file == nullptr) {
        return;
    }
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    CHECK_EQ_INT(g_Rng.generated, 4);
    CHECK_EQ_INT(file->stages[1]->frame_count, 5);
    CHECK_EQ_INT(file->stages[1]->frames[2].frame, 9999999);
    CHECK_EQ_INT(file->stages[1]->frames[3].frame, 1);
    CHECK_EQ_INT(file->stages[1]->frames[4].frame, 9999999);
    Replay_Free(file);
}

TEST(playback_restores_stage_state_and_previous_stage_score)
{
    M_SetUp();
    M_Fixture();
    g_GameManager.run.stage = 2;
    CHECK(Result_Ignore(Replay_Register(REPLAY_PLAY, "synthetic.rpy")));
    CHECK_EQ_INT(g_GameManager.run.character, 1);
    CHECK_EQ_INT(g_GameManager.run.shot_type, 1);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 2);
    CHECK_EQ_INT(g_GameManager.player.power, 64);
    CHECK_EQ_INT(g_GameManager.rank.value, 19);
    CHECK_EQ_INT(g_GameManager.player.point_items_total, 55);
    CHECK_EQ_INT(g_GameManager.score.value, 7654321);
    CHECK_EQ_INT(g_GameManager.score.shown, 7654321);
    CHECK_EQ_INT(g_Rng.seed, 0xabcd);
    CHECK_EQ_INT(g_Rng.generated, 0);
    CHECK_EQ_INT(m_Loads, 0);
    Replay_EndStage();
    g_GameManager.run.stage = 1;
    CHECK(Result_Ignore(Replay_Register(REPLAY_PLAY, "ignored.rpy")));
    CHECK_EQ_INT(m_Loads, 0);
    CHECK_EQ_INT(g_Rng.seed, 0x1234);
    Replay_Discard();
}

TEST(playback_preserves_live_controls_and_replaces_gameplay_buttons)
{
    M_SetUp();
    M_Fixture();
    CHECK(Result_Ignore(Replay_Register(REPLAY_PLAY, "synthetic.rpy")));
    g_Buttons.word = 0x104a;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_Buttons.word, 0x1019);
    CHECK(g_Buttons.held.shoot);
    CHECK(g_Buttons.held.up);
    CHECK(g_Buttons.held.menu);
    CHECK(g_Buttons.held.confirm);
    CHECK(!g_Buttons.held.bomb);
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_Buttons.word, 0x1108);
    Replay_Discard();
}

TEST(playback_stops_its_frame_counter_while_not_playing)
{
    M_SetUp();
    M_Fixture();
    CHECK(Result_Ignore(Replay_Register(REPLAY_PLAY, "synthetic.rpy")));
    g_GameManager.mode.playing = false;
    for (int32_t i = 0; i < 20; i++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK_EQ_INT(g_Buttons.word, 0);
    g_GameManager.mode.playing = true;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_Buttons.word, 0x11);
    Replay_Discard();
}

TEST(skippable_dialogue_restarts_the_calc_chain_without_drawing)
{
    M_SetUp();
    M_Fixture();
    Chain_InitElem(&m_InputElem, M_Input, nullptr);
    Chain_InitElem(&m_ObserverElem, M_Observe, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&g_Chain, &m_InputElem, 0)));
    CHECK(Result_Ignore(Chain_AddCalc(&g_Chain, &m_ObserverElem, 6)));
    g_Chain.on_calc_end = M_CalcEnd;
    CHECK(Result_Ignore(Replay_Register(REPLAY_PLAY, "synthetic.rpy")));
    m_Dialogue = true;
    m_Skippable = true;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_Calcs, 2);
    CHECK_EQ_INT(m_CalcEnds, 2);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_Calcs, 5);
    CHECK_EQ_INT(m_CalcEnds, 5);
    m_Skippable = false;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_Calcs, 6);
    Replay_Discard();
}

TEST(playback_save_discards_without_writing_or_using_rng)
{
    M_SetUp();
    M_Fixture();
    CHECK(Result_Ignore(Replay_Register(REPLAY_PLAY, "synthetic.rpy")));
    const RNG before = g_Rng;
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Ignore(Replay_Save(&writer, nullptr)));
    CHECK_EQ_INT(Writer_Tell(&writer), 0);
    CHECK_EQ_INT(g_Rng.seed, before.seed);
    CHECK_EQ_INT(g_Rng.generated, before.generated);
    CHECK(g_Chain.calc.next == nullptr);
    CHECK(g_Chain.draw.next == nullptr);
    CHECK(Result_Ignore(Writer_Close(&writer)));
}

TEST(missing_stage_failure_can_be_discarded_and_registered_again)
{
    M_SetUp();
    M_Fixture();
    g_GameManager.run.stage = 3;
    CHECK(!Result_Ignore(Replay_Register(REPLAY_PLAY, "synthetic.rpy")));
    m_Dialogue = true;
    m_Skippable = true;
    CHECK_EQ_INT(Chain_RunCalc(&g_Chain), CHAIN_RUN_CONTINUE);
    Replay_EndStage();
    Replay_Discard();
    CHECK(g_Chain.calc.next == nullptr);
    g_GameManager.run.stage = 1;
    CHECK(Result_Ignore(Replay_Register(REPLAY_PLAY, "synthetic.rpy")));
    Chain_Release(&g_Chain);
    Replay_Discard();
}

TEST(invalid_save_metadata_keeps_recording_and_rng_unchanged_for_retry)
{
    M_SetUp();
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, nullptr)));
    g_Buttons.word = 1;
    Chain_RunCalc(&g_Chain);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    const REPLAY_SAVE_INFO info = { "Name too long", "11/10/26", 60.0f, 60.0f };
    const RNG before = g_Rng;
    CHECK(!Result_Ignore(Replay_Save(&writer, &info)));
    CHECK_EQ_INT(g_Rng.seed, before.seed);
    CHECK_EQ_INT(g_Rng.generated, before.generated);
    CHECK_EQ_INT(Writer_Tell(&writer), 0);
    CHECK(g_Chain.calc.next != nullptr);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    REPLAY_FILE *const file = M_Save();
    if (file == nullptr) {
        return;
    }
    CHECK_EQ_INT(file->stages[1]->frame_count, 4);
    Replay_Free(file);
}

TEST(playback_reads_the_kept_file_and_opens_no_file)
{
    M_SetUp();
    M_Fixture();
    g_GameManager.run.stage = 1;
    CHECK(Result_Ignore(Replay_Register(REPLAY_PLAY, "synthetic.rpy")));
    READER reader;
    Reader_OpenBuffer(&reader, m_Bytes, m_Size, nullptr);
    REPLAY_FILE *file = nullptr;
    CHECK(Result_Ignore(Replay_Read(&reader, &file)));
    CHECK_EQ_INT(g_GameManager.run.difficulty, file->difficulty);
    CHECK_EQ_INT(g_Rng.seed, file->stages[1]->rng_seed);
    CHECK_EQ_INT(g_GameManager.player.lives, file->stages[1]->lives);
    Replay_Free(file);
    CHECK_EQ_INT(m_Loads, 0);
    Replay_Discard();
    Chain_Release(&g_Chain);
}

TEST(playback_of_a_path_not_kept_fails)
{
    M_SetUp();
    M_Fixture();
    const RESULT result = Replay_Register(REPLAY_PLAY, "elsewhere.rpy");
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK_EQ_INT(m_Loads, 0);
    Replay_Discard();
    Chain_Release(&g_Chain);
}

TEST(a_kept_file_is_a_copy_that_a_new_copy_replaces)
{
    const uint8_t first[] = { 1, 2, 3 };
    const uint8_t second[] = { 4 };
    Replay_AddFile("a.rpy", first, sizeof(first));
    Replay_AddFile("b.rpy", second, sizeof(second));
    Replay_AddFile("a.rpy", second, sizeof(second));
    size_t size = 0;
    const uint8_t *data = Replay_FindFile("a.rpy", &size);
    CHECK(data != nullptr && size == 1 && data[0] == 4);
    Replay_RemoveFile("a.rpy");
    CHECK(Replay_FindFile("a.rpy", &size) == nullptr);
    CHECK(Replay_FindFile("b.rpy", &size) != nullptr);
    Replay_ClearFiles();
    CHECK(Replay_FindFile("b.rpy", &size) == nullptr);
}
