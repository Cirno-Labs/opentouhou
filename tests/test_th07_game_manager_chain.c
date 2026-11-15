#include "th07/game_manager.h"

#include "test.h"

#include "th07/ascii.h"
#include "th07/ecl.h"
#include "th07/enemy.h"
#include "th07/effect.h"
#include "th07/game.h"
#include "th07/gui.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screen_effect.h"
#include "th07/settings.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/player.h"
#include "th07/stubs.h"

#include <string.h>

CHAIN g_Chain;
RNG g_Rng;
BUTTONS g_Buttons;
float g_GameSpeed;
STAGE g_Stage;
TH07_SETTINGS g_Settings;
PLAYER g_Player;
GUI g_Gui;

static char m_SongPath[] = "bgm/th07_02.mid";

static SCENE m_Scene;
static char m_Events[128];
static int32_t m_EventCount;
static char m_Fail;
static bool m_ReplayFails;
static int32_t m_PlayerGenerated;
static int32_t m_StageGenerated;
static uint16_t m_StageSeed;
static int32_t m_ReplayGenerated;
static uint16_t m_ReplaySeed;
static int32_t m_BulletCount;
static bool m_MsgRunning;
static int32_t m_SaveMode;
static int32_t m_FadeFrames;
static float m_MusicFade;
static int32_t m_PlayTimeAdds;
static int32_t m_PlayTimeRestarts;
static int32_t m_PlayTimeStops;
static int32_t m_AfterCalls;
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

RESULT Player_Register(void)
{
    m_PlayerGenerated = g_Rng.generated;
    return M_Register('P');
}

RESULT Stage_Register(void)
{
    m_StageGenerated = g_Rng.generated;
    m_StageSeed = g_Rng.seed;
    return M_Register('S');
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
    M_Event(mode == REPLAY_PLAY ? 'R' : 'W');
    if (mode == REPLAY_PLAY) {
        CHECK_EQ_STR(path, g_GameManager.mode.replay_path);
        Rng_SetSeed(&g_Rng, 0x9876);
        m_ReplayGenerated = g_Rng.generated;
        m_ReplaySeed = g_Rng.seed;
    } else {
        CHECK_EQ_STR(path, "replay/th7_00.rpy");
    }
    return m_ReplayFails ? FAIL("test replay failure") : OK;
}

RESULT ResultScreen_Register(const int32_t mode)
{
    M_Event('D');
    m_SaveMode = mode;
    return OK;
}

RESULT ResultScreen_LoadGameRecords(void)
{
    M_Event('H');
    CHECK_EQ_INT(g_GameManager.records.spell_captures[5], 0);
    g_GameManager.score.high = 1234567;
    g_GameManager.score.high_continues = 2;
    return m_Fail == 'H' ? FAIL("test score failure") : OK;
}

void PlayTime_Add(void)
{
    m_PlayTimeAdds++;
}

void PlayTime_Restart(void)
{
    m_PlayTimeRestarts++;
}

void PlayTime_Stop(void)
{
    m_PlayTimeStops++;
}

bool Gui_IsMsgRunning(void)
{
    return m_MsgRunning;
}

int32_t Bullet_Count(void)
{
    return m_BulletCount;
}

void ScreenEffect_FadeOut(const int32_t duration, const uint32_t color)
{
    m_FadeFrames = duration;
    CHECK_EQ_INT(color, 0);
}

void Sound_Queue(const int32_t sound)
{
    M_Event(sound == SOUND_PAUSE ? 'Q' : '?');
}

void Sound_PlayMusic(const char *const path)
{
    CHECK_EQ_STR(path, m_SongPath);
    M_Event('M');
}

void Sound_StopMusic(void)
{
    M_Event('m');
}

void Sound_PauseMusic(void)
{
    M_Event('Z');
}

