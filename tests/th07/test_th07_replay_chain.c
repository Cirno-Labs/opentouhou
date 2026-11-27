#include "tests/test.h"
#include "tests/th07/replay_lzss_literals.h"
#include "th07/enemy.h"
#include "th07/exe_identity.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/screens/result/saves.h"
#include "th07/settings.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "stg/replay_files.h"

#include <stdio.h>
#include <string.h>

#define M_PLAIN_SIZE 0x54
#define M_HEADER_SIZE 0xe8
#define M_STAGE_HEADER_SIZE 0x2c
#define M_MAX_FRAMES 128
#define M_MAX_FPS 8
#define M_MAX_SIZE 0x4000
#define M_CHERRY_OFFSET 54321
#define M_PATH "replay/th7_01.rpy"

RNG g_Rng;

// Holds what M_Encode writes for a slot; a slot without frames is left out.
typedef struct {
    int32_t score;
    int32_t cherry_plus;
    uint16_t buttons[M_MAX_FRAMES];
    uint16_t events[M_MAX_FRAMES];
    int32_t frame_count;
    uint8_t fps[M_MAX_FPS];
    int32_t fps_count;
} M_SLOT;

typedef struct {
    uint8_t difficulty;
    M_SLOT slots[8];
} M_SPEC;

static uint8_t m_Image[M_MAX_SIZE];
static uint8_t m_File[M_MAX_SIZE];
static int32_t m_BorderStarts;
static bool m_Dialogue;
static bool m_Skippable;
static bool m_Boss;
static int32_t m_Calcs;
static uint16_t m_SeenPlayWord;
static CHAIN_ELEM m_ObserverElem;

void Player_StartBorder(void)
{
    m_BorderStarts++;
}

bool Gui_IsMsgRunning(void)
{
    return m_Dialogue;
}

bool Gui_IsMsgSkippable(void)
{
    return m_Skippable;
}

bool Enemy_IsBossActive(void)
{
    return m_Boss;
}

static uint8_t m_Resaved[M_MAX_SIZE];
static size_t m_ResavedSize;
static int32_t m_Resaves;

void ResultScreen_PutReplayFile(
    const char *const path, uint8_t *const data, const size_t size)
{
    CHECK_EQ_STR(path, M_PATH);
    CHECK(size <= sizeof(m_Resaved));
    memcpy(m_Resaved, data, size);
    m_ResavedSize = size;
    m_Resaves++;
    Allocator_Free(&g_HeapAllocator, data, size);
}

static void M_PutU16(uint8_t *const b, const size_t at, const uint16_t value)
{
    b[at] = (uint8_t)value;
    b[at + 1] = (uint8_t)(value >> 8);
}

static void M_PutU32(uint8_t *const b, const size_t at, const uint32_t value)
{
    for (size_t i = 0; i < 4; i++) {
        b[at + i] = (uint8_t)(value >> (8 * i));
    }
}

static void M_PutStage(uint8_t *const s, const M_SLOT *const slot, int32_t n)
{
    M_PutU32(s, 0x00, (uint32_t)slot->score);
    M_PutU32(s, 0x04, (uint32_t)(n * 100 + 1));
    M_PutU32(s, 0x08, (uint32_t)(n * 1000 + 2));
    M_PutU32(s, 0x0c, (uint32_t)(n * 1000 + 3));
    M_PutU32(s, 0x10, (uint32_t)slot->cherry_plus);
    M_PutU32(s, 0x14, (uint32_t)(n * 1000 + 5));
    M_PutU32(s, 0x18, (uint32_t)(n + 1));
    M_PutU32(s, 0x1c, (uint32_t)(n * 10 + 6));
    M_PutU16(s, 0x20, (uint16_t)(0xab00 + n));
    s[0x22] = (uint8_t)(100 + n);
    s[0x23] = (uint8_t)(2 + n);
    s[0x24] = (uint8_t)(1 + n);
    s[0x25] = (uint8_t)(20 + n);
    s[0x26] = (uint8_t)(10 + n);
    s[0x27] = (uint8_t)(40 + n);
    for (int32_t i = 0; i < slot->frame_count; i++) {
        M_PutU16(s, M_STAGE_HEADER_SIZE + 4 * (size_t)i, slot->buttons[i]);
        M_PutU16(s, M_STAGE_HEADER_SIZE + 4 * (size_t)i + 2, slot->events[i]);
    }
}

