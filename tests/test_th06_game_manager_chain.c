// SPDX-License-Identifier: GPL-3.0-only

#include "th06/game_manager.h"

#include "test.h"

#include "th06/bullet.h"
#include "th06/ascii.h"
#include "th06/ecl.h"
#include "th06/effect.h"
#include "th06/enemy.h"
#include "th06/game.h"
#include "th06/gui.h"
#include "th06/player.h"
#include "th06/scene.h"
#include "th06/screen_effect.h"
#include "th06/sound.h"
#include "th06/stage.h"
#include "th06/screens/result.h"
#include "th06/replay.h"

#include "core/math/x87.h"

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;
STAGE g_Stage;

static char m_SongPath[] = "bgm/th06_02.mid";

static SCENE m_Scene;
static char m_Events[128];
static int32_t m_EventCount;
static char m_Fail;
static bool m_ReplayFailed;
static bool m_RestoreReplay;
static int32_t m_LoadGenerated;
static int32_t m_StageGenerated;
static int32_t m_ReplayGenerated;
static int32_t m_FadeFrames;
static uint32_t m_FadeColor;
static int32_t m_AfterCalls;
static int32_t m_FrameResets;
static CHAIN_ELEM m_AfterElem;

static void M_Event(const char event)
{
    m_Events[m_EventCount++] = event;
    m_Events[m_EventCount] = '\0';
}

static RESULT M_Register(const char event)
{
    M_Event(event);
    return m_Fail == event ? FAIL("test setup failure") : OK;
}

SCENE Scene_GetRequested(void)
{
    return m_Scene;
}

void Scene_Request(const SCENE scene)
{
    m_Scene = scene;
}

RESULT Stage_Register(void)
{
    m_StageGenerated = g_Rng.generated;
    return M_Register('S');
}

RESULT Player_Register(void)
{
    return M_Register('P');
}
RESULT Bullet_Register(void)
{
    return M_Register('B');
}
RESULT Enemy_Register(void)
{
    return M_Register('E');
}
RESULT Ecl_Load(void)
{
    return M_Register('L');
}
RESULT Effect_Register(void)
{
    return M_Register('F');
}
RESULT Gui_Register(void)
{
    return M_Register('G');
}
void Stage_Remove(void)
{
    M_Event('s');
}
void Bullet_Remove(void)
{
    M_Event('b');
}
void Player_Remove(void)
{
    M_Event('p');
}
void Enemy_Remove(void)
{
    M_Event('e');
}
void Ecl_Release(void)
{
    M_Event('l');
}
void Effect_Remove(void)
{
    M_Event('f');
}
void Gui_Remove(void)
{
    M_Event('g');
}
void Replay_EndStage(void)
{
    M_Event('r');
}
void AsciiText_Initialize(void)
{
    M_Event('A');
}

RESULT Replay_Register(const REPLAY_MODE mode, const char *const path)
{
    M_Event(mode == 1 ? 'R' : 'W');
    m_ReplayGenerated = g_Rng.generated;
    if (m_RestoreReplay) {
        Rng_SetSeed(&g_Rng, 0x9876);
        g_GameManager.score.shown = 20000000;
        g_GameManager.score.value = 20000000;
        g_GameManager.player.power = 80;
        g_GameManager.rank.value = 19;
    }
    return m_ReplayFailed ? FAIL("test replay failure") : OK;
}

void ResultScreen_LoadGameRecords(
    uint32_t *const high_score, SPELL_CARD_RECORD spell_cards[SPELL_CARD_COUNT],
    CLEAR_RECORD clears[SCORE_SHOT_COUNT],
    PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                            [SCORE_PRACTICE_DIFFICULTY_COUNT],
    const int32_t shot, const int32_t difficulty)
{
    M_Event('D');
    m_LoadGenerated = g_Rng.generated;
    *high_score = 1234567;
    for (int32_t i = 0; i < 4; i++) {
        for (int32_t j = 0; j < 5; j++) {
            clears[i].without_continues[j] = 1;
            clears[i].with_continues[j] = 1;
        }
    }
    practice[shot][g_GameManager.run.stage][difficulty].score = 555;
}

