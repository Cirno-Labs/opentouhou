#include "recorder.h"

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

#include "core/base/utils.h"
#include "core/io/writer.h"

#include <string.h>

// Records a th06 practice stage as th06record.

#define M_MAX_THREATS 64

static const char *const m_Difficulties[] = {
    "easy", "normal", "hard", "lunatic", "extra",
};
static const char *const m_Stages[] = { "1", "2", "3", "4", "5", "6", "extra" };
static const char *const m_Characters[] = { "reimu", "marisa" };
static const char *const m_ShotTypes[] = { "a", "b" };

// Tells whether the player died with no lives left: practice reaches the
// result scene both after a clear and after the last life, and sets
// mode.in_continue_menu only on the second.
static bool m_Died;
static bool m_Started;

static RESULT M_Start(
    const int32_t difficulty, const int32_t stage, const int32_t character,
    const int32_t shot_type)
{
    MUST(Files_Mount());
    GameManager_Create(&g_GameManager);
    m_Started = true;
    MUST(Game_Init());
    m_Died = false;
    const TH06_INPUT none = {};
    FAIL_IF(!Game_Tick(&none), "the game quit at boot");
    FAIL_IF(Scene_Get() != SCENE_TITLE, "the game did not reach the title");
    // Sets practice up as the title's practice menu does.
    GAME_MANAGER *const gm = &g_GameManager;
    gm->run.character = (uint8_t)character;
    gm->run.shot_type = (uint8_t)shot_type;
    gm->run.difficulty = difficulty;
    gm->run.stage = stage;
    gm->mode.in_practice = true;
    gm->player.lives = 2;
    gm->player.bombs = 3;
    TitleScene_Remove();
    Scene_Request(SCENE_GAMEPLAY);
    return OK;
}

static bool M_Tick(const void *const input)
{
    const bool running = Game_Tick(input);
    m_Died = m_Died || g_GameManager.mode.in_continue_menu;
    return running;
}

// Inserts offset into offsets, which holds *count offsets nearest first,
// keeping at most max.
static void M_Insert(
    VEC2 *const offsets, float *const distances, int32_t *const count,
    const int32_t max, const VEC2 offset)
{
    const float distance = offset.x * offset.x + offset.y * offset.y;
    int32_t i = *count;
    if (i < max) {
        (*count)++;
    } else if (distance >= distances[max - 1]) {
        return;
    } else {
        i = max - 1;
    }
    for (; i > 0 && distances[i - 1] > distance; i--) {
        offsets[i] = offsets[i - 1];
        distances[i] = distances[i - 1];
    }
    offsets[i] = offset;
    distances[i] = distance;
}

static VEC2 M_Offset(const VEC3 pos)
{
    return (VEC2) { pos.x - g_Player.pos.x, pos.y - g_Player.pos.y };
}

static int32_t M_ListThreats(VEC2 *const offsets, const int32_t max)
{
    float distances[M_MAX_THREATS];
    const int32_t limit = MIN(max, M_MAX_THREATS);
    int32_t count = 0;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        if (g_Bullets.bullets[i].state != BULLET_FREE) {
            M_Insert(
                offsets, distances, &count, limit,
                M_Offset(g_Bullets.bullets[i].pos));
        }
    }
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        if (g_Bullets.lasers[i].active) {
            M_Insert(
                offsets, distances, &count, limit,
                M_Offset(g_Bullets.lasers[i].pos));
        }
    }
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        if (g_Enemies.enemies[i].flags.active) {
            M_Insert(
                offsets, distances, &count, limit,
                M_Offset(g_Enemies.enemies[i].move.pos));
        }
    }
    const PLAY_RECT *const area = &g_GameManager.play_area;
    const VEC2 edges[] = {
        { -g_Player.pos.x, 0.0f },
        { area->width - g_Player.pos.x, 0.0f },
        { 0.0f, -g_Player.pos.y },
        { 0.0f, area->height - g_Player.pos.y },
    };
    for (size_t i = 0; i < ARRAY_SIZE(edges); i++) {
        M_Insert(offsets, distances, &count, limit, edges[i]);
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

static RESULT M_Save(const char *const path)
{
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    const REPLAY_SAVE_INFO info = { .name = "", .date = "01/01/01" };
    const RESULT result = Replay_Save(&writer, &info);
    const RESULT closed = Writer_Close(&writer);
    MUST(result);
    return closed;
}

static void M_Stop(void)
{
    if (m_Started) {
        Game_Shutdown();
    }
}

int main(int argc, char **argv)
{
    const RECORDER_GAME game = {
        .program = "th06record",
        .input_schema = &g_InputSchema,
        .bot_ini = "replay/th06/bot.ini",
        .difficulties = { m_Difficulties, ARRAY_SIZE(m_Difficulties) },
        .stages = { m_Stages, ARRAY_SIZE(m_Stages) },
        .characters = { m_Characters, ARRAY_SIZE(m_Characters) },
        .shot_types = { m_ShotTypes, ARRAY_SIZE(m_ShotTypes) },
        .start = M_Start,
        .tick = M_Tick,
        .list_threats = M_ListThreats,
        .has_ended = M_HasEnded,
        .save = M_Save,
        .stop = M_Stop,
    };
    return Recorder_Main(&game, argc, argv);
}