// Lays out a replay as SaveReplay does, packs it as LZSS literals and
// encrypts it, then keeps it for M_PATH.
static void M_Keep(const M_SPEC *const spec)
{
    uint8_t *const b = m_Image;
    memset(b, 0, sizeof(m_Image));
    memcpy(b, "T7RP", 4);
    M_PutU16(b, 4, 0x1100);
    b[0xd] = 0xc3;
    b[0x55] = 0x62;
    b[0x56] = 3;
    b[0x57] = spec->difficulty;
    memcpy(b + 0x58, "11/15", 6);
    memcpy(b + 0x5e, "TEST", 4);
    b[0x70 + 0x1c] = 4;
    memcpy(b + 0xe0, "0100b", 6);
    size_t offset = M_HEADER_SIZE;
    for (int32_t n = 1; n <= 7; n++) {
        const M_SLOT *const slot = &spec->slots[n];
        if (slot->frame_count != 0) {
            M_PutU32(b, 0x1c + 4 * (size_t)(n - 1), (uint32_t)offset);
            M_PutStage(b + offset, slot, n);
            offset += M_STAGE_HEADER_SIZE + 4 * (size_t)slot->frame_count;
        }
    }
    for (int32_t n = 1; n <= 7; n++) {
        const M_SLOT *const slot = &spec->slots[n];
        if (slot->frame_count != 0) {
            M_PutU32(b, 0x38 + 4 * (size_t)(n - 1), (uint32_t)offset);
            memcpy(b + offset, slot->fps, (size_t)slot->fps_count);
            offset += (size_t)slot->fps_count;
        }
    }
    memset(m_File, 0, sizeof(m_File));
    memcpy(m_File, b, M_PLAIN_SIZE);
    const size_t packed = ReplayTestLzss_EncodeLiterals(
        b + M_PLAIN_SIZE, offset - M_PLAIN_SIZE, m_File + M_PLAIN_SIZE);
    const size_t size = M_PLAIN_SIZE + packed;
    M_PutU32(m_File, 0x10, (uint32_t)offset);
    M_PutU32(m_File, 0x14, (uint32_t)(size - M_PLAIN_SIZE));
    M_PutU32(m_File, 0x18, (uint32_t)(offset - M_PLAIN_SIZE));
    uint32_t checksum = 0x3f000318;
    for (size_t i = 0xd; i < size; i++) {
        checksum += m_File[i];
    }
    M_PutU32(m_File, 8, checksum);
    uint8_t key = m_File[0xd];
    for (size_t i = 0x10; i < size; i++) {
        m_File[i] = (uint8_t)(m_File[i] + key);
        key = (uint8_t)(key + 7);
    }
    ReplayFiles_Add(M_PATH, Mem_Dup(m_File, size), size);
}

// Fills a slot with count frames whose buttons are first, first + 1, ...
static void M_Frames(M_SLOT *const slot, const uint16_t first, int32_t count)
{
    slot->frame_count = count;
    for (int32_t i = 0; i < count; i++) {
        slot->buttons[i] = (uint16_t)(first + i);
    }
    slot->fps[0] = 60;
    slot->fps_count = 1;
}

static CHAIN_STATUS M_Observe(void *const arg)
{
    m_Calcs++;
    m_SeenPlayWord = g_Buttons.play_word;
    return CHAIN_CONTINUE;
}

static REPLAY_EVENT m_EventToMark;
static CHAIN_ELEM m_MarkerElem;

static CHAIN_STATUS M_MarkEvent(void *const arg)
{
    Replay_MarkEvent(m_EventToMark);
    return CHAIN_CONTINUE;
}

static void M_SetUp(const int32_t stage)
{
    Replay_Discard();
    Chain_Release();
    Chain_Init();
    ReplayFiles_Clear();
    m_BorderStarts = 0;
    m_Dialogue = false;
    m_Skippable = false;
    m_Boss = false;
    m_Calcs = 0;
    m_SeenPlayWord = 0;
    m_Resaves = 0;
    g_Settings.fix_replay_desync = false;
    g_Settings.fix_input_lag = false;
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = stage;
    g_GameManager.mode.playing = true;
    g_GameManager.mode.in_replay = true;
    g_GameManager.cherry.offset = M_CHERRY_OFFSET;
    Rng_SetSeed(&g_Rng, 0x4321);
}

static void M_Observer(const int16_t priority)
{
    Chain_InitElem(&m_ObserverElem, M_Observe, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&m_ObserverElem, priority)));
}

// Marks event at priority 7, between the mark element's reset (6) and the
// record element's write (16).
static void M_MarksEvent(const REPLAY_EVENT event)
{
    m_EventToMark = event;
    Chain_InitElem(&m_MarkerElem, M_MarkEvent, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&m_MarkerElem, 7)));
}

static void M_Play(const M_SPEC *const spec)
{
    M_Keep(spec);
    CHECK(Result_Log(Replay_Register(REPLAY_PLAY, M_PATH), LOG_LEVEL_ERROR));
}

static void M_Calcs(const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        Chain_RunCalc();
    }
}

static void M_TearDown(void)
{
    Replay_Discard();
    Chain_Release();
    ReplayFiles_Clear();
}

TEST(playback_starts_on_the_leading_empty_frame)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0x40, 4);
    M_Play(&spec);
    g_Buttons.word = 0x1008;
    M_Calcs(1);
    CHECK_EQ_INT(g_Buttons.play_word, 0x40);
    M_Calcs(1);
    CHECK_EQ_INT(g_Buttons.play_word, 0x41);
    CHECK_EQ_INT(g_Buttons.previous_play_word, 0x40);
    CHECK_EQ_INT(g_Buttons.word, 0x1008);
    M_TearDown();
}

