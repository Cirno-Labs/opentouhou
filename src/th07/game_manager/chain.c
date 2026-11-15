#include "th07/game_manager.h"

#include "th07/ascii.h"
#include "th07/bullet.h"
#include "th07/ecl.h"
#include "th07/effect.h"
#include "th07/enemy.h"
#include "th07/game.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screen_effect.h"
#include "th07/settings.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/stubs.h"

#include "core/base/log.h"
#include "core/base/utils.h"
#include "core/math/x87.h"

#define M_CALC_PRIORITY 2
#define M_DRAW_PRIORITY 2
#define M_COUNT_MAX 999999
#define M_SCORE_MAX 999999999

static const struct {
    int32_t value, min, max;
} m_Ranks[] = {
    { 16, 12, 20 }, { 16, 10, 32 }, { 16, 10, 32 },
    { 16, 10, 32 }, { 16, 15, 16 }, { 16, 15, 16 },
};

// By difficulty, from Easy to Phantasm, then by stage in a practice.
static const int32_t m_CherryMaxes[] = {
    200000, 200000, 250000, 300000, 400000, 400000,
};
static const int32_t m_ExtraCherries[] = { 200000, 300000 };
static const int32_t m_PracticeCherryRaises[] = {
    0, 0, 0, 50000, 100000, 150000, 200000,
};

// By demo_index, the calc on which a demo fades out.
static const int32_t m_DemoFadeFrames[] = { 8100, 7020, 4620 };

static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;

static void M_SetRankForDifficulty(GAME_MANAGER *const gm)
{
    gm->rank.value = m_Ranks[gm->run.difficulty].value;
    gm->rank.min = m_Ranks[gm->run.difficulty].min;
    gm->rank.max = m_Ranks[gm->run.difficulty].max;
}

static void M_CountUp(int32_t *const count)
{
    if (*count < M_COUNT_MAX) {
        *count = *count + 1;
    }
}

static void M_ResetPlayArea(GAME_MANAGER *const gm)
{
    gm->play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
}

// Saves score.dat, then reads the high score and the records of the game
// from it. A practice takes its high score from the practice scores.
static RESULT M_LoadScoreFile(GAME_MANAGER *const gm)
{
    Result_Log(ResultScreen_Register(2), LOG_LEVEL_WARNING);
    for (size_t i = 0; i < ARRAY_SIZE(gm->records.spell_captures); i++) {
        gm->records.spell_captures[i] = 0;
    }
    MUST(ResultScreen_LoadGameRecords(), "score file");
    if (gm->mode.in_practice) {
        typeof(gm->records.practice[0][0][0]) *const practice =
            &gm->records.practice[gm->run.shot_index][gm->run.stage]
                                 [gm->run.difficulty];
        gm->score.high = practice->score;
        practice->play_count++;
        gm->score.high_continues = 0;
    }
    return OK;
}

static void M_SetUpCherry(GAME_MANAGER *const gm)
{
    const int32_t difficulty = gm->run.difficulty;
    if (!gm->mode.in_practice) {
        if (difficulty >= 0 && difficulty < 6) {
            gm->cherry.max = gm->cherry.offset + m_CherryMaxes[difficulty];
        }
        if (difficulty >= 4 && difficulty < 6) {
            gm->cherry.value =
                gm->cherry.offset + m_ExtraCherries[difficulty - 4];
        }
        return;
    }
    if (difficulty >= 0 && difficulty < 4) {
        gm->cherry.max = gm->cherry.offset + m_CherryMaxes[difficulty];
    }
    const int32_t stage = gm->run.stage + 1;
    if (stage >= 2 && stage <= 6) {
        gm->cherry.max += m_PracticeCherryRaises[stage];
        gm->cherry.value = gm->cherry.max;
    }
}