void ResultScreen_ResetFrames(void)
{
    m_FrameResets++;
}

void ScreenEffect_FadeOut(const int32_t duration, const uint32_t color)
{
    m_FadeFrames = duration;
    m_FadeColor = color;
}

static CHAIN_STATUS M_After(void *const arg)
{
    m_AfterCalls++;
    return CHAIN_CONTINUE;
}

static void M_Setup(void)
{
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    Rng_SetSeed(&g_Rng, 1234);
    g_Buttons = (BUTTONS) {};
    g_GameManager.run.difficulty = 1;
    g_GameManager.player.lives = 3;
    g_GameManager.player.bombs = 2;
    m_Scene = SCENE_GAMEPLAY;
    m_EventCount = 0;
    m_Events[0] = '\0';
    m_Fail = '\0';
    m_ReplayFailed = false;
    m_RestoreReplay = false;
    m_FadeFrames = 0;
    m_FadeColor = 0xffffffff;
    m_AfterCalls = 0;
    m_FrameResets = 0;
    g_Stage.data.song_paths[0] = m_SongPath;
    Sound_Reset();
}

static void M_Start(void)
{
    CHECK(Result_Ignore(GameManager_Register()));
    Chain_InitElem(&m_AfterElem, M_After, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&g_Chain, &m_AfterElem, 6)));
}

TEST(new_game_initializes_records_and_registers_in_original_order)
{
    M_Setup();
    RNG expected = g_Rng;
    uint16_t random_values[32];
    for (int32_t i = 0; i < 2048; i++) {
        const uint16_t value = Rng_U16(&expected);
        if (i < 32) {
            random_values[i] = value;
        }
    }
    M_Start();
    CHECK_EQ_STR(m_Events, "DSPBELFGWA");
    CHECK_EQ_INT(m_LoadGenerated, 2048);
    CHECK_EQ_INT(m_StageGenerated, 0);
    CHECK_EQ_INT(g_Rng.generated, 0);
    CHECK_EQ_INT(g_GameManager.run.rng_seed, expected.seed);
    CHECK_EQ_INT(g_GameManager.run.stage, 1);
    CHECK_EQ_INT(g_GameManager.run.start_lives, 3);
    CHECK_EQ_INT(g_GameManager.run.start_bombs, 2);
    CHECK_EQ_INT(g_GameManager.score.high, 1234567);
    CHECK_EQ_INT(g_GameManager.rank.value, 16);
    CHECK_EQ_INT(g_GameManager.rank.min, 10);
    CHECK_EQ_INT(g_GameManager.rank.max, 32);
    const SPELL_CARD_RECORD *const card = &g_GameManager.records.spell_cards[0];
    CHECK_EQ_INT(card->header.magic, 0x4b544143);
    CHECK_EQ_INT(card->header.reserved[0], random_values[4] >> 8);
    CHECK_EQ_INT(
        card->bonus,
        (int32_t)((uint32_t)random_values[6]
                  | (uint32_t)random_values[7] << 16));
    CHECK_EQ_INT(card->checksum, random_values[9] & 0xff);
    CHECK_EQ_INT(card->history[0], random_values[9] >> 8);
    CHECK_EQ_INT(card->name_bytes[0], random_values[12] & 0xff);
    CHECK_EQ_INT(card->name[0], 0);
    CHECK_EQ_INT(card->attempts, 0);
    CHECK_EQ_INT(card->captures, 0);
    CHECK_EQ_INT(g_GameManager.records.spell_cards[63].index, 63);
}

TEST(new_game_resets_the_frame_counts)
{
    M_Setup();
    M_Start();
    CHECK_EQ_INT(m_FrameResets, 1);
}

TEST(removal_uses_original_module_order_and_stops_playing)
{
    M_Setup();
    M_Start();
    m_EventCount = 0;
    GameManager_Remove();
    CHECK_EQ_STR(m_Events, "sbpelfgrA");
    CHECK(!g_GameManager.mode.playing);
    GameManager_Remove();
    CHECK_EQ_STR(m_Events, "sbpelfgrA");
}