TEST(a_pause_marker_keeps_the_shoot_edge_from_the_recorded_frames)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 3);
    spec.slots[1].buttons[1] = 1;
    spec.slots[1].events[1] = REPLAY_EVENT_PAUSE;
    M_Play(&spec);
    M_Calcs(2);
    CHECK_EQ_INT(g_Buttons.play_word, 1);
    CHECK_EQ_INT(g_Buttons.previous_play_word, 0);
    M_TearDown();
}

TEST(the_replay_fix_defaults_on_and_can_be_disabled)
{
    TH07_SETTINGS settings;
    Config_SetDefaults(&g_SettingsSchema, &settings);
    CHECK(settings.fix_replay_desync);
    INI *ini;
    const char *const text = "[fixes]\nreplay_desync = false\n";
    CHECK(IS_OK(Ini_Parse(text, strlen(text), &g_HeapAllocator, &ini)));
    Config_Load(&g_SettingsSchema, ini, &settings);
    CHECK(!settings.fix_replay_desync);
    Ini_Free(ini);
}

// Plays a bomb pressed on frame 2 and started on frame early ? 2 : 3, then
// a pause marker on frame 5 with shoot newly held. Returns the previous
// play word of the marker's calc.
static uint16_t M_PreviousOnResume(const bool fix, const bool early)
{
    M_SetUp(1);
    g_Settings.fix_replay_desync = fix;
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 9);
    memset(spec.slots[1].buttons, 0, sizeof(spec.slots[1].buttons));
    spec.slots[1].buttons[2] = 2;
    spec.slots[1].buttons[3] = 2;
    spec.slots[1].events[early ? 2 : 3] = REPLAY_EVENT_BOMB;
    spec.slots[1].buttons[5] = 1;
    spec.slots[1].events[5] = REPLAY_EVENT_PAUSE;
    M_Play(&spec);
    M_Calcs(6);
    CHECK_EQ_INT(g_Buttons.play_word, 1);
    const uint16_t previous = g_Buttons.previous_play_word;
    M_TearDown();
    return previous;
}

TEST(the_replay_fix_keeps_the_resume_shoot_edge_of_bombs_a_frame_late)
{
    CHECK_EQ_INT(M_PreviousOnResume(true, false), 0);
}

TEST(the_replay_fix_holds_the_resume_buttons_of_bombs_on_their_press)
{
    CHECK_EQ_INT(M_PreviousOnResume(true, true), 1);
    CHECK_EQ_INT(M_PreviousOnResume(false, true), 0);
}

TEST(playback_steps_key_repeat_on_the_play_word)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 40);
    for (int32_t i = 0; i < 40; i++) {
        spec.slots[1].buttons[i] = 0x10;
    }
    M_Play(&spec);
    int32_t repeats = 0;
    for (int32_t calc = 0; calc < 40; calc++) {
        M_Calcs(1);
        if (g_Buttons.repeat) {
            repeats++;
            CHECK_EQ_INT(calc, 33);
        }
    }
    CHECK_EQ_INT(repeats, 1);
    M_TearDown();
}

TEST(every_30th_calc_restores_the_rate_of_the_next_byte)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 100);
    const uint8_t fps[] = { 0x80 | 50, 40, 0x80 | 30, 25 };
    memcpy(spec.slots[1].fps, fps, sizeof(fps));
    spec.slots[1].fps_count = 4;
    M_Play(&spec);
    g_GameManager.mode.replay_fps = 55;
    M_Calcs(1);
    CHECK_EQ_INT(g_GameManager.mode.replay_fps, 40);
    g_GameManager.mode.replay_fps = 55;
    M_Calcs(29);
    CHECK_EQ_INT(g_GameManager.mode.replay_fps, 55);
    M_Calcs(1);
    CHECK_EQ_INT(g_GameManager.mode.replay_fps, 30);
    M_Calcs(30);
    CHECK_EQ_INT(g_GameManager.mode.replay_fps, 25);
    M_Calcs(30);
    CHECK_EQ_INT(g_GameManager.mode.replay_fps, 25);
    M_TearDown();
}

TEST(playback_waits_while_not_playing_and_in_slow_mode)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0x40, 4);
    M_Play(&spec);
    g_GameManager.mode.playing = false;
    M_Calcs(5);
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    g_GameManager.mode.playing = true;
    g_GameManager.run.score_disabled = true;
    M_Calcs(5);
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    g_GameManager.run.score_disabled = false;
    M_Calcs(1);
    CHECK_EQ_INT(g_Buttons.play_word, 0x40);
    M_TearDown();
}

TEST(skippable_dialogue_restarts_the_chain_on_2_calcs_of_3)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 20);
    M_Observer(10);
    M_Play(&spec);
    m_Dialogue = true;
    m_Skippable = true;
    M_Calcs(1);
    CHECK_EQ_INT(m_Calcs, 2);
    M_Calcs(1);
    CHECK_EQ_INT(m_Calcs, 5);
    m_Skippable = false;
    M_Calcs(1);
    CHECK_EQ_INT(m_Calcs, 6);
    M_TearDown();
}