static void M_CountPlay(GAME_MANAGER *const gm)
{
    typeof(gm->records.play_counts[0]) *const counts = gm->records.play_counts;
    const int32_t difficulty = gm->run.difficulty;
    const int32_t shot = gm->run.shot_index + 1;
    M_CountUp(&counts[difficulty].plays[0]);
    M_CountUp(&counts[6].plays[0]);
    M_CountUp(&counts[difficulty].plays[shot]);
    M_CountUp(&counts[6].plays[shot]);
    if (Scene_GetRequested() == SCENE_RETRY) {
        M_CountUp(&counts[difficulty].retries);
        M_CountUp(&counts[6].retries);
    }
    if (gm->mode.in_practice) {
        M_CountUp(&counts[difficulty].practices);
        M_CountUp(&counts[6].practices);
    }
}

static RESULT M_NewGame(GAME_MANAGER *const gm)
{
    // The original allocates a block of random size, then fills its
    // checksum keys.
    Rng_U32InRange(&g_Rng, 0xffff);
    gm->cherry.offset = (int32_t)Rng_U32InRange(&g_Rng, 100000) + 6543;
    for (int32_t i = 0; i < 29; i++) {
        Rng_U32InRange(&g_Rng, 100000);
    }
    gm->run.start_lives = (uint8_t)g_Settings.lives;
    gm->run.score_disabled = g_Settings.slowdown;
    gm->player.power_item_combo = 0;
    gm->cherry.value = gm->cherry.offset;
    gm->cherry.plus = gm->cherry.offset;
    if (gm->run.difficulty >= 4) {
        gm->run.start_lives = 2;
    }
    if (gm->mode.in_practice) {
        gm->run.start_lives = 8;
    }
    MUST(Player_Register(), "player");
    if (!gm->mode.in_replay) {
        gm->player.lives = (float)gm->run.start_lives;
        GameManager_DrawChecksumKeys();
        gm->player.bombs = (float)X87_Ftol(g_Player.shot_type.bombs);
    }
    M_ResetPlayArea(gm);
    gm->player_area = (PLAY_RECT) { 8.0f, 16.0f, 368.0f, 416.0f };
    gm->player.power = 0.0f;
    GameManager_DrawChecksumKeys();
    gm->run.progress_frames = 0;
    gm->score.shown = 0;
    gm->score.value = 0;
    gm->score.step = 0;
    gm->score.high = 100000;
    gm->stats.continues_used = 0;
    gm->player.graze_total = 0;
    gm->player.point_items_total = 0;
    if (gm->run.difficulty < 4) {
        gm->player.next_extend_items = 50;
    } else {
        gm->player.next_extend_items = 200;
        gm->run.score_disabled = false;
    }
    gm->player.extends = 0;
    MUST(M_LoadScoreFile(gm));
    M_SetRankForDifficulty(gm);
    gm->stats.deaths = 0.0f;
    GameManager_DrawChecksumKeys();
    gm->stats.bombs_used = 0.0f;
    GameManager_DrawChecksumKeys();
    gm->stats.spell_cards_captured = 0;
    M_SetUpCherry(gm);
    if (gm->mode.in_replay) {
        gm->run.score_disabled = false;
    } else if (!gm->run.score_disabled) {
        M_CountPlay(gm);
    }
    return OK;
}

