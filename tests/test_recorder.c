#include "recorder.h"

#include "test.h"
#include "test_dir.h"

#include "core/base/utils.h"
#include "core/vfs.h"
#include "shell/game/files.h"

#include <stddef.h>
#include <stdlib.h>

#define M_TRAP 45

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

// Holds a toy game: the player walks between two walls. It loses a life
// on every 25th frame that it stands on a multiple of 3, or, with a trap,
// only at frame 280 when it stood at the trap or right of it at frame 153,
// and the game then ends at frame 400.
static int32_t m_Trap;
static int32_t m_StartLives;
static int32_t m_Frame;
static int32_t m_X;
static int32_t m_Lives;
static int32_t m_TrapX;
static int32_t m_MostTrapX;
static M_INPUT *m_Saved;
static int32_t m_SavedFrames;
// Counts the sources that the toy game mounts at each start and that are
// still open.
static int32_t m_OpenSources;
static int32_t m_MostOpenSources;

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    return false;
}

static RESULT M_Open(
    VFS_SOURCE *const source, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    return FAIL("the toy source has no files");
}

static void M_List(
    const VFS_SOURCE *const source, const char *const dir,
    void (*const visit)(void *user, const char *name), void *const user)
{
}

static void M_Close(VFS_SOURCE *const source)
{
    m_OpenSources--;
}

static const VFS_SOURCE_FUNCS m_SourceFuncs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

static VFS_SOURCE m_Source = { .funcs = &m_SourceFuncs, .name = "toy" };

static RESULT M_SetStart(const char *const name, const char *const value)
{
    FAIL_IF(strcmp(name, "lives") != 0, "there is no start value %s", name);
    m_StartLives = atoi(value);
    return OK;
}

static void M_Reset(void)
{
    m_Frame = 0;
    m_X = 0;
    m_Lives = m_StartLives;
    m_TrapX = 0;
}

static RESULT M_Start(void)
{
    Vfs_MountSource(Shell_GetFiles(), "", &m_Source, 0);
    m_OpenSources++;
    m_MostOpenSources = MAX(m_MostOpenSources, m_OpenSources);
    M_Reset();
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
    if (m_Trap == 0) {
        m_Lives -= m_Frame % 25 == 0 && m_X % 3 == 0;
        return true;
    }
    if (m_Frame == 153) {
        m_TrapX = m_X;
        m_MostTrapX = MAX(m_MostTrapX, m_X);
    }
    m_Lives -= m_Frame == 280 && m_TrapX >= m_Trap;
    return true;
}

static int32_t M_ListThreats(VEC2 *const offsets, const int32_t max)
{
    offsets[0] = (VEC2) { (float)(-50 - m_X), 0.0f };
    offsets[1] = (VEC2) { (float)(50 - m_X), 0.0f };
    return 2;
}

static int32_t M_Setbacks(void)
{
    return m_StartLives - m_Lives;
}

static RECORDER_SCORE M_Score(void)
{
    return (RECORDER_SCORE) { .values = { m_Frame, m_X }, .count = 2 };
}

static bool M_HasEnded(void)
{
    return m_Trap == 0 ? m_Lives < 0 : m_Frame >= 400;
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
    .max_frames = 216000,
    .set_start = M_SetStart,
    .start = M_Start,
    .script_input = M_ScriptInput,
    .tick = M_Tick,
    .list_threats = M_ListThreats,
    .setbacks = M_Setbacks,
    .score = M_Score,
    .has_ended = M_HasEnded,
    .save = M_Save,
    .stop = M_Stop,
};