TEST(fast_playback_restarts_the_chain_on_4_calcs_of_5_without_a_boss)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 20);
    M_Observer(10);
    M_Play(&spec);
    g_GameManager.mode.replay_speed = 2;
    M_Calcs(1);
    CHECK_EQ_INT(m_Calcs, 4);
    M_Calcs(1);
    CHECK_EQ_INT(m_Calcs, 9);
    m_Boss = true;
    M_Calcs(1);
    CHECK_EQ_INT(m_Calcs, 10);
    g_GameManager.mode.replay_speed = 1;
    m_Boss = false;
    M_Calcs(1);
    CHECK_EQ_INT(m_Calcs, 11);
    M_TearDown();
}

TEST(restoring_a_stage_takes_its_state_and_the_replay_config)
{
    M_SetUp(1);
    M_SPEC spec = { .difficulty = 2 };
    M_Frames(&spec.slots[1], 0, 4);
    spec.slots[1].cherry_plus = 49999;
    g_GameManager.run.start_lives = 2;
    g_GameManager.run.score_disabled = true;
    M_Play(&spec);
    const GAME_MANAGER *const gm = &g_GameManager;
    CHECK_EQ_INT(gm->run.character, 1);
    CHECK_EQ_INT(gm->run.shot_type, 1);
    CHECK_EQ_INT(gm->run.shot_index, 3);
    CHECK_EQ_INT(gm->run.difficulty, 2);
    CHECK_EQ_INT(gm->player.point_items_total, 101);
    CHECK_EQ_INT(gm->cherry.value, M_CHERRY_OFFSET + 1002);
    CHECK_EQ_INT(gm->cherry.max, M_CHERRY_OFFSET + 1003);
    CHECK_EQ_INT(gm->cherry.plus, M_CHERRY_OFFSET + 49999);
    CHECK_EQ_INT(gm->player.graze_total, 1005);
    CHECK_EQ_INT(gm->player.extends, 2);
    CHECK_EQ_INT(gm->player.next_extend_items, 16);
    CHECK_EQ_INT(g_Rng.seed, 0xab01);
    // The seed keeps the count of the two checksum key draws.
    CHECK_EQ_INT(g_Rng.generated, 8);
    CHECK(gm->player.power == 101.0f);
    CHECK(gm->player.lives == 3.0f);
    CHECK(gm->player.bombs == 2.0f);
    CHECK_EQ_INT(gm->rank.value, 21);
    CHECK_EQ_INT(gm->player.power_item_combo, 11);
    CHECK_EQ_INT(gm->stats.spell_cards_captured, 41);
    CHECK_EQ_INT(gm->run.start_lives, 4);
    CHECK(!gm->run.score_disabled);
    CHECK_EQ_INT(m_BorderStarts, 0);
    CHECK_EQ_INT(gm->score.value, 0);
    M_TearDown();
}

TEST(a_full_plus_starts_the_border)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 4);
    spec.slots[1].cherry_plus = 50003;
    M_Play(&spec);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET + 50000);
    CHECK_EQ_INT(m_BorderStarts, 1);
    M_TearDown();
}

TEST(stages_2_to_6_take_the_previous_slot_score)
{
    M_SPEC spec = {};
    for (int32_t n = 1; n <= 7; n++) {
        M_Frames(&spec.slots[n], 0, 4);
        spec.slots[n].score = n * 1000;
    }
    for (int32_t stage = 1; stage <= 8; stage++) {
        M_SetUp(stage);
        g_GameManager.score.value = 77;
        g_GameManager.score.shown = 66;
        M_Play(&spec);
        const bool takes = stage >= 2 && stage <= 6;
        CHECK_EQ_INT(
            g_GameManager.score.value, takes ? (stage - 1) * 1000 : 77);
        CHECK_EQ_INT(
            g_GameManager.score.shown, takes ? (stage - 1) * 1000 : 66);
        M_TearDown();
    }
}

// Plays stage 1 of four frames for calcs, ends it and starts stage 3,
// which takes the score of slot 2, 0x12345678.
static int32_t M_ScoreAfterEnding(const int32_t calcs, const bool fix)
{
    M_SetUp(1);
    g_Settings.fix_replay_desync = fix;
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0x10, 4);
    M_Frames(&spec.slots[2], 0x20, 4);
    M_Frames(&spec.slots[3], 0x30, 4);
    spec.slots[2].score = 0x12345678;
    M_Play(&spec);
    M_Calcs(calcs);
    Replay_EndStage();
    g_GameManager.run.stage = 3;
    CHECK(Result_Log(Replay_Register(REPLAY_PLAY, M_PATH), LOG_LEVEL_ERROR));
    const int32_t score = (int32_t)g_GameManager.score.value;
    M_TearDown();
    return score;
}