static RESULT M_Added(void *const arg)
{
    GAME_MANAGER *const gm = arg;
    gm->run.shot_index = (uint8_t)(gm->run.character * 2 + gm->run.shot_type);
    PlayTime_Restart();
    g_GameSpeed = 1.0f;
    if (Scene_GetRequested() != SCENE_NEXT_STAGE) {
        MUST(M_NewGame(gm));
    } else {
        gm->score.shown = gm->score.value;
        gm->score.step = 0;
        MUST(Player_Register(), "player");
    }
    gm->rank.sub = 0;
    gm->player.point_items = 0;
    gm->player.graze = 0;
    gm->mode.paused = 0;
    gm->run.stage++;
    if (!gm->mode.in_replay) {
        uint8_t *const no_continues =
            gm->records.clears[gm->run.shot_index].no_continues;
        uint8_t *const with_continues =
            gm->records.clears[gm->run.shot_index].with_continues;
        const int32_t difficulty = gm->run.difficulty;
        if (gm->stats.continues_used == 0
            && no_continues[difficulty] < gm->run.stage - 1) {
            no_continues[difficulty] = (uint8_t)(gm->run.stage - 1);
        }
        if (with_continues[difficulty] < gm->run.stage - 1) {
            with_continues[difficulty] = (uint8_t)(gm->run.stage - 1);
        }
    }
    if (gm->mode.in_practice && gm->run.stage != 1) {
        gm->player.power = 128.0f;
        GameManager_DrawChecksumKeys();
    }
    bool replay_failed = false;
    if (gm->mode.in_replay) {
        M_SetRankForDifficulty(gm);
        replay_failed = !Result_Log(
            Replay_Register(REPLAY_PLAY, gm->mode.replay_path),
            LOG_LEVEL_ERROR);
        // The draws count in generated, but the seed goes back.
        const uint16_t seed = g_Rng.seed;
        GameManager_DrawChecksumKeys();
        g_Rng.seed = seed;
    }
    gm->run.rng_seed = g_Rng.seed;
    MUST(Stage_Register(), "stage background");
    MUST(Bullet_Register(), "bullets");
    MUST(Enemy_Register(), "enemies");
    MUST(Ecl_Load(), "enemy scripts");
    MUST(Effect_Register(), "effects");
    MUST(Gui_Register(), "interface");
    if (!gm->mode.in_replay) {
        Result_Ignore(Replay_Register(REPLAY_RECORD, "replay/th7_00.rpy"));
    }
    if (gm->run.stage == 6) {
        Sound_StopMusic();
    } else if (g_Stage.data.song_paths[0] != nullptr) {
        Sound_PlayMusic(g_Stage.data.song_paths[0]);
    }
    gm->mode.in_continue_menu = false;
    gm->mode.playing = true;
    gm->score.value = 0;
    gm->mode.game_cleared = false;
    AsciiText_Initialize();
    gm->mode.calc_dropped = false;
    if (replay_failed) {
        Scene_Request(SCENE_TITLE);
    }
    return OK;
}

static void M_Deleted(void *const arg)
{
    GAME_MANAGER *const gm = arg;
    Sound_StopMusic();
    Stage_Remove();
    Bullet_Remove();
    Player_Remove();
    Enemy_Remove();
    Ecl_Release();
    Effect_Remove();
    Gui_Remove();
    Replay_EndStage();
    if (!gm->mode.in_replay) {
        PlayTime_Add();
    }
    PlayTime_Stop();
    gm->mode.playing = false;
    AsciiText_Initialize();
    gm->mode.calc_dropped = false;
    gm->run.stage_frames = 0;
}

// Drops calcs of a slow replay so that it plays at the frame rate it was
// recorded at.
static bool M_DropsSlowPlayback(GAME_MANAGER *const gm)
{
    if (!gm->mode.in_replay || gm->mode.replay_speed != 1
        || Gui_IsMsgRunning()) {
        return false;
    }
    gm->mode.drop_frames++;
    const int16_t fps = gm->mode.replay_fps;
    const uint32_t frames = gm->mode.drop_frames;
    return (fps < 20 && frames % 3 != 0)
        || (fps >= 20 && fps < 30 && frames % 2 != 0)
        || (fps >= 30 && fps < 40 && frames % 3 == 0)
        || (fps >= 40 && fps < 50 && frames % 6 == 0);
}

// Returns whether the demo ended.
static bool M_RunDemo(GAME_MANAGER *const gm)
{
    if (Buttons_WasPressed(&g_Buttons, 0xffff)) {
        Scene_Request(SCENE_TITLE);
    }
    gm->mode.demo_frames++;
    if (gm->mode.demo_index >= ARRAY_SIZE(m_DemoFadeFrames)) {
        return false;
    }
    const int32_t fade = m_DemoFadeFrames[gm->mode.demo_index];
    if (gm->mode.demo_frames == fade) {
        ScreenEffect_FadeOut(120, 0);
        Sound_FadeOutMusic(3.0f);
    }
    if (gm->mode.demo_frames >= fade + 120) {
        Scene_Request(SCENE_TITLE);
        return true;
    }
    return false;
}

