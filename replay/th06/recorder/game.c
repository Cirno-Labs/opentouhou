// Fills RECORDER_GAME for th06: sets up practice as the title's practice
// menu does, records through gameplay's own recording, and saves through
// Replay_Save, which writes into a given WRITER.

#include "../../core/recorder.h"

#include "th06/bullet.h"
#include "th06/buttons.h"
#include "th06/enemy.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/player.h"
#include "th06/replay/chain.h"
#include "th06/scene.h"
#include "th06/screens/main.h"

#include "core/base/log.h"
#include "core/io/writer.h"
#include "core/math/vec.h"

// Practice never sets mode.game_cleared: both a clean run and one that
// exhausts every life reach SCENE_GAME_RESULT through it unset
// (gui.c's M_NextScene and PauseMenu_UpdateContinueMenu). Track dying
// instead, through mode.in_continue_menu, which only a death that leaves
// no lives sets.
static bool m_Died;

static bool M_Tick(const void *const input)
{
    const bool result = Game_Tick(input);
    m_Died = m_Died || g_GameManager.mode.in_continue_menu;
    return result;
}

// start_practice does its own GameManager_Create and Game_Init, so that it
// can boot to the title before setting up practice; init has nothing left
// to do.
static RESULT M_Init(void)
{
    return OK;
}

// Boots to the title, then sets up practice exactly as the title's
// practice menu would: M_StartSelect, M_UpdatePracticeStage and
// M_StartGame in th06/screens/main.c.
static RESULT M_StartPractice(
    const int32_t difficulty, const int32_t stage, const int32_t character,
    const int32_t shot_type)
{
    GameManager_Create(&g_GameManager);
    MUST(Game_Init());
    m_Died = false;

    const TH06_INPUT empty = {};
    FAIL_IF(!Game_Tick(&empty), "the game quit on boot");
    FAIL_IF(Scene_Get() != SCENE_TITLE, "the game did not reach the title");

    GAME_MANAGER *const gm = &g_GameManager;
    gm->run.character = (uint8_t)character;
    gm->run.shot_type = (uint8_t)shot_type;
    gm->run.difficulty = difficulty;
    gm->run.stage = stage;
    gm->mode.in_practice = true;
    // The Extra stage and practice always start with two lives and three
    // bombs, as M_StartGame sets them.
    gm->player.lives = 2;
    gm->player.bombs = 3;
    TitleScene_Remove();
    Scene_Request(SCENE_GAMEPLAY);
    return OK;
}

// Inserts offset into the max nearest offsets seen so far, kept sorted by
// distance ascending in offsets[0..*count).
static void M_Insert(
    VEC2 *const offsets, float *const dists, int32_t *const count,
    const int32_t max, const VEC2 offset, const float dist)
{
    int32_t i = *count;
    if (i < max) {
        (*count)++;
    } else if (dist >= dists[max - 1]) {
        return;
    } else {
        i = max - 1;
    }
    while (i > 0 && dists[i - 1] > dist) {
        offsets[i] = offsets[i - 1];
        dists[i] = dists[i - 1];
        i--;
    }
    offsets[i] = offset;
    dists[i] = dist;
}

// Writes the offsets from the player to up to max threats, nearest first;
// reads g_Bullets, g_Enemies, g_Player and the play area only, never
// changes them.
static int32_t M_ListThreats(VEC2 *const offsets, const int32_t max)
{
    if (max > 64) {
        LOG_FATAL("list_threats: max is too large");
    }
    float dists[64];
    int32_t count = 0;
    const VEC2 player = { g_Player.pos.x, g_Player.pos.y };

    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        const BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        const VEC2 pos = { bullet->pos.x, bullet->pos.y };
        const VEC2 offset = Vec2_Sub(pos, player);
        M_Insert(offsets, dists, &count, max, offset, Vec2_Length(offset));
    }
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        const LASER *const laser = &g_Bullets.lasers[i];
        if (!laser->active) {
            continue;
        }
        const VEC2 pos = { laser->pos.x, laser->pos.y };
        const VEC2 offset = Vec2_Sub(pos, player);
        M_Insert(offsets, dists, &count, max, offset, Vec2_Length(offset));
    }
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        const ENEMY *const enemy = &g_Enemies.enemies[i];
        if (!enemy->flags.active) {
            continue;
        }
        const VEC2 pos = { enemy->move.pos.x, enemy->move.pos.y };
        const VEC2 offset = Vec2_Sub(pos, player);
        M_Insert(offsets, dists, &count, max, offset, Vec2_Length(offset));
    }

    const PLAY_RECT *const area = &g_GameManager.play_area;
    const VEC2 edges[4] = {
        { -player.x, 0.0f },
        { area->width - player.x, 0.0f },
        { 0.0f, -player.y },
        { 0.0f, area->height - player.y },
    };
    for (int32_t i = 0; i < 4; i++) {
        M_Insert(offsets, dists, &count, max, edges[i], Vec2_Length(edges[i]));
    }
    return count;
}

static bool M_HasEnded(RECORDER_END *const out)
{
    if (Scene_Get() != SCENE_GAME_RESULT) {
        return false;
    }
    *out = m_Died ? RECORDER_GAME_OVER : RECORDER_STAGE_CLEARED;
    return true;
}

static RESULT M_SaveReplay(const char *const path)
{
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    const REPLAY_SAVE_INFO info = {
        .name = "",
        .date = "01/01/01",
        .achieved_frames = 0.0f,
        .expected_frames = 0.0f,
    };
    const RESULT result = Replay_Save(&writer, &info);
    MUST(Writer_Close(&writer));
    return result;
}

const RECORDER_GAME g_RecorderGame = {
    .program = "th06record",
    .input_schema = &g_InputSchema,
    .open_data = Files_Init,
    .close_data = Files_Shutdown,
    .init = M_Init,
    .start_practice = M_StartPractice,
    .tick = M_Tick,
    .list_threats = M_ListThreats,
    .has_ended = M_HasEnded,
    .save_replay = M_SaveReplay,
    .shutdown = Game_Shutdown,
};