TEST(ending_a_played_stage_a_calc_late_clears_the_next_score_low_word)
{
    CHECK_EQ_INT(M_ScoreAfterEnding(2, false), 0x12345678);
    CHECK_EQ_INT(M_ScoreAfterEnding(3, false), 0x12340000);
}

TEST(the_replay_fix_preserves_saved_scores_when_a_stage_ends_late)
{
    CHECK_EQ_INT(M_ScoreAfterEnding(2, true), 0x12345678);
    CHECK_EQ_INT(M_ScoreAfterEnding(3, true), 0x12345678);
}

// Plays stage 1 of four frames without bombs for calcs and ends it, then
// stage 2 with a pause marker on frame 2 with shoot newly held. Returns
// the previous play word of the marker's calc.
static uint16_t M_PreviousOnResumeAfterEnding(const int32_t calcs)
{
    M_SetUp(1);
    g_Settings.fix_replay_desync = true;
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 4);
    M_Frames(&spec.slots[2], 0, 4);
    memset(spec.slots[1].buttons, 0, sizeof(spec.slots[1].buttons));
    memset(spec.slots[2].buttons, 0, sizeof(spec.slots[2].buttons));
    spec.slots[2].buttons[2] = 1;
    spec.slots[2].events[2] = REPLAY_EVENT_PAUSE;
    M_Play(&spec);
    M_Calcs(calcs);
    Replay_EndStage();
    g_GameManager.run.stage = 2;
    CHECK(Result_Log(Replay_Register(REPLAY_PLAY, M_PATH), LOG_LEVEL_ERROR));
    M_Calcs(3);
    CHECK_EQ_INT(g_Buttons.play_word, 1);
    const uint16_t previous = g_Buttons.previous_play_word;
    M_TearDown();
    return previous;
}

TEST(the_replay_fix_holds_the_resume_buttons_after_a_stage_ends_late)
{
    CHECK_EQ_INT(M_PreviousOnResumeAfterEnding(2), 0);
    CHECK_EQ_INT(M_PreviousOnResumeAfterEnding(3), 1);
}

TEST(phantasm_plays_slot_7)
{
    M_SetUp(8);
    M_SPEC spec = { .difficulty = 5 };
    M_Frames(&spec.slots[7], 0x70, 4);
    M_Play(&spec);
    CHECK_EQ_INT(g_Rng.seed, 0xab07);
    M_Calcs(1);
    CHECK_EQ_INT(g_Buttons.play_word, 0x70);
    M_TearDown();
}

TEST(has_stage_tells_the_slots_of_the_replay_being_played)
{
    M_SetUp(5);
    CHECK(!Replay_HasStage(5));
    M_SPEC spec = {};
    M_Frames(&spec.slots[5], 0, 4);
    M_Frames(&spec.slots[7], 0, 4);
    M_Play(&spec);
    for (int32_t stage = 1; stage <= 7; stage++) {
        CHECK(Replay_HasStage(stage) == (stage == 5 || stage == 7));
    }
    M_TearDown();
    CHECK(!Replay_HasStage(5));
}

TEST(has_stage_is_false_while_recording)
{
    M_SetUp(1);
    g_GameManager.mode.in_replay = false;
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    CHECK(!Replay_HasStage(1));
    M_TearDown();
}

TEST(next_stage_restores_its_slot_and_starts_its_frames_again)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0x10, 8);
    M_Frames(&spec.slots[2], 0x20, 8);
    M_Play(&spec);
    M_Calcs(3);
    g_GameManager.run.stage = 2;
    g_Buttons.play_word = 0x12;
    CHECK(Result_Log(Replay_Register(REPLAY_PLAY, M_PATH), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    CHECK_EQ_INT(g_Rng.seed, 0xab02);
    M_Calcs(1);
    CHECK_EQ_INT(g_Buttons.play_word, 0x20);
    CHECK_EQ_INT(g_Buttons.previous_play_word, 0);
    M_TearDown();
}

TEST(playback_past_the_last_frame_plays_no_buttons_and_logs_once)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0x40, 2);
    M_Play(&spec);
    FILE *const log = fopen("test_th07_replay_chain_log.txt", "w+");
    CHECK(log != nullptr);
    if (log == nullptr) {
        return;
    }
    Log_SetFile(log);
    M_Calcs(2);
    CHECK_EQ_INT(g_Buttons.play_word, 0x41);
    M_Calcs(1);
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    CHECK_EQ_INT(g_Buttons.previous_play_word, 0x41);
    M_Calcs(5);
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    CHECK_EQ_INT(g_Buttons.held_frames, 5);
    Log_SetFile(nullptr);
    rewind(log);
    int32_t lines = 0;
    for (int c = fgetc(log); c != EOF; c = fgetc(log)) {
        if (c == '\n') {
            lines++;
        }
    }
    fclose(log);
    CHECK_EQ_INT(lines, 1);
    M_TearDown();
}