void Sound_FadeOutMusic(const float seconds)
{
    m_MusicFade = seconds;
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
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_GameSpeed = 0.5f;
    memset(&g_Settings, 0, sizeof(g_Settings));
    g_Settings.lives = 3;
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.shot_type.bombs = 3.0f;
    memset(&g_Gui, 0, sizeof(g_Gui));
    g_GameManager.run.difficulty = 1;
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 1;
    g_GameManager.records.spell_captures[5] = 7;
    strcpy(g_GameManager.mode.replay_path, "replay/test.rpy");
    m_Scene = SCENE_GAMEPLAY;
    m_EventCount = 0;
    m_Events[0] = '\0';
    m_Fail = '\0';
    m_ReplayFails = false;
    m_BulletCount = 0;
    m_MsgRunning = false;
    m_SaveMode = -1;
    m_FadeFrames = 0;
    m_MusicFade = 0.0f;
    m_PlayTimeAdds = 0;
    m_PlayTimeRestarts = 0;
    m_PlayTimeStops = 0;
    m_AfterCalls = 0;
    g_Stage.data.song_paths[0] = m_SongPath;
}

static void M_Start(void)
{
    CHECK(Result_Ignore(GameManager_Register()));
    Chain_InitElem(&m_AfterElem, M_After, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&g_Chain, &m_AfterElem, 3)));
    m_EventCount = 0;
    m_Events[0] = '\0';
}

// Returns the generator after count draws of Rng_U32InRange.
static RNG M_After32(const RNG start, const int32_t count)
{
    RNG rng = start;
    for (int32_t i = 0; i < count; i++) {
        Rng_U32InRange(&rng, 100000);
    }
    return rng;
}

// Returns the cherry offset that a new game draws from start.
static int32_t M_CherryOffset(const RNG start)
{
    RNG rng = start;
    Rng_U32InRange(&rng, 0xffff);
    return (int32_t)Rng_U32InRange(&rng, 100000) + 6543;
}

TEST(new_game_registers_in_original_order)
{
    M_Setup();
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_STR(m_Events, "PDHSBELFGWMA");
    CHECK_EQ_INT(m_SaveMode, 2);
    CHECK(g_GameSpeed == 1.0f);
    CHECK_EQ_INT(m_PlayTimeRestarts, 1);
    CHECK_EQ_INT(g_GameManager.run.stage, 1);
    CHECK_EQ_INT(g_GameManager.run.shot_index, 3);
    CHECK(g_GameManager.mode.playing);
}

TEST(new_game_takes_the_checksum_numbers_in_order)
{
    M_Setup();
    const RNG start = g_Rng;
    CHECK(Result_Ignore(GameManager_Register()));
    // The block size and the 30 keys come before the player; the lives,
    // power, deaths and bombs used take two each after it.
    CHECK_EQ_INT(m_PlayerGenerated, 62);
    CHECK_EQ_INT(m_StageGenerated, 78);
    CHECK_EQ_INT(m_StageSeed, M_After32(start, 39).seed);
    CHECK_EQ_INT(g_GameManager.run.rng_seed, m_StageSeed);
    CHECK_EQ_INT(g_GameManager.cherry.offset, M_CherryOffset(start));
}

TEST(a_practice_past_stage_1_takes_two_more_for_the_power)
{
    M_Setup();
    g_GameManager.mode.in_practice = true;
    g_GameManager.run.stage = 2;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(m_StageGenerated, 82);
    CHECK(g_GameManager.player.power == 128.0f);
    M_Setup();
    g_GameManager.mode.in_practice = true;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(m_StageGenerated, 78);
    CHECK(g_GameManager.player.power == 0.0f);
}

TEST(a_replay_takes_its_numbers_and_keeps_the_replay_seed)
{
    M_Setup();
    g_GameManager.mode.in_replay = true;
    g_GameManager.player.lives = 5.0f;
    g_GameManager.rank.value = 3;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_STR(m_Events, "PDHRSBELFGMA");
    CHECK_EQ_INT(m_PlayerGenerated, 62);
    CHECK(g_GameManager.player.lives == 5.0f);
    CHECK_EQ_INT(m_ReplayGenerated, 0);
    CHECK_EQ_INT(m_StageSeed, m_ReplaySeed);
    CHECK_EQ_INT(m_StageGenerated, 4);
    CHECK_EQ_INT(g_GameManager.rank.value, 16);
}

TEST(a_failed_replay_returns_to_the_title)
{
    M_Setup();
    g_GameManager.mode.in_replay = true;
    m_ReplayFails = true;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(m_Scene, SCENE_TITLE);
}