// Records a run of game with the extra arguments and returns its inputs
// and frames.
static M_INPUT *M_Record(
    const char *const name, const RECORDER_GAME *const game,
    const char *const *const extra, const int32_t extra_count,
    int32_t *const frames)
{
    char *const dir = TestDir_Prepare(name);
    char *const ini = TestDir_Join(dir, "bot.ini");
    const char text[] = "[roles]\nshoot = shoot\nleft = left\nright = right\n";
    TestDir_Write(ini, text, sizeof(text) - 1);
    RECORDER_GAME with_ini = *game;
    with_ini.bot_ini = ini;
    const char *args[16] = {
        "toyrecord", "--data", dir, "--out", "unused", "--start", "lives=2",
    };
    int32_t count = 7;
    for (int32_t i = 0; i < extra_count && count < (int32_t)ARRAY_SIZE(args);
         i++) {
        args[count++] = extra[i];
    }
    CHECK_EQ_INT(Recorder_Main(&with_ini, count, (char **)args), 0);
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
    M_Reset();
    for (int32_t i = 0; i < frames; i++) {
        M_Tick(&inputs[i]);
    }
    return m_Lives;
}

TEST(a_losing_run_starts_with_the_script_and_plays_to_the_end)
{
    m_Trap = 0;
    const char *const extra[] = { "--lose" };
    int32_t frames;
    M_INPUT *const inputs =
        M_Record("lose", &m_Game, extra, ARRAY_SIZE(extra), &frames);
    CHECK(inputs[0].shoot && !inputs[0].left && !inputs[0].right);
    CHECK_EQ_INT(M_Play(inputs, frames), -1);
    Mem_Free(inputs);
}

TEST(a_search_gives_the_same_run_each_time)
{
    m_Trap = 0;
    const char *const extra[] = { "--until", "300", "--seed", "5" };
    int32_t first_frames;
    M_INPUT *const first =
        M_Record("search_a", &m_Game, extra, ARRAY_SIZE(extra), &first_frames);
    int32_t second_frames;
    M_INPUT *const second =
        M_Record("search_b", &m_Game, extra, ARRAY_SIZE(extra), &second_frames);
    CHECK_EQ_INT(first_frames, 303);
    CHECK_EQ_INT(second_frames, first_frames);
    CHECK(memcmp(first, second, (size_t)first_frames * sizeof(M_INPUT)) == 0);
    CHECK_EQ_INT(M_Play(first, first_frames), 2);
    Mem_Free(first);
    Mem_Free(second);
}

TEST(each_start_gets_fresh_files)
{
    m_Trap = 0;
    m_MostOpenSources = 0;
    const char *const extra[] = { "--until", "300" };
    int32_t frames;
    M_INPUT *const inputs =
        M_Record("files", &m_Game, extra, ARRAY_SIZE(extra), &frames);
    CHECK_EQ_INT(m_MostOpenSources, 1);
    CHECK_EQ_INT(m_OpenSources, 0);
    Mem_Free(inputs);
}

TEST(a_search_stops_at_the_frame_cap)
{
    m_Trap = 0;
    RECORDER_GAME game = m_Game;
    game.max_frames = 200;
    int32_t frames;
    M_INPUT *const inputs = M_Record("cap", &game, nullptr, 0, &frames);
    CHECK_EQ_INT(frames, 200);
    CHECK_EQ_INT(M_Play(inputs, frames), 2);
    Mem_Free(inputs);
}

TEST(a_dead_end_falls_back_to_a_survivor_of_the_beam)
{
    m_Trap = M_TRAP;
    m_MostTrapX = -50;
    int32_t frames;
    M_INPUT *const inputs = M_Record("beam", &m_Game, nullptr, 0, &frames);
    CHECK_EQ_INT(frames, 400);
    CHECK_EQ_INT(M_Play(inputs, frames), 2);
    CHECK(m_TrapX < M_TRAP);
    CHECK(m_MostTrapX >= M_TRAP);
    Mem_Free(inputs);
}

TEST(scores_compare_value_by_value)
{
    const RECORDER_SCORE low = { .values = { 1, 9 }, .count = 2 };
    const RECORDER_SCORE high = { .values = { 2, 0 }, .count = 2 };
    const RECORDER_SCORE tie = { .values = { 2, 5 }, .count = 2 };
    CHECK(Recorder_CompareScores(high, low) > 0);
    CHECK(Recorder_CompareScores(low, high) < 0);
    CHECK(Recorder_CompareScores(tie, high) > 0);
    CHECK_EQ_INT(Recorder_CompareScores(high, high), 0);
}