TEST(playback_of_a_missing_slot_or_path_fails)
{
    M_SetUp(3);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0, 4);
    M_Keep(&spec);
    CHECK(!Result_Ignore(Replay_Register(REPLAY_PLAY, M_PATH)));
    m_Dialogue = true;
    m_Skippable = true;
    CHECK_EQ_INT(Chain_RunCalc(), CHAIN_RUN_CONTINUE);
    Replay_Discard();
    CHECK(g_Chain.calc.next == nullptr);
    CHECK(!Result_Ignore(Replay_Register(REPLAY_PLAY, "elsewhere.rpy")));
    M_TearDown();
}

TEST(recording_keeps_the_buttons_at_16_for_the_next_calc)
{
    M_SetUp(1);
    g_GameManager.mode.in_replay = false;
    M_Observer(8);
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    g_Buttons.word = 0x11;
    M_Calcs(1);
    CHECK_EQ_INT(m_SeenPlayWord, 0);
    CHECK_EQ_INT(g_Buttons.play_word, 0x11);
    g_Buttons.word = 0x13;
    M_Calcs(1);
    CHECK_EQ_INT(m_SeenPlayWord, 0x11);
    CHECK_EQ_INT(g_Buttons.play_word, 0x13);
    CHECK_EQ_INT(g_Buttons.previous_play_word, 0x11);
    CHECK_EQ_INT(g_Buttons.held_frames, 0);
    g_GameManager.mode.playing = false;
    g_Buttons.word = 0x8;
    M_Calcs(1);
    CHECK_EQ_INT(g_Buttons.play_word, 0x13);
    g_GameManager.mode.playing = true;
    g_GameManager.run.score_disabled = true;
    M_Calcs(1);
    CHECK_EQ_INT(g_Buttons.play_word, 0x8);
    M_TearDown();
}

TEST(recording_marks_and_clears_a_pause_request_at_6)
{
    M_SetUp(1);
    g_GameManager.mode.in_replay = false;
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    g_GameManager.mode.pause_requested = true;
    g_GameManager.mode.playing = false;
    Rng_U16(&g_Rng);
    M_Calcs(1);
    CHECK(!g_GameManager.mode.pause_requested);
    CHECK_EQ_INT(g_Rng.generated, 0);
    M_TearDown();
}

TEST(registering_clears_the_play_words)
{
    M_SetUp(1);
    g_Buttons.play_word = 5;
    g_Buttons.previous_play_word = 6;
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    CHECK_EQ_INT(g_Buttons.previous_play_word, 0);
    g_Buttons.play_word = 5;
    g_GameManager.run.stage = 2;
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    Replay_EndStage();
    M_TearDown();
}

TEST(marking_an_event_without_a_recording_does_nothing)
{
    M_SetUp(1);
    Replay_MarkEvent(REPLAY_EVENT_HIT);
    CHECK(g_Chain.calc.next == nullptr);
    M_TearDown();
}

// Plays a replay of difficulty with frames in slot, then resaves it with
// a changed score. Sets *before to the RNG as the resave starts.
static void M_PlayAndResave(
    const uint8_t difficulty, const int32_t slot, RNG *const before)
{
    M_SPEC spec = { .difficulty = difficulty };
    M_Frames(&spec.slots[slot], 0x10, 4);
    M_Play(&spec);
    M_Calcs(2);
    snprintf(
        g_GameManager.mode.replay_path, sizeof(g_GameManager.mode.replay_path),
        "%s", M_PATH);
    g_GameManager.score.value = 7777;
    g_GameManager.score.shown = 7770;
    *before = g_Rng;
    Replay_ResaveIfScoreChanged();
}