TEST(new_game_sets_the_starting_counts)
{
    M_Setup();
    g_Settings.slowdown = true;
    g_GameManager.score.value = 500;
    g_GameManager.stats.deaths = 4.0f;
    g_GameManager.stats.continues_used = 2;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.run.start_lives, 3);
    CHECK(g_GameManager.run.score_disabled);
    CHECK(g_GameManager.player.lives == 3.0f);
    CHECK(g_GameManager.player.bombs == 3.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(g_GameManager.score.high, 1234567);
    CHECK_EQ_INT(g_GameManager.score.high_continues, 2);
    CHECK_EQ_INT(g_GameManager.stats.continues_used, 0);
    CHECK(g_GameManager.stats.deaths == 0.0f);
    CHECK_EQ_INT(g_GameManager.player.next_extend_items, 50);
    CHECK_EQ_INT(g_GameManager.rank.min, 10);
    CHECK_EQ_INT(g_GameManager.rank.max, 32);
    CHECK(g_GameManager.player_area.width == 368.0f);
    CHECK_EQ_INT(g_GameManager.records.spell_captures[5], 0);
}

TEST(extra_and_practice_set_their_lives)
{
    M_Setup();
    g_Settings.slowdown = true;
    g_GameManager.run.difficulty = 4;
    g_GameManager.run.stage = 6;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.run.start_lives, 2);
    CHECK(!g_GameManager.run.score_disabled);
    CHECK_EQ_INT(g_GameManager.player.next_extend_items, 200);
    CHECK_EQ_INT(g_GameManager.rank.min, 15);
    CHECK_EQ_INT(g_GameManager.rank.max, 16);
    M_Setup();
    g_GameManager.mode.in_practice = true;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.run.start_lives, 8);
    CHECK(g_GameManager.player.lives == 8.0f);
}

TEST(the_cherry_starts_by_difficulty)
{
    const int32_t maxes[] = { 200000, 200000, 250000, 300000, 400000, 400000 };
    const int32_t values[] = { 0, 0, 0, 0, 200000, 300000 };
    for (int32_t difficulty = 0; difficulty < 6; difficulty++) {
        M_Setup();
        g_GameManager.run.difficulty = difficulty;
        g_GameManager.cherry.value = 5;
        g_GameManager.cherry.plus = 5;
        const int32_t offset = M_CherryOffset(g_Rng);
        CHECK(Result_Ignore(GameManager_Register()));
        CHECK_EQ_INT(g_GameManager.cherry.offset, offset);
        CHECK_EQ_INT(g_GameManager.cherry.max, offset + maxes[difficulty]);
        CHECK_EQ_INT(g_GameManager.cherry.value, offset + values[difficulty]);
        CHECK_EQ_INT(g_GameManager.cherry.plus, offset);
    }
}

TEST(a_practice_cherry_starts_full_past_stage_1)
{
    const int32_t maxes[] = { 250000, 250000, 300000, 350000, 400000, 450000 };
    for (int32_t stage = 0; stage < 6; stage++) {
        M_Setup();
        g_GameManager.mode.in_practice = true;
        g_GameManager.run.difficulty = 2;
        g_GameManager.run.stage = stage;
        const int32_t offset = M_CherryOffset(g_Rng);
        CHECK(Result_Ignore(GameManager_Register()));
        CHECK_EQ_INT(g_GameManager.cherry.max, offset + maxes[stage]);
        CHECK_EQ_INT(
            g_GameManager.cherry.value,
            offset + (stage == 0 ? 0 : maxes[stage]));
    }
}

TEST(a_practice_takes_its_high_score_and_counts_its_play)
{
    M_Setup();
    g_GameManager.mode.in_practice = true;
    g_GameManager.run.difficulty = 3;
    g_GameManager.run.stage = 4;
    g_GameManager.records.practice[3][4][3].score = 7654;
    g_GameManager.records.practice[3][4][3].play_count = 9;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.score.high, 7654);
    CHECK_EQ_INT(g_GameManager.score.high_continues, 0);
    CHECK_EQ_INT(g_GameManager.records.practice[3][4][3].play_count, 10);
}