TEST(next_stage_keeps_totals_and_score_before_final_reset)
{
    M_Setup();
    M_Start();
    g_GameManager.score.value = 12345678;
    g_GameManager.player.power = 90;
    g_GameManager.player.point_items_total = 30;
    g_GameManager.player.graze_total = 77;
    g_GameManager.stats.deaths = 2;
    g_GameManager.rank.sub = 88;
    GameManager_Remove();
    m_Scene = SCENE_NEXT_STAGE;
    Rng_SetSeed(&g_Rng, 4321);
    m_EventCount = 0;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_STR(m_Events, "SPBELFGWA");
    CHECK_EQ_INT(g_GameManager.run.stage, 2);
    CHECK_EQ_INT(g_GameManager.score.shown, 12345678);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(g_GameManager.player.power, 90);
    CHECK_EQ_INT(g_GameManager.player.point_items_total, 30);
    CHECK_EQ_INT(g_GameManager.player.graze_total, 77);
    CHECK_EQ_INT(g_GameManager.stats.deaths, 2);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    CHECK_EQ_INT(g_GameManager.run.rng_seed, 4321);
    CHECK_EQ_INT(m_FrameResets, 1);
}

TEST(replay_restores_before_stage_rng_count_resets)
{
    M_Setup();
    g_GameManager.mode.in_replay = true;
    m_RestoreReplay = true;
    M_Start();
    CHECK_EQ_STR(m_Events, "DRSPBELFGA");
    CHECK_EQ_INT(m_ReplayGenerated, 2048);
    CHECK_EQ_INT(g_GameManager.run.rng_seed, 0x9876);
    CHECK_EQ_INT(g_GameManager.score.next_extend, 2);
    CHECK_EQ_INT(g_GameManager.score.shown, 20000000);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(g_GameManager.rank.value, 19);
    CHECK_EQ_INT(g_GameManager.player.power, 80);
}

TEST(replay_failure_finishes_setup_and_requests_title)
{
    M_Setup();
    g_GameManager.mode.in_replay = true;
    m_ReplayFailed = true;
    M_Start();
    CHECK_EQ_STR(m_Events, "DRSPBELFGA");
    CHECK_EQ_INT(m_Scene, SCENE_TITLE);
}

TEST(module_failure_keeps_the_element_for_original_cleanup)
{
    M_Setup();
    m_Fail = 'E';
    CHECK(!Result_Ignore(GameManager_Register()));
    CHECK_EQ_STR(m_Events, "DSPBE");
    CHECK(g_Chain.calc.next != nullptr);
    GameManager_Remove();
    CHECK_EQ_STR(m_Events, "DSPBEsbpelfgrA");
}

TEST(extra_forces_difficulty_and_disables_normal_extends)
{
    M_Setup();
    g_GameManager.run.stage = 6;
    M_Start();
    CHECK_EQ_INT(g_GameManager.run.stage, 7);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 4);
    CHECK_EQ_INT(g_GameManager.rank.min, 14);
    CHECK_EQ_INT(g_GameManager.rank.max, 18);
    CHECK_EQ_INT(g_GameManager.score.next_extend, 4);
}

TEST(practice_uses_preincrement_score_index_and_stage_power)
{
    M_Setup();
    g_GameManager.run.stage = 1;
    g_GameManager.mode.in_practice = true;
    M_Start();
    CHECK_EQ_INT(g_GameManager.score.high, 555);
    CHECK_EQ_INT(g_GameManager.player.power, 64);
    CHECK_EQ_INT(g_GameManager.run.stage, 2);
}

TEST(pause_stops_later_calc_and_draw_changes_its_state)
{
    M_Setup();
    M_Start();
    g_Buttons.pressed.menu = true;
    g_GameManager.play_area.x = 44.0f;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.mode.paused, 1);
    CHECK_EQ_INT(m_AfterCalls, 0);
    CHECK_EQ_INT(g_GameManager.run.stage_frames, 0);
    CHECK(!g_GameManager.mode.playing);
    CHECK(g_GameManager.play_area.x == 32.0f);
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_GameManager.mode.paused, 2);
}

