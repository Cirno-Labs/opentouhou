#include "th07/replay.h"

#include "test.h"

#include "th07/enemy.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/player.h"

#include "core/base/log.h"

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

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;

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

typedef struct {
    uint8_t *bytes;
    uint32_t bit;
} M_BITS;

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

static void M_PutBits(M_BITS *const bits, const uint32_t value, const int count)
{
    for (int i = count - 1; i >= 0; i--) {
        if ((value >> i & 1) != 0) {
            bits->bytes[bits->bit / 8] |= (uint8_t)(0x80 >> (bits->bit % 8));
        }
        bits->bit++;
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
    M_BITS bits = { .bytes = m_File + M_PLAIN_SIZE };
    for (size_t i = M_PLAIN_SIZE; i < offset; i++) {
        M_PutBits(&bits, 1, 1);
        M_PutBits(&bits, b[i], 8);
    }
    M_PutBits(&bits, 0, 14);
    const size_t size = M_PLAIN_SIZE + bits.bit / 8;
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
    Replay_AddFile(M_PATH, m_File, size);
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
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    Replay_ClearFiles();
    m_BorderStarts = 0;
    m_Dialogue = false;
    m_Skippable = false;
    m_Boss = false;
    m_Calcs = 0;
    m_SeenPlayWord = 0;
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
    CHECK(Result_Ignore(Chain_AddCalc(&g_Chain, &m_ObserverElem, priority)));
}

// Marks event at priority 7, between the mark element's reset (6) and the
// record element's write (16).
static void M_MarksEvent(const REPLAY_EVENT event)
{
    m_EventToMark = event;
    Chain_InitElem(&m_MarkerElem, M_MarkEvent, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&g_Chain, &m_MarkerElem, 7)));
}

static void M_Play(const M_SPEC *const spec)
{
    M_Keep(spec);
    CHECK(Result_Log(Replay_Register(REPLAY_PLAY, M_PATH), LOG_LEVEL_ERROR));
}

static void M_Calcs(const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        Chain_RunCalc(&g_Chain);
    }
}

static void M_TearDown(void)
{
    Replay_Discard();
    Chain_Release(&g_Chain);
    Replay_ClearFiles();
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
static int32_t M_ScoreAfterEnding(const int32_t calcs)
{
    M_SetUp(1);
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
    CHECK_EQ_INT(M_ScoreAfterEnding(2), 0x12345678);
    CHECK_EQ_INT(M_ScoreAfterEnding(3), 0x12340000);
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
    CHECK_EQ_INT(Chain_RunCalc(&g_Chain), CHAIN_RUN_CONTINUE);
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

TEST(get_last_events_is_0_before_the_first_calc)
{
    M_SetUp(1);
    CHECK_EQ_INT(Replay_GetLastEvents(), 0);
    M_TearDown();
}

TEST(get_last_events_returns_the_recorded_calcs_events)
{
    M_SetUp(1);
    g_GameManager.mode.in_replay = false;
    CHECK(Result_Ignore(Replay_Register(REPLAY_RECORD, M_PATH)));
    M_MarksEvent(REPLAY_EVENT_HIT);
    M_Calcs(1);
    CHECK_EQ_INT(Replay_GetLastEvents(), REPLAY_EVENT_HIT);
    m_EventToMark = REPLAY_EVENT_BOMB;
    M_Calcs(1);
    CHECK_EQ_INT(Replay_GetLastEvents(), REPLAY_EVENT_BOMB);
    M_TearDown();
}

TEST(get_last_events_returns_the_played_frames_events)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0x40, 4);
    spec.slots[1].events[0] = 73;
    spec.slots[1].events[1] = REPLAY_EVENT_BOMB;
    M_Play(&spec);
    M_Calcs(1);
    CHECK_EQ_INT(Replay_GetLastEvents(), 73);
    M_Calcs(1);
    CHECK_EQ_INT(Replay_GetLastEvents(), REPLAY_EVENT_BOMB);
    M_TearDown();
}

TEST(get_last_events_is_0_during_playback)
{
    M_SetUp(1);
    M_SPEC spec = {};
    M_Frames(&spec.slots[1], 0x40, 4);
    M_Play(&spec);
    M_Calcs(2);
    CHECK_EQ_INT(Replay_GetLastEvents(), 0);
    M_TearDown();
}