TEST(new_game_counts_the_play_retry_and_practice)
{
    M_Setup();
    m_Scene = SCENE_RETRY;
    g_GameManager.mode.in_practice = true;
    g_GameManager.records.play_counts[6].plays[4] = 999999;
    CHECK(Result_Ignore(GameManager_Register()));
    typeof(g_GameManager.records.play_counts[0]) *const counts =
        g_GameManager.records.play_counts;
    CHECK_EQ_INT(counts[1].plays[0], 1);
    CHECK_EQ_INT(counts[6].plays[0], 1);
    CHECK_EQ_INT(counts[1].plays[4], 1);
    CHECK_EQ_INT(counts[6].plays[4], 999999);
    CHECK_EQ_INT(counts[1].retries, 1);
    CHECK_EQ_INT(counts[6].retries, 1);
    CHECK_EQ_INT(counts[1].practices, 1);
    CHECK_EQ_INT(counts[6].practices, 1);
}

TEST(slow_mode_and_replays_count_no_play)
{
    M_Setup();
    g_Settings.slowdown = true;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.records.play_counts[6].plays[0], 0);
    M_Setup();
    g_GameManager.mode.in_replay = true;
    g_GameManager.run.score_disabled = true;
    g_Settings.slowdown = true;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.records.play_counts[6].plays[0], 0);
    CHECK(!g_GameManager.run.score_disabled);
}

TEST(a_next_stage_keeps_the_game)
{
    M_Setup();
    m_Scene = SCENE_NEXT_STAGE;
    g_GameManager.run.stage = 2;
    g_GameManager.score.value = 4321;
    g_GameManager.score.step = 9;
    g_GameManager.score.high = 99;
    g_GameManager.player.graze = 30;
    g_GameManager.player.point_items = 40;
    g_GameManager.rank.sub = 50;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_STR(m_Events, "PSBELFGWMA");
    CHECK_EQ_INT(m_StageGenerated, 0);
    CHECK_EQ_INT(g_GameManager.run.stage, 3);
    CHECK_EQ_INT(g_GameManager.score.shown, 4321);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(g_GameManager.score.step, 0);
    CHECK_EQ_INT(g_GameManager.score.high, 99);
    CHECK_EQ_INT(g_GameManager.player.graze, 0);
    CHECK_EQ_INT(g_GameManager.player.point_items, 0);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
}

TEST(stage_6_stops_the_music)
{
    M_Setup();
    m_Scene = SCENE_NEXT_STAGE;
    g_GameManager.run.stage = 5;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_STR(m_Events, "PSBELFGWmA");
}

TEST(a_stage_records_the_stages_cleared)
{
    M_Setup();
    m_Scene = SCENE_NEXT_STAGE;
    g_GameManager.run.stage = 3;
    g_GameManager.stats.continues_used = 1;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.records.clears[3].no_continues[1], 0);
    CHECK_EQ_INT(g_GameManager.records.clears[3].with_continues[1], 3);
    GameManager_Remove();
    g_GameManager.stats.continues_used = 0;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.records.clears[3].no_continues[1], 4);
    GameManager_Remove();
    g_GameManager.mode.in_replay = true;
    CHECK(Result_Ignore(GameManager_Register()));
    CHECK_EQ_INT(g_GameManager.records.clears[3].no_continues[1], 4);
}

TEST(a_failed_setup_fails_the_register)
{
    const char steps[] = "PHSBELFG";
    for (size_t i = 0; i < strlen(steps); i++) {
        M_Setup();
        m_Fail = steps[i];
        CHECK(!Result_Ignore(GameManager_Register()));
    }
}

TEST(score_counts_up_by_a_32nd_from_1_to_its_limit)
{
    M_Setup();
    M_Start();
    g_GameManager.score.value = 31;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.shown, 1);
    g_GameManager.score.value = 100000000;
    g_GameManager.score.step = 0;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.step, 0x8d55e);
    CHECK_EQ_INT(g_GameManager.score.shown, 1 + 0x8d55e);
    CHECK_EQ_INT(g_GameManager.score.high, 1234567);
    g_GameManager.score.value = 1000;
    g_GameManager.score.shown = 990;
    g_GameManager.score.step = 3;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.shown, 993);
    g_GameManager.score.step = 50;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.shown, 1000);
    CHECK_EQ_INT(g_GameManager.score.step, 0);
}

TEST(score_drops_back_to_the_shown_score)
{
    M_Setup();
    M_Start();
    g_GameManager.score.shown = 500;
    g_GameManager.score.value = 0;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.value, 500);
    CHECK_EQ_INT(g_GameManager.score.shown, 500);
}

