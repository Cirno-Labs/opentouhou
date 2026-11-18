#include "../core/session_native.h"

#include "th06/buttons.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"
#include "th06/main.h"
#include "th06/settings.h"

static RESULT M_Result(uint32_t score);
static RESULT M_Ending(uint32_t score);
static RESULT M_BadEnding(uint32_t score);

static const SCRIPT_BINDING m_Bindings[] = {
#define M_BUTTON(field_, bit_, ...) { #field_, bit_ },
#define M_STICK(...)
    TH06_ACTIONS(M_BUTTON, M_STICK)
#undef M_BUTTON
#undef M_STICK
};

static const SESSION_ACTION m_Actions[] = {
    { "result", M_Result },
    { "ending", M_Ending },
    { "bad-ending", M_BadEnding },
};

static void M_Buttons(void *const input, const uint16_t word)
{
    *(TH06_INPUT *)input = Buttons_FromWord(word);
}

static void M_Prepare(void)
{
    Rng_SetSeed(&g_Rng, 0);
    g_Settings.sound = false;
    g_Settings.music = SETTINGS_MUSIC_OFF;
}

static bool M_CaseReady(void)
{
    return g_GameManager.mode.playing && !g_GameManager.mode.in_demo;
}

static bool M_CaseEnded(void)
{
    return false;
}

static RESULT M_Finish(
    const uint32_t score, const bool continued, const SCENE scene)
{
    FAIL_IF(
        !M_CaseReady() || Scene_Get() != SCENE_GAMEPLAY,
        "finish setup requires gameplay");
    g_GameManager.score.value = score;
    g_GameManager.score.shown = score;
    g_GameManager.stats.continues_used = continued;
    Scene_Request(scene);
    return OK;
}

static RESULT M_Result(const uint32_t score)
{
    return M_Finish(score, false, SCENE_GAME_RESULT);
}

static RESULT M_Ending(const uint32_t score)
{
    return M_Finish(score, false, SCENE_ENDING);
}

static RESULT M_BadEnding(const uint32_t score)
{
    return M_Finish(score, true, SCENE_ENDING);
}

static void M_Point(SESSION_POINT *const point)
{
    const SCENE scene = Scene_Get();
    point->scene = (uint32_t)scene;
    point->requested_scene = (uint32_t)Scene_GetRequested();
    point->buttons = g_Buttons.word;
    point->rng_seed = g_Rng.seed;
    point->rng_generated = (uint32_t)g_Rng.generated;
    point->gameplay = M_CaseReady() && Scene_Get() == SCENE_GAMEPLAY;
    point->score_valid = scene == SCENE_GAMEPLAY || scene == SCENE_ENDING
        || scene == SCENE_GAME_RESULT;
    if (point->score_valid) {
        point->score = g_GameManager.score.value;
        point->shown_score = g_GameManager.score.shown;
    }
}

const SESSION_GAME g_SessionGame = {
    .game = &g_GameDesc,
    .bindings = m_Bindings,
    .binding_count = ARRAY_SIZE(m_Bindings),
    .replay_copy = "replay/th6_ud0000.rpy",
    .set_buttons = M_Buttons,
    .prepare = M_Prepare,
    .comparison = &g_RunnerGame,
    .case_ready = M_CaseReady,
    .case_ended = M_CaseEnded,
    .point = M_Point,
    .actions = m_Actions,
    .action_count = ARRAY_SIZE(m_Actions),
};
