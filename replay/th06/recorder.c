#include "recorder.h"

#include "th06/buttons.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/replay/chain.h"
#include "th06/scene.h"
#include "th06/screens/title.h"

#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/writer.h"

#include <stdio.h>
#include <string.h>

// Records a th06 practice stage as th06record. The start values
// difficulty, stage, character and shot choose it, as the title's practice
// menu does.

typedef enum {
    M_START_DIFFICULTY,
    M_START_STAGE,
    M_START_CHARACTER,
    M_START_SHOT,
    M_START_COUNT,
} M_START_INDEX;

// Names a start value and its values, in the order of their numbers.
typedef struct {
    const char *name;
    const char *const *values;
    int32_t count;
} M_START;

static const char *const m_Difficulties[] = {
    "easy", "normal", "hard", "lunatic", "extra",
};
static const char *const m_Stages[] = { "1", "2", "3", "4", "5", "6", "extra" };
static const char *const m_Characters[] = { "reimu", "marisa" };
static const char *const m_ShotTypes[] = { "a", "b" };

static const M_START m_StartNames[M_START_COUNT] = {
    { "difficulty", m_Difficulties, ARRAY_SIZE(m_Difficulties) },
    { "stage", m_Stages, ARRAY_SIZE(m_Stages) },
    { "character", m_Characters, ARRAY_SIZE(m_Characters) },
    { "shot", m_ShotTypes, ARRAY_SIZE(m_ShotTypes) },
};

// Holds each start value as its index among the names of its choice, or -1
// while it is unset.
static int32_t m_Starts[M_START_COUNT] = { -1, -1, -1, -1 };
static bool m_Started;
static int32_t m_Lives;
static int32_t m_Setbacks;

static RESULT M_SetStart(const char *const name, const char *const value)
{
    for (int32_t i = 0; i < M_START_COUNT; i++) {
        const M_START *const start = &m_StartNames[i];
        if (strcmp(start->name, name) != 0) {
            continue;
        }
        char names[128] = "";
        for (int32_t j = 0; j < start->count; j++) {
            if (Str_EqualIgnoreCase(start->values[j], value)) {
                m_Starts[i] = j;
                return OK;
            }
            snprintf(
                names + strlen(names), sizeof(names) - strlen(names), "%s%s",
                j > 0 ? ", " : "", start->values[j]);
        }
        return FAIL("%s takes one of %s, not %s", name, names, value);
    }
    return FAIL(
        "there is no start value %s: give difficulty, stage, character "
        "and shot",
        name);
}

static RESULT M_Start(void)
{
    for (int32_t i = 0; i < M_START_COUNT; i++) {
        FAIL_IF(
            m_Starts[i] < 0, "the start value %s is missing",
            m_StartNames[i].name);
    }
    MUST(Files_Mount());
    Rng_SetSeed(&g_Rng, 0);
    GameManager_Create(&g_GameManager);
    m_Started = true;
    MUST(Game_Init());
    const TH06_INPUT none = {};
    FAIL_IF(!Game_Tick(&none), "the game quit at boot");
    FAIL_IF(Scene_Get() != SCENE_TITLE, "the game did not reach the title");
    // Sets practice up as the title's practice menu does.
    GAME_MANAGER *const gm = &g_GameManager;
    gm->run.character = (uint8_t)m_Starts[M_START_CHARACTER];
    gm->run.shot_type = (uint8_t)m_Starts[M_START_SHOT];
    gm->run.difficulty = m_Starts[M_START_DIFFICULTY];
    gm->run.stage = m_Starts[M_START_STAGE];
    gm->mode.in_practice = true;
    gm->player.lives = 2;
    gm->player.bombs = 3;
    TitleScene_Remove();
    Scene_Request(SCENE_GAMEPLAY);
    m_Lives = g_GameManager.player.lives;
    m_Setbacks = 0;
    return OK;
}

static bool M_ScriptInput(const int32_t frame, void *const input)
{
    return false;
}

// Counts each life lost as a setback.
static bool M_Tick(const void *const input)
{
    const bool running = Game_Tick(input);
    const int32_t lives = g_GameManager.player.lives;
    m_Setbacks += lives < m_Lives ? m_Lives - lives : 0;
    m_Lives = lives;
    return running;
}

static int32_t M_Setbacks(void)
{
    return m_Setbacks;
}

static RECORDER_SCORE M_Score(void)
{
    return (RECORDER_SCORE) {
        .values = { g_GameManager.run.stage_frames, g_GameManager.score.value },
        .count = 2,
    };
}

// Ends the run at the result scene, which practice reaches both after a
// clear and after the last life.
static bool M_HasEnded(void)
{
    return Scene_Get() == SCENE_GAME_RESULT;
}

static RESULT M_Save(
    const char *const path, const void *const inputs, const int32_t frames)
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
        m_Started = false;
    }
}

int main(int argc, char **argv)
{
    const RECORDER_GAME game = {
        .program = "th06record",
        .input_schema = &g_InputSchema,
        .bot_ini = "replay/th06/bot.ini",
        .max_frames = 216000,
        .set_start = M_SetStart,
        .start = M_Start,
        .script_input = M_ScriptInput,
        .tick = M_Tick,
        .setbacks = M_Setbacks,
        .score = M_Score,
        .has_ended = M_HasEnded,
        .save = M_Save,
        .stop = M_Stop,
    };
    return Recorder_Main(&game, argc, argv);
}