TEST(a_shown_score_past_the_high_score_raises_it)
{
    M_Setup();
    M_Start();
    g_GameManager.stats.continues_used = 3;
    g_GameManager.score.shown = 1234567;
    g_GameManager.score.value = 1234567 + 32 * 5;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.high, 1234567 + 5);
    CHECK_EQ_INT(g_GameManager.score.high_continues, 3);
}

TEST(score_caps_at_999999999_tens)
{
    M_Setup();
    M_Start();
    g_GameManager.score.value = 1000000000;
    g_GameManager.score.shown = 999999999;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.score.value, 999999999);
    CHECK_EQ_INT(g_GameManager.score.shown, 999999999);
}

TEST(remove_caps_the_score_and_shows_it_in_full)
{
    M_Setup();
    M_Start();
    g_GameManager.score.value = 1500000000;
    g_GameManager.score.shown = 10;
    GameManager_Remove();
    CHECK_EQ_STR(m_Events, "msbpelfgrA");
    CHECK_EQ_INT(g_GameManager.score.value, 999999999);
    CHECK_EQ_INT(g_GameManager.score.shown, 999999999);
    CHECK(!g_GameManager.mode.playing);
    CHECK_EQ_INT(m_PlayTimeAdds, 1);
    CHECK_EQ_INT(m_PlayTimeStops, 1);
    CHECK_EQ_INT(g_GameManager.run.stage_frames, 0);
}

TEST(a_replay_adds_no_play_time)
{
    M_Setup();
    g_GameManager.mode.in_replay = true;
    M_Start();
    GameManager_Remove();
    CHECK_EQ_INT(m_PlayTimeAdds, 0);
    CHECK_EQ_INT(m_PlayTimeStops, 1);
}

TEST(the_menu_button_pauses_and_puts_the_play_area_back)
{
    M_Setup();
    M_Start();
    g_GameManager.play_area.x = 40.0f;
    g_Buttons.word = 0x8;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.mode.paused, 1);
    CHECK(g_GameManager.mode.pause_requested);
    CHECK(g_GameManager.play_area.x == 32.0f);
    CHECK(g_GameManager.camera.viewport.x == 32.0f);
    CHECK(g_GameManager.camera.viewport.max_z == 1.0f);
    CHECK_EQ_STR(m_Events, "ZQ");
    CHECK_EQ_INT(m_PlayTimeAdds, 1);
    CHECK(!g_GameManager.mode.playing);
    CHECK_EQ_INT(m_AfterCalls, 0);
    CHECK_EQ_INT(g_GameManager.run.stage_frames, 0);
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_GameManager.mode.paused, 2);
}

TEST(stage_6_pauses_without_the_music_early)
{
    M_Setup();
    m_Scene = SCENE_NEXT_STAGE;
    g_GameManager.run.stage = 5;
    M_Start();
    g_Gui.frames = 299;
    g_Buttons.word = 0x8;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_STR(m_Events, "Q");
}

TEST(no_pause_in_a_demo_menu_or_dropped_calc)
{
    M_Setup();
    M_Start();
    g_Buttons.word = 0x8;
    g_GameManager.mode.calc_dropped = true;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.mode.paused, 0);
    g_GameManager.mode.calc_dropped = false;
    g_GameManager.mode.in_continue_menu = true;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.mode.paused, 0);
    g_GameManager.mode.in_continue_menu = false;
    g_GameManager.mode.in_demo = true;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.mode.paused, 0);
}

TEST(a_running_game_counts_its_frames)
{
    M_Setup();
    M_Start();
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_GameManager.run.stage_frames, 2);
    CHECK_EQ_INT(m_AfterCalls, 2);
    CHECK(g_GameManager.mode.playing);
}

TEST(a_demo_fades_out_and_returns_by_its_index)
{
    const int32_t fades[] = { 8100, 7020, 4620 };
    for (int32_t index = 0; index < 3; index++) {
        M_Setup();
        g_GameManager.mode.in_demo = true;
        g_GameManager.mode.demo_index = (uint8_t)index;
        M_Start();
        g_GameManager.mode.demo_frames = fades[index] - 1;
        Chain_RunCalc(&g_Chain);
        CHECK_EQ_INT(m_FadeFrames, 120);
        CHECK(m_MusicFade == 3.0f);
        g_GameManager.mode.demo_frames = fades[index] + 118;
        Chain_RunCalc(&g_Chain);
        CHECK_EQ_INT(m_Scene, SCENE_GAMEPLAY);
        CHECK_EQ_INT(m_AfterCalls, 2);
        Chain_RunCalc(&g_Chain);
        CHECK_EQ_INT(m_Scene, SCENE_TITLE);
        CHECK_EQ_INT(m_AfterCalls, 2);
    }
}