TEST(continue_menu_stops_later_calc_and_score_changes)
{
    M_Setup();
    M_Start();
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.score.value = 100;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_AfterCalls, 0);
    CHECK_EQ_INT(g_GameManager.score.shown, 0);
    CHECK_EQ_INT(g_GameManager.run.stage_frames, 0);
}

TEST(score_counts_up_in_tens_and_extends_at_most_once_per_calc)
{
    M_Setup();
    M_Start();
    g_GameManager.score.value = 20000010;
    g_GameManager.score.shown = 20000000;
    g_GameManager.player.lives = 8;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.shown, 20000010);
    CHECK_EQ_INT(g_GameManager.score.step, 0);
    CHECK_EQ_INT(g_GameManager.score.next_extend, 1);
    CHECK_EQ_INT(g_GameManager.player.lives, 8);
    CHECK_EQ_INT(g_GameManager.rank.value, 18);
    CHECK_EQ_INT(g_GameManager.score.high, 20000010);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.next_extend, 1);
    CHECK_EQ_INT(g_GameManager.run.stage_frames, 2);
}

TEST(extend_queues_its_sound_only_below_eight_lives)
{
    M_Setup();
    M_Start();
    g_GameManager.score.value = 20000010;
    g_GameManager.score.shown = 20000000;
    g_GameManager.player.lives = 8;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    g_GameManager.score.value = 40000010;
    g_GameManager.score.shown = 40000000;
    g_GameManager.player.lives = 2;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_EXTEND);
}

TEST(stage_music_plays_on_add_and_stops_on_removal)
{
    M_Setup();
    M_Start();
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th06_02.mid");
    GameManager_Remove();
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_STOP);
}

TEST(demo_leaves_the_music_alone)
{
    M_Setup();
    g_GameManager.mode.in_demo = true;
    M_Start();
    GameManager_Remove();
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
}

TEST(score_clamps_and_does_not_fall_below_the_shown_score)
{
    M_Setup();
    M_Start();
    g_GameManager.score.next_extend = -1;
    g_GameManager.score.value = 1000000000;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.value, 999999990);
    CHECK_EQ_INT(g_GameManager.score.shown, 78910);
    g_GameManager.score.value = 50;
    g_GameManager.score.shown = 100;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.value, 100);
    CHECK_EQ_INT(g_GameManager.score.shown, 100);
}

TEST(demo_fades_then_returns_to_title_and_accepts_any_button)
{
    M_Setup();
    g_GameManager.mode.in_demo = true;
    M_Start();
    g_GameManager.mode.demo_frames = 3599;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_FadeFrames, 120);
    CHECK_EQ_INT(m_FadeColor, 0);
    CHECK_EQ_INT(m_Scene, SCENE_GAMEPLAY);
    g_GameManager.mode.demo_frames = 3719;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_Scene, SCENE_TITLE);
    m_Scene = SCENE_GAMEPLAY;
    g_Buttons.word = 0x1000;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_Scene, SCENE_TITLE);
}

TEST(camera_uses_truncated_viewport_dimensions)
{
    M_Setup();
    M_Start();
    g_GameManager.play_area.width = 383.75f;
    g_GameManager.play_area.height = 447.75f;
    g_GameManager.camera.zoom = 1.0f;
    Chain_RunCalc(&g_Chain);
    CHECK(g_GameManager.camera.viewport.width == 383.0f);
    CHECK(g_GameManager.camera.viewport.height == 447.0f);
    CHECK(
        g_GameManager.camera.distance
        == (447.0f / 2.0f) / X87_Tan(0.5235988f / 2.0f));
    const MAT4 fixed = g_GameManager.camera.view;
    g_GameManager.camera.offset_x = 10.0f;
    g_GameManager.camera.zoom = 2.0f;
    GameManager_SetUpFixedCamera(0.0f);
    CHECK(memcmp(&fixed, &g_GameManager.camera.view, sizeof(fixed)) == 0);
}

TEST(demo_accepts_partial_release_of_the_whole_button_word)
{
    M_Setup();
    g_GameManager.mode.in_demo = true;
    M_Start();
    g_Buttons.word = 1;
    g_Buttons.previous_word = 3;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_Scene, SCENE_TITLE);
}
