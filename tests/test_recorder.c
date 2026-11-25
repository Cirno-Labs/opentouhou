#include "recorder.h"

#include "test.h"
#include "test_dir.h"

#include "core/base/utils.h"

#include <stddef.h>
#include <stdlib.h>

typedef struct {
    bool shoot;
    bool left;
    bool right;
} M_INPUT;

#define M_BUTTON(field)                                                        \
    { .type = INPUT_ACTION_BUTTON,                                             \
      .name = #field,                                                          \
      .offset = offsetof(M_INPUT, field) }

static const INPUT_ACTION m_Actions[] = {
    M_BUTTON(shoot),
    M_BUTTON(left),
    M_BUTTON(right),
};

static const INPUT_SCHEMA m_Schema = {
    .actions = m_Actions,
    .count = ARRAY_SIZE(m_Actions),
    .size = sizeof(M_INPUT),
    .players = 1,
};

// Holds a toy game: the player walks between two walls and loses a life on
// every 25th frame that it stands on a multiple of 3.
static int32_t m_StartLives;
static int32_t m_Frame;
static int32_t m_X;
static int32_t m_Lives;
static M_INPUT *m_Saved;
static int32_t m_SavedFrames;

static RESULT M_SetStart(const char *const name, const char *const value)
{
    FAIL_IF(strcmp(name, "lives") != 0, "there is no start value %s", name);
    m_StartLives = atoi(value);
    return OK;
}

static RESULT M_Start(void)
{
    m_Frame = 0;
    m_X = 0;
    m_Lives = m_StartLives;
    return OK;
}

static bool M_ScriptInput(const int32_t frame, void *const input)
{
    if (frame >= 3) {
        return false;
    }
    *(M_INPUT *)input = (M_INPUT) { .shoot = true };
    return true;
}

static bool M_Tick(const void *const input)
{
    const M_INPUT *const held = input;
    m_X = MAX(-50, MIN(50, m_X + held->right - held->left));
    m_Frame++;
    if (m_Frame % 25 == 0 && m_X % 3 == 0) {
        m_Lives--;
    }
    return true;
}

static int32_t M_ListThreats(VEC2 *const offsets, const int32_t max)
{
    offsets[0] = (VEC2) { (float)(-50 - m_X), 0.0f };
    offsets[1] = (VEC2) { (float)(50 - m_X), 0.0f };
    return 2;
}

static RECORDER_STATE M_State(void)
{
    return (RECORDER_STATE) { .lives = m_Lives, .progress = m_Frame };
}

static bool M_HasEnded(void)
{
    return m_Lives < 0;
}

static RESULT M_Save(
    const char *const path, const void *const inputs, const int32_t frames)
{
    Mem_Free(m_Saved);
    m_Saved = Mem_Dup(inputs, (size_t)frames * sizeof(M_INPUT));
    m_SavedFrames = frames;
    return OK;
}

static void M_Stop(void)
{
}

static const RECORDER_GAME m_Game = {
    .program = "toyrecord",
    .input_schema = &m_Schema,
    .bot_ini = "",
    .set_start = M_SetStart,
    .start = M_Start,
    .script_input = M_ScriptInput,
    .tick = M_Tick,
    .list_threats = M_ListThreats,
    .state = M_State,
    .has_ended = M_HasEnded,
    .save = M_Save,
    .stop = M_Stop,
};

// Records a run with the extra arguments and returns its inputs and frames.
static M_INPUT *M_Record(
    const char *const name, const char *const *const extra,
    const int32_t extra_count, int32_t *const frames)
{
    char *const dir = TestDir_Prepare(name);
    char *const ini = TestDir_Join(dir, "bot.ini");
    const char text[] = "[roles]\nshoot = shoot\nleft = left\nright = right\n";
    TestDir_Write(ini, text, sizeof(text) - 1);
    const char *args[16] = {
        "toyrecord", "--data",  dir,       "--out",      "unused", "--bot",
        ini,         "--start", "lives=2", "--strategy", "wander",
    };
    int32_t count = 11;
    for (int32_t i = 0; i < extra_count && count < (int32_t)ARRAY_SIZE(args);
         i++) {
        args[count++] = extra[i];
    }
    CHECK_EQ_INT(Recorder_Main(&m_Game, count, (char **)args), 0);
    Mem_Free(ini);
    Mem_Free(dir);
    M_INPUT *const saved = m_Saved;
    *frames = m_SavedFrames;
    m_Saved = nullptr;
    return saved;
}

// Plays inputs on the toy game and returns the lives that it ends with.
static int32_t M_Play(const M_INPUT *const inputs, const int32_t frames)
{
    Result_Ignore(M_Start());
    for (int32_t i = 0; i < frames; i++) {
        M_Tick(&inputs[i]);
    }
    return m_Lives;
}

TEST(a_straight_run_starts_with_the_script_and_stops_at_its_progress)
{
    const char *const extra[] = { "--until-progress", "100" };
    int32_t frames;
    M_INPUT *const inputs =
        M_Record("straight", extra, ARRAY_SIZE(extra), &frames);
    CHECK_EQ_INT(frames, 100);
    CHECK(inputs[0].shoot && !inputs[0].left && !inputs[0].right);
    Mem_Free(inputs);
}

TEST(a_search_without_misses_gives_the_same_run_each_time)
{
    const char *const extra[] = {
        "--segment", "20",     "--no-miss", "--until-progress",
        "300",       "--seed", "5",
    };
    int32_t first_frames;
    M_INPUT *const first =
        M_Record("search_a", extra, ARRAY_SIZE(extra), &first_frames);
    int32_t second_frames;
    M_INPUT *const second =
        M_Record("search_b", extra, ARRAY_SIZE(extra), &second_frames);
    CHECK_EQ_INT(first_frames, 303);
    CHECK_EQ_INT(second_frames, first_frames);
    CHECK(memcmp(first, second, (size_t)first_frames * sizeof(M_INPUT)) == 0);
    CHECK_EQ_INT(M_Play(first, first_frames), 2);
    Mem_Free(first);
    Mem_Free(second);
}