static void M_CountScore(GAME_MANAGER *const gm)
{
    if (gm->score.value >= 1000000000) {
        gm->score.value = M_SCORE_MAX;
    }
    if (gm->score.shown == gm->score.value) {
        return;
    }
    if (gm->score.value < gm->score.shown) {
        gm->score.value = gm->score.shown;
    }
    uint32_t step = (gm->score.value - gm->score.shown) >> 5;
    if (step >= 0x8d55e) {
        step = 0x8d55e;
    } else if (step == 0) {
        step = 1;
    }
    if (gm->score.step < step) {
        gm->score.step = step;
    }
    if (gm->score.shown + gm->score.step > gm->score.value) {
        gm->score.step = gm->score.value - gm->score.shown;
    }
    gm->score.shown += gm->score.step;
    if (gm->score.shown >= gm->score.value) {
        gm->score.step = 0;
        gm->score.shown = gm->score.value;
    }
    if (gm->score.high < gm->score.shown) {
        gm->score.high = gm->score.shown;
        gm->score.high_continues = gm->stats.continues_used;
    }
}

// Drops calcs in slow mode, more of them the more bullets show.
static bool M_DropsSlowMode(GAME_MANAGER *const gm)
{
    gm->mode.calc_dropped = false;
    gm->mode.drop_frames++;
    const int32_t bullets = Bullet_Count();
    const uint32_t frames = gm->mode.drop_frames;
    if ((bullets >= 320 && frames % 3 == 0)
        || (bullets < 320 && bullets >= 224 && frames % 4 == 0)
        || (bullets < 224 && bullets >= 128 && frames % 5 == 0)) {
        gm->mode.calc_dropped = true;
        return true;
    }
    if (bullets < 128) {
        gm->mode.drop_frames = 0;
    }
    return false;
}

static CHAIN_STATUS M_Calc(void *const arg)
{
    GAME_MANAGER *const gm = arg;
    if (!gm->mode.in_continue_menu && gm->mode.paused == 0 && !gm->mode.in_demo
        && !gm->mode.calc_dropped && Buttons_WasPressed(&g_Buttons, 0x8)) {
        gm->mode.paused = 1;
        M_ResetPlayArea(gm);
        gm->mode.pause_requested = true;
        if (gm->run.stage != 6 || g_Gui.frames >= 300) {
            Sound_PauseMusic();
        }
        Sound_Queue(SOUND_PAUSE);
        PlayTime_Add();
    }
    gm->camera.viewport = (VIEWPORT) {
        .x = (float)(uint32_t)gm->play_area.x,
        .y = (float)(uint32_t)gm->play_area.y,
        .width = (float)(uint32_t)gm->play_area.width,
        .height = (float)(uint32_t)gm->play_area.height,
        .min_z = 0.0f,
        .max_z = 1.0f,
    };
    if (M_DropsSlowPlayback(gm)) {
        return CHAIN_STOP;
    }
    if (gm->mode.in_demo && M_RunDemo(gm)) {
        return CHAIN_STOP;
    }
    gm->mode.playing = !gm->mode.in_continue_menu && gm->mode.paused == 0;
    if (gm->mode.paused == 1 || gm->mode.paused == 2
        || gm->mode.in_continue_menu) {
        return CHAIN_STOP;
    }
    M_CountScore(gm);
    if (gm->run.score_disabled && M_DropsSlowMode(gm)) {
        return CHAIN_STOP;
    }
    gm->run.stage_frames++;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    GAME_MANAGER *const gm = arg;
    if (gm->mode.paused != 0) {
        gm->mode.paused = 2;
    }
    return CHAIN_CONTINUE;
}

RESULT GameManager_Register(void)
{
    Chain_InitElem(&m_CalcElem, M_Calc, &g_GameManager);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    g_GameManager.run.stage_frames = 0;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, &g_GameManager);
    return Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY);
}

void GameManager_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
    if (g_GameManager.score.value >= 1000000000) {
        g_GameManager.score.value = M_SCORE_MAX;
    }
    g_GameManager.score.shown = g_GameManager.score.value;
}