TEST(any_button_ends_a_demo)
{
    M_Setup();
    g_GameManager.mode.in_demo = true;
    M_Start();
    g_Buttons.word = 0x100;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_Scene, SCENE_TITLE);
    CHECK_EQ_INT(g_GameManager.mode.demo_frames, 1);
}

// Returns the calcs of the first calcs that slow playback drops at fps.
static int32_t M_SlowPlaybackDrops(const int16_t fps, const int32_t calcs)
{
    M_Setup();
    g_GameManager.mode.in_replay = true;
    g_GameManager.mode.replay_speed = 1;
    M_Start();
    g_GameManager.mode.replay_fps = fps;
    for (int32_t i = 0; i < calcs; i++) {
        Chain_RunCalc(&g_Chain);
    }
    return calcs - m_AfterCalls;
}

TEST(slow_playback_drops_calcs_by_the_frame_rate)
{
    CHECK_EQ_INT(M_SlowPlaybackDrops(19, 12), 8);
    CHECK_EQ_INT(M_SlowPlaybackDrops(19, 1), 1);
    CHECK_EQ_INT(M_SlowPlaybackDrops(20, 12), 6);
    CHECK_EQ_INT(M_SlowPlaybackDrops(29, 1), 1);
    CHECK_EQ_INT(M_SlowPlaybackDrops(30, 12), 4);
    CHECK_EQ_INT(M_SlowPlaybackDrops(39, 2), 0);
    CHECK_EQ_INT(M_SlowPlaybackDrops(40, 12), 2);
    CHECK_EQ_INT(M_SlowPlaybackDrops(49, 5), 0);
    CHECK_EQ_INT(M_SlowPlaybackDrops(50, 12), 0);
    CHECK_EQ_INT(g_GameManager.mode.drop_frames, 12);
}

TEST(slow_playback_drops_nothing_during_dialogue)
{
    M_Setup();
    g_GameManager.mode.in_replay = true;
    g_GameManager.mode.replay_speed = 1;
    m_MsgRunning = true;
    M_Start();
    g_GameManager.mode.replay_fps = 10;
    for (int32_t i = 0; i < 6; i++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK_EQ_INT(m_AfterCalls, 6);
    CHECK_EQ_INT(g_GameManager.mode.drop_frames, 0);
}

// Returns the calcs of 60 that slow mode drops with bullets in play.
static int32_t M_SlowModeDrops(const int32_t bullets)
{
    M_Setup();
    M_Start();
    g_GameManager.run.score_disabled = true;
    m_BulletCount = bullets;
    int32_t dropped = 0;
    for (int32_t i = 0; i < 60; i++) {
        Chain_RunCalc(&g_Chain);
        dropped += g_GameManager.mode.calc_dropped;
    }
    CHECK_EQ_INT(dropped, 60 - m_AfterCalls);
    return dropped;
}

TEST(slow_mode_drops_calcs_by_the_bullet_count)
{
    CHECK_EQ_INT(M_SlowModeDrops(320), 20);
    CHECK_EQ_INT(M_SlowModeDrops(319), 15);
    CHECK_EQ_INT(M_SlowModeDrops(224), 15);
    CHECK_EQ_INT(M_SlowModeDrops(223), 12);
    CHECK_EQ_INT(M_SlowModeDrops(128), 12);
    CHECK_EQ_INT(M_SlowModeDrops(127), 0);
    CHECK_EQ_INT(g_GameManager.mode.drop_frames, 0);
}

TEST(a_dropped_calc_still_counts_the_score)
{
    M_Setup();
    M_Start();
    g_GameManager.run.score_disabled = true;
    m_BulletCount = 320;
    g_GameManager.mode.drop_frames = 2;
    g_GameManager.score.value = 64;
    Chain_RunCalc(&g_Chain);
    CHECK(g_GameManager.mode.calc_dropped);
    CHECK_EQ_INT(g_GameManager.score.shown, 2);
    CHECK_EQ_INT(g_GameManager.run.stage_frames, 0);
}