TEST(resave_writes_the_new_scores_with_three_new_keys)
{
    M_SetUp(8);
    RNG rng;
    M_PlayAndResave(5, 7, &rng);
    uint8_t random[3];
    random[0] = (uint8_t)(Rng_U16InRange(&rng, 128) + 64);
    random[1] = (uint8_t)Rng_U16InRange(&rng, 256);
    random[2] = (uint8_t)Rng_U16InRange(&rng, 256);
    CHECK(memcmp(&g_Rng, &rng, sizeof(rng)) == 0);
    CHECK_EQ_INT(m_Resaves, 1);
    CHECK_EQ_INT(m_Resaved[0xd], random[0]);
    CHECK_EQ_INT(m_Resaved[0xc], random[2]);
    CHECK(!Replay_HasStage(7));

    READER reader;
    Reader_OpenBuffer(&reader, m_Resaved, m_ResavedSize, nullptr);
    REPLAY_FILE *file = nullptr;
    CHECK(Result_Log(Replay_Read(&reader, &file), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    CHECK_EQ_INT(file->score, 7770);
    CHECK_EQ_INT(file->stages[7]->score, 7777);
    CHECK_EQ_INT(file->stages[7]->frames[1].buttons, 0x11);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Log(Replay_Write(file, &writer, random), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const expected = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    CHECK_EQ_INT(size, m_ResavedSize);
    CHECK(memcmp(expected, m_Resaved, size) == 0);
    Allocator_Free(&g_HeapAllocator, expected, size);
    Replay_Free(file);
    M_TearDown();
}

TEST(resave_does_nothing_while_the_score_is_unchanged)
{
    M_SetUp(8);
    M_SPEC spec = { .difficulty = 5 };
    M_Frames(&spec.slots[7], 0x10, 4);
    M_Play(&spec);
    const RNG rng = g_Rng;
    Replay_ResaveIfScoreChanged();
    CHECK_EQ_INT(m_Resaves, 0);
    CHECK(memcmp(&g_Rng, &rng, sizeof(rng)) == 0);
    CHECK(Replay_HasStage(7));
    M_TearDown();
}

TEST(the_replay_fix_keeps_the_source_recording_when_playback_ends)
{
    M_SetUp(8);
    g_Settings.fix_replay_desync = true;
    RNG rng;
    M_PlayAndResave(5, 7, &rng);
    CHECK_EQ_INT(m_Resaves, 0);
    CHECK(memcmp(&g_Rng, &rng, sizeof(rng)) == 0);
    CHECK(!Replay_HasStage(7));
    M_TearDown();
}

TEST(resave_of_another_config_ends_playback_without_a_write)
{
    M_SetUp(3);
    RNG rng;
    M_PlayAndResave(1, 3, &rng);
    CHECK_EQ_INT(m_Resaves, 0);
    CHECK(memcmp(&g_Rng, &rng, sizeof(rng)) == 0);
    CHECK(!Replay_HasStage(3));
    M_TearDown();
}

TEST(resave_ignores_the_config_in_practice_extra_and_phantasm)
{
    M_SetUp(3);
    g_GameManager.mode.in_practice = true;
    RNG rng;
    M_PlayAndResave(1, 3, &rng);
    CHECK_EQ_INT(m_Resaves, 1);
    M_TearDown();
    M_SetUp(7);
    M_PlayAndResave(4, 7, &rng);
    CHECK_EQ_INT(m_Resaves, 1);
    M_TearDown();
}

// Records count calcs of stage 2 and sets the score that a save stores.
static void M_Record(const int32_t count, const bool slow_mode)
{
    M_SetUp(2);
    g_GameManager.mode.in_replay = false;
    g_GameManager.run.score_disabled = slow_mode;
    g_GameManager.run.start_lives = (uint8_t)g_Settings.lives;
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    g_Buttons.word = 0x3;
    M_Calcs(count);
    g_GameManager.score.value = 4444;
    g_GameManager.score.shown = 4440;
}

static const REPLAY_SAVE_INFO m_SaveInfo = {
    .name = "KUU",
    .date = "11/16",
    .slowdown = 1.5f,
};

TEST(save_writes_the_recording_with_its_name_and_three_new_keys)
{
    M_Record(3, false);
    RNG rng = g_Rng;
    Rng_U16InRange(&rng, 128);
    Rng_U16InRange(&rng, 256);
    Rng_U16InRange(&rng, 256);
    Replay_Save(M_PATH, &m_SaveInfo);
    CHECK(memcmp(&g_Rng, &rng, sizeof(rng)) == 0);
    CHECK_EQ_INT(m_Resaves, 1);
    READER reader;
    Reader_OpenBuffer(&reader, m_Resaved, m_ResavedSize, nullptr);
    REPLAY_FILE *file = nullptr;
    CHECK(Result_Log(Replay_Read(&reader, &file), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    CHECK_EQ_STR(file->name, "KUU");
    CHECK_EQ_STR(file->date, "11/16");
    CHECK(file->slowdown == 1.5f);
    CHECK_EQ_INT(file->score, 4440);
    CHECK_EQ_INT(file->exe_size, TH07_EXE_SIZE);
    CHECK_EQ_INT(file->exe_checksum, TH07_EXE_CHECKSUM);
    const REPLAY_STAGE *const stage = file->stages[2];
    CHECK_EQ_INT(stage->score, 4444);
    CHECK_EQ_INT(stage->frame_count, 5);
    CHECK_EQ_INT(stage->frames[1].buttons, 0x3);
    CHECK_EQ_INT(stage->frames[stage->frame_count - 1].buttons, 0);
    Replay_Free(file);
    M_TearDown();
}

TEST(save_writes_the_events_marked_in_each_calc)
{
    M_SetUp(2);
    g_GameManager.mode.in_replay = false;
    g_GameManager.run.start_lives = (uint8_t)g_Settings.lives;
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    M_MarksEvent(REPLAY_EVENT_HIT);
    M_Calcs(1);
    m_EventToMark = REPLAY_EVENT_BOMB;
    M_Calcs(1);
    Replay_Save(M_PATH, &m_SaveInfo);
    READER reader;
    Reader_OpenBuffer(&reader, m_Resaved, m_ResavedSize, nullptr);
    REPLAY_FILE *file = nullptr;
    CHECK(Result_Log(Replay_Read(&reader, &file), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    const REPLAY_STAGE *const stage = file->stages[2];
    CHECK_EQ_INT(stage->frames[1].events, REPLAY_EVENT_HIT);
    CHECK_EQ_INT(stage->frames[2].events, REPLAY_EVENT_BOMB);
    Replay_Free(file);
    M_TearDown();
}

TEST(the_input_lag_fix_defaults_off_and_can_be_enabled)
{
    TH07_SETTINGS settings;
    Config_SetDefaults(&g_SettingsSchema, &settings);
    CHECK(!settings.fix_input_lag);
    INI *ini;
    const char *const text = "[fixes]\ninput_lag = true\n";
    CHECK(IS_OK(Ini_Parse(text, strlen(text), &g_HeapAllocator, &ini)));
    Config_Load(&g_SettingsSchema, ini, &settings);
    CHECK(settings.fix_input_lag);
    Ini_Free(ini);
}

TEST(the_input_lag_fix_takes_the_live_buttons_outside_playback)
{
    M_SetUp(1);
    g_Buttons.word = 0x11;
    Replay_TakeLiveButtons();
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    g_Settings.fix_input_lag = true;
    Replay_TakeLiveButtons();
    CHECK_EQ_INT(g_Buttons.play_word, 0);
    g_GameManager.mode.in_replay = false;
    Replay_TakeLiveButtons();
    g_Buttons.word = 0x13;
    Replay_TakeLiveButtons();
    CHECK_EQ_INT(g_Buttons.play_word, 0x13);
    CHECK_EQ_INT(g_Buttons.previous_play_word, 0x11);
    M_TearDown();
}

// Marks a bomb in the calc whose play word newly holds bomb, as the player
// starts one at priority 8.
static CHAIN_STATUS M_BombOnPress(void *const arg)
{
    if (Buttons_FromWord(g_Buttons.play_word).bomb
        && !Buttons_FromWord(g_Buttons.previous_play_word).bomb) {
        Replay_MarkEvent(REPLAY_EVENT_BOMB);
    }
    return M_Observe(arg);
}

TEST(the_input_lag_fix_records_a_bomb_on_its_press_for_stock_playback)
{
    static const uint16_t words[] = { 0, 0x1, 0x3, 0x3, 0x1, 0 };
    uint16_t live[ARRAY_SIZE(words)] = {};
    M_SetUp(2);
    g_GameManager.mode.in_replay = false;
    g_GameManager.run.start_lives = (uint8_t)g_Settings.lives;
    g_Settings.fix_input_lag = true;
    Chain_InitElem(&m_ObserverElem, M_BombOnPress, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&m_ObserverElem, 8)));
    // The stage registers within a calc, after the buttons are read.
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    M_Calcs(1);
    for (size_t i = 1; i < ARRAY_SIZE(words); i++) {
        g_Buttons.word = words[i];
        Replay_TakeLiveButtons();
        M_Calcs(1);
        live[i] = m_SeenPlayWord;
    }
    CHECK_EQ_INT(live[2], 0x3);
    Replay_Save(M_PATH, &m_SaveInfo);
    CHECK_EQ_INT(m_Resaves, 1);
    READER reader;
    Reader_OpenBuffer(&reader, m_Resaved, m_ResavedSize, nullptr);
    REPLAY_FILE *file = nullptr;
    CHECK(Result_Log(Replay_Read(&reader, &file), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    const REPLAY_STAGE *const stage = file->stages[2];
    CHECK_EQ_INT(stage->frame_count, (int32_t)ARRAY_SIZE(words) + 1);
    CHECK_EQ_INT(stage->frames[2].buttons, 0x3);
    CHECK_EQ_INT(stage->frames[2].events, REPLAY_EVENT_BOMB);
    Replay_Free(file);
    ReplayFiles_Add(M_PATH, Mem_Dup(m_Resaved, m_ResavedSize), m_ResavedSize);
    g_GameManager.mode.in_replay = true;
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    CHECK(Result_Log(Replay_Register(REPLAY_PLAY, M_PATH), LOG_LEVEL_ERROR));
    M_Calcs(1);
    CHECK_EQ_INT(m_SeenPlayWord, 0);
    for (size_t i = 1; i < ARRAY_SIZE(words); i++) {
        M_Calcs(1);
        CHECK_EQ_INT(m_SeenPlayWord, live[i]);
    }
    M_TearDown();
}

TEST(save_of_a_slow_mode_recording_or_a_playback_writes_nothing)
{
    M_Record(3, true);
    const RNG rng = g_Rng;
    Replay_Save(M_PATH, &m_SaveInfo);
    CHECK_EQ_INT(m_Resaves, 0);
    CHECK(memcmp(&g_Rng, &rng, sizeof(rng)) == 0);
    M_TearDown();
    M_SetUp(3);
    M_SPEC spec = { .difficulty = 1 };
    M_Frames(&spec.slots[3], 0x10, 4);
    M_Play(&spec);
    Replay_Save(M_PATH, &m_SaveInfo);
    CHECK_EQ_INT(m_Resaves, 0);
    CHECK(!Replay_HasStage(3));
    M_TearDown();
}
