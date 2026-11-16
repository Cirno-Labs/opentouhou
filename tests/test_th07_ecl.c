#include "th07/ecl.h"
#include "th07/ecl/private.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/bullet.h"
#include "th07/effect.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/player.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/screens/result/records.h"
#include "th07/stubs.h"

#include "test.h"
#include "th07_data.h"
#include "th07_ecl_builder.h"
#include "th07_play_setup.h"

#include "core/base/color.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_ALL 0xff
#define M_SEED 7
#define M_PATH "data/ecldata1.ecl"

// Appends an instruction for all difficulties with the argument words
// that follow mask.
#define M_INSTR(sub_, time_, opcode_, mask_, ...)                              \
    EclBuilder_Instr(                                                          \
        &m_Builder, sub_, time_, opcode_, M_ALL, mask_,                        \
        (const uint32_t[]) { __VA_ARGS__ },                                    \
        (int32_t)(sizeof((const uint32_t[]) { __VA_ARGS__ }) / 4))
#define M_NOARGS(sub_, time_, opcode_)                                         \
    EclBuilder_Instr(&m_Builder, sub_, time_, opcode_, M_ALL, 0, nullptr, 0)
#define M_F(x_) EclBuilder_Float(x_)
#define M_NONE ((uint32_t)-1)
#define M_MAX_CALLS 16
#define M_PI_F 3.1415927f
#define M_HALF_PI 1.5707964f
#define M_QUARTER_PI 0.7853982f
#define M_THREE_QUARTER_PI 2.3561945f

// The test links these instead of the modules they stand for. The
// items and the spell card gui record their calls.

typedef struct {
    VEC3 pos;
    ITEM_TYPE type;
    ITEM_STATE state;
} M_ITEM_CALL;

typedef struct {
    int32_t names;
    int32_t sprite;
    char name[64];
    int32_t ends;
    int32_t bonuses;
    int32_t bonus;
    int32_t attempts;
    int32_t attempt_index;
    char attempt_name[64];
    int32_t captures;
    int32_t capture_index;
    int32_t capture_bonus;
} M_SPELL_CALLS;

static const char *m_Directory;
static ECL_BUILDER m_Builder;
static uint8_t m_File[ECL_BUILDER_MAX_SIZE];
static ENEMY m_Enemy;
static ENEMY m_Other;
static M_ITEM_CALL m_Items[M_MAX_CALLS];
static int32_t m_ItemCount;
static M_SPELL_CALLS m_Spell;

float Player_AngleFrom(const VEC3 pos)
{
    return 0.5f;
}

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
    if (m_ItemCount < M_MAX_CALLS) {
        m_Items[m_ItemCount] = (M_ITEM_CALL) { pos, type, state };
    }
    m_ItemCount++;
}

void Gui_ShowSpellCardName(const int32_t sprite, const char *const name)
{
    m_Spell.names++;
    m_Spell.sprite = sprite;
    SDL_strlcpy(m_Spell.name, name, sizeof(m_Spell.name));
}

void Gui_EndSpellCardName(void)
{
    m_Spell.ends++;
}

void Gui_ShowSpellCardBonus(const int32_t bonus)
{
    m_Spell.bonuses++;
    m_Spell.bonus = bonus;
}

void ResultScreen_AddSpellCardAttempt(
    const int32_t index, const char *const name)
{
    m_Spell.attempts++;
    m_Spell.attempt_index = index;
    SDL_strlcpy(m_Spell.attempt_name, name, sizeof(m_Spell.attempt_name));
}

void ResultScreen_AddSpellCardCapture(const int32_t index, const int32_t bonus)
{
    m_Spell.captures++;
    m_Spell.capture_index = index;
    m_Spell.capture_bonus = bonus;
}

void ScreenEffect_Shake(
    const int32_t duration, const int32_t from, const int32_t to)
{
}

void ScreenEffect_Flash(
    const int32_t duration, const int32_t count, const uint32_t color)
{
}

static void M_SetUp(const char *const case_name)
{
    m_Directory = PlaySetup_SetUp(case_name, M_SEED);
    m_ItemCount = 0;
    memset(&m_Spell, 0, sizeof(m_Spell));
    EclBuilder_Init(&m_Builder);
}

static void M_TearDown(void)
{
    Ecl_Release();
    PlaySetup_TearDown();
}

// Writes size bytes of m_File as the stage's script file and loads it.
static bool M_LoadBytes(const size_t size)
{
    Th07Data_WriteFile(m_Directory, M_PATH, m_File, size);
    Files_Shutdown();
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    return Result_Log(Ecl_Load(), LOG_LEVEL_INFO);
}

static size_t M_Build(void)
{
    return EclBuilder_Build(&m_Builder, m_File);
}

static bool M_Loads(void)
{
    return M_LoadBytes(M_Build());
}

// Sets enemy up as Enemy_Spawn would leave it, at frame 0 of sub.
static void M_InitEnemy(ENEMY *const enemy, const int16_t sub)
{
    memset(enemy, 0, sizeof(*enemy));
    enemy->life = 1;
    enemy->script.death_sub = -1;
    enemy->script.pending_interrupt = -1;
    enemy->script.periodic.sub = -1;
    enemy->script.state.frame_func = ECL_FUNC_NONE;
    enemy->turns.left = -1;
    Ecl_StartSub(&enemy->script.state, sub);
}

static void M_Start(void)
{
    CHECK(M_Loads());
    M_InitEnemy(&m_Enemy, 0);
}

static ECL_RUN M_Frame(void)
{
    return Ecl_RunEnemy(&m_Enemy);
}

static void M_Frames(const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        CHECK(M_Frame() == ECL_RUN_CONTINUE);
    }
}

static int32_t *M_Ints(void)
{
    return m_Enemy.script.state.vars.ints;
}

static float *M_Floats(void)
{
    return m_Enemy.script.state.vars.floats;
}

// Returns the shared int variable index, read through opcode 4 in place
// of instruction 0 of subroutine 0, with instruction 1 held back.
static int32_t M_Shared(const int32_t index)
{
    static ENEMY reader;
    M_InitEnemy(&reader, 0);
    ECL_INSTR *const instr = Ecl_GetInstr(0, 0);
    ECL_INSTR *const next = Ecl_GetInstr(0, 1);
    const ECL_INSTR saved = *instr;
    const int32_t time = next->time;
    *instr = (ECL_INSTR) {
        .opcode = 4,
        .difficulty_mask = M_ALL,
        .param_mask = 3,
        .args = { { .i = 10000 }, { .i = 10037 + index } },
    };
    next->time = 999;
    Ecl_RunEnemy(&reader);
    *instr = saved;
    next->time = time;
    return reader.script.state.vars.ints[0];
}

TEST(load_reads_subroutines_and_timelines)
{
    M_SetUp("load_reads");
    const int32_t sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(sub, 3, 4, 1, 10000, 9);
    const int32_t first = EclBuilder_BeginTimeline(&m_Builder);
    EclBuilder_Entry(
        &m_Builder, first, 10, 0, ECL_TIMELINE_SPAWN, 32,
        (const uint32_t[]) { M_F(1.0f), M_F(2.0f), M_F(3.0f), 4, 5, 6 });
    EclBuilder_Entry(
        &m_Builder, first, 20, 0, ECL_TIMELINE_INTERRUPT_BOSS, 32,
        (const uint32_t[]) { 2, 3, 0, 0, 0, 0 });
    const int32_t second = EclBuilder_BeginTimeline(&m_Builder);
    EclBuilder_Entry(
        &m_Builder, second, 5, 0, ECL_TIMELINE_SPAWN, 12,
        (const uint32_t[]) { M_F(7.0f), M_F(8.0f), 0, 9, 9, 9 });
    CHECK(M_Loads());
    CHECK_EQ_INT(Ecl_GetTimelineCount(), 2);
    const ECL_INSTR *const instr = Ecl_GetInstr(0, 0);
    CHECK_EQ_INT(instr->time, 3);
    CHECK_EQ_INT(instr->opcode, 4);
    CHECK_EQ_INT(instr->param_mask, 1);
    CHECK_EQ_INT(instr->args[1].i, 9);
    CHECK_EQ_INT(Ecl_GetInstr(0, 1)->time, -1);
    const ECL_TIMELINE_ENTRY *const a = Ecl_GetTimeline(0);
    CHECK_EQ_INT(a[0].time, 10);
    CHECK(a[0].args.spawn.pos.y == 2.0f);
    CHECK_EQ_INT(a[0].args.spawn.life, 4);
    CHECK_EQ_INT(a[0].args.spawn.item, 5);
    CHECK_EQ_INT(a[0].args.spawn.score, 6);
    CHECK_EQ_INT(a[1].opcode, ECL_TIMELINE_INTERRUPT_BOSS);
    CHECK_EQ_INT(a[1].args.interrupt.boss, 2);
    CHECK_EQ_INT(a[1].args.interrupt.interrupt, 3);
    CHECK(a[2].time < 0);
    const ECL_TIMELINE_ENTRY *const b = Ecl_GetTimeline(1);
    CHECK(b[0].args.spawn.pos.x == 7.0f);
    CHECK(b[0].args.spawn.pos.y == 0.0f);
    CHECK_EQ_INT(b[0].args.spawn.life, 0);
    CHECK(b[1].time < 0);
    M_TearDown();
}

TEST(load_accepts_a_terminator_with_only_its_time)
{
    M_SetUp("load_terminator");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginTimeline(&m_Builder);
    const size_t size = M_Build();
    CHECK(M_LoadBytes(size - 2));
    CHECK(Ecl_GetTimeline(0)[0].time < 0);
    CHECK(!M_LoadBytes(size - 3));
    M_TearDown();
}

TEST(load_rejects_a_bad_header)
{
    M_SetUp("load_header");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginTimeline(&m_Builder);
    M_Build();
    CHECK(!M_LoadBytes(0x40));
    const size_t size = M_Build();
    m_File[2] = 17;
    CHECK(!M_LoadBytes(size));
    M_Build();
    m_File[0] = 100;
    CHECK(!M_LoadBytes(size));
    M_Build();
    m_File[ECL_BUILDER_SUB_TABLE] = 0xf0;
    m_File[ECL_BUILDER_SUB_TABLE + 1] = 0xff;
    CHECK(!M_LoadBytes(size));
    M_Build();
    m_File[4] = 0xf0;
    m_File[5] = 0xff;
    CHECK(!M_LoadBytes(size));
    M_TearDown();
}

TEST(load_rejects_an_instruction_past_the_file_or_too_short)
{
    M_SetUp("load_instr");
    const int32_t sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(sub, 0, 12, 0, 10000, 1, 2);
    const size_t size = M_Build();
    CHECK(M_LoadBytes(size));
    const size_t at = ECL_BUILDER_SUB_TABLE + 4;
    m_File[at + 6] = 0xff;
    m_File[at + 7] = 0x7f;
    CHECK(!M_LoadBytes(size));
    M_Build();
    m_File[at + 6] = 8;
    m_File[at + 7] = 0;
    CHECK(!M_LoadBytes(size));
    CHECK(!M_LoadBytes(at + 2));
    EclBuilder_Init(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 12, 0, 10000, 1);
    CHECK(!M_Loads());
    M_TearDown();
}

TEST(load_rejects_a_jump_between_instructions)
{
    M_SetUp("load_jump");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 17, 1, 10000);
    const int32_t jump = M_INSTR(0, 1, 2, 0, 0, (uint32_t)-16);
    CHECK(M_Loads());
    CHECK_EQ_INT(Ecl_GetInstr(0, 1)->target, 0);
    EclBuilder_SetArg(&m_Builder, 0, jump, 1, (uint32_t)-6);
    CHECK(!M_Loads());
    EclBuilder_SetArg(&m_Builder, 0, jump, 1, 20);
    CHECK(M_Loads());
    CHECK_EQ_INT(Ecl_GetInstr(0, 1)->target, 2);
    EclBuilder_Init(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    const int32_t branch = M_INSTR(0, 0, 28, 0, 0, 0, 0, 0);
    CHECK(M_Loads());
    EclBuilder_SetArg(&m_Builder, 0, branch, 3, 4);
    CHECK(!M_Loads());
    M_TearDown();
}

// Returns whether a file whose subroutine 0 holds the instruction opcode
// with mask and the words, and whose subroutine 1 is empty, loads.
static bool M_LoadsInstr(
    const int16_t opcode, const uint16_t mask, const uint32_t *const words,
    const int32_t count)
{
    EclBuilder_Init(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_Instr(&m_Builder, 0, 0, opcode, M_ALL, mask, words, count);
    return M_Loads();
}

#define M_LOADS(opcode_, mask_, ...)                                           \
    M_LoadsInstr(                                                              \
        opcode_, mask_, (const uint32_t[]) { __VA_ARGS__ },                    \
        (int32_t)(sizeof((const uint32_t[]) { __VA_ARGS__ }) / 4))

TEST(load_rejects_a_subroutine_the_file_does_not_have)
{
    M_SetUp("load_subs");
    CHECK(M_LOADS(41, 0, 1));
    CHECK(!M_LOADS(41, 1, 2));
    CHECK(!M_LOADS(92, 0, 2, 0, 0, 0, 0, 0, 0));
    CHECK(!M_LOADS(93, 0, M_NONE, 0, 0, 0, 0, 0, 0));
    CHECK(M_LOADS(108, 0, M_NONE, 0));
    CHECK(!M_LOADS(108, 0, 2, 0));
    CHECK(M_LOADS(108, 1, 10000, 0));
    CHECK(!M_LOADS(113, 0, 5));
    CHECK(!M_LOADS(115, 0, 5));
    CHECK(M_LOADS(144, 0, 30, M_NONE));
    CHECK(!M_LOADS(144, 0, 30, 2));
    CHECK(M_LOADS(148, 0, 0, 10, 1));
    CHECK(!M_LOADS(148, 0, 0, 10, 2));
    EclBuilder_Init(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    const int32_t timeline = EclBuilder_BeginTimeline(&m_Builder);
    const uint32_t args[6] = {};
    EclBuilder_Entry(
        &m_Builder, timeline, 0, 9, ECL_TIMELINE_START_MSG, 32, args);
    CHECK(M_Loads());
    EclBuilder_Entry(
        &m_Builder, timeline, 0, 9, ECL_TIMELINE_SPAWN_RANDOM, 32, args);
    CHECK(!M_Loads());
    M_TearDown();
}

TEST(load_rejects_a_pattern_function_or_curve_it_does_not_have)
{
    M_SetUp("load_funcs");
    CHECK(M_LOADS(121, 0, 23, 0));
    CHECK(!M_LOADS(121, 0, 24, 0));
    CHECK(!M_LOADS(121, 0, M_NONE, 0));
    CHECK(M_LOADS(121, 1, 10000, 0));
    CHECK(M_LOADS(122, 0, M_NONE, 0));
    CHECK(!M_LOADS(122, 0, 24, 0));
    CHECK(M_LOADS(27, 0, M_F(10004.0f), 10, 7, 0, 0, 0, 0, 0));
    CHECK(!M_LOADS(27, 0, M_F(10004.0f), 10, 8, 0, 0, 0, 0, 0));
    CHECK(M_LOADS(27, 4, M_F(10004.0f), 10, 10000, 0, 0, 0, 0, 0));
    M_TearDown();
}

TEST(load_reads_spell_card_names)
{
    M_SetUp("load_names");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_SpellCard(&m_Builder, 0, 0, 3, 140, "\x83\x65st");
    CHECK(M_Loads());
    CHECK_EQ_STR(Ecl_GetInstr(0, 0)->text, "\xe3\x83\x86st");
    CHECK_EQ_INT((uint32_t)Ecl_GetInstr(0, 0)->args[0].i >> 16, 140);
    EclBuilder_Init(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_SpellCard(&m_Builder, 0, 0, 3, 141, "x");
    CHECK(!M_Loads());
    EclBuilder_Init(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_SpellCard(&m_Builder, 0, 0, 3, 0, "\x81");
    CHECK(!M_Loads());
    char name[ECL_BUILDER_NAME_SIZE + 1];
    memset(name, 'a', ECL_BUILDER_NAME_SIZE);
    name[ECL_BUILDER_NAME_SIZE] = '\0';
    EclBuilder_Init(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_SpellCard(&m_Builder, 0, 0, 3, 0, name);
    CHECK(!M_Loads());
    M_TearDown();
}

TEST(load_rejects_a_timeline_past_the_file)
{
    M_SetUp("load_timeline");
    EclBuilder_BeginSub(&m_Builder);
    const int32_t timeline = EclBuilder_BeginTimeline(&m_Builder);
    const uint32_t args[6] = {};
    EclBuilder_Entry(&m_Builder, timeline, 0, 0, ECL_TIMELINE_SPAWN, 32, args);
    const size_t size = M_Build();
    CHECK(M_LoadBytes(size));
    CHECK(!M_LoadBytes(size - 8));
    m_File[size - 4 - 32 + 6] = 7;
    CHECK(!M_LoadBytes(size));
    M_TearDown();
}

TEST(run_steps_the_instructions_due_on_each_frame)
{
    M_SetUp("run_frames");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10000, 1);
    M_INSTR(0, 2, 4, 1, 10001, 2);
    EclBuilder_Instr(
        &m_Builder, 0, 2, 4, 1 << 2, 1, (const uint32_t[]) { 10002, 3 }, 2);
    M_NOARGS(0, 3, 1);
    M_Start();
    M_Frames(2);
    CHECK_EQ_INT(M_Ints()[0], 1);
    CHECK_EQ_INT(M_Ints()[1], 0);
    M_Frames(1);
    CHECK_EQ_INT(M_Ints()[1], 2);
    CHECK_EQ_INT(M_Ints()[2], 0);
    CHECK(M_Frame() == ECL_RUN_DELETE);
    M_TearDown();
}

TEST(run_loop_counts_down_in_the_shared_instruction)
{
    M_SetUp("run_loop");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 17, 1, 10000);
    M_INSTR(0, 1, 3, 0, 0, (uint32_t)-16, 3);
    M_Start();
    M_Frames(6);
    CHECK_EQ_INT(M_Ints()[0], 3);
    CHECK_EQ_INT(Ecl_GetInstr(0, 1)->args[2].i, 0);
    CHECK_EQ_INT(m_Enemy.script.state.pc, 2);
    M_InitEnemy(&m_Other, 0);
    for (int32_t i = 0; i < 4; i++) {
        Ecl_RunEnemy(&m_Other);
    }
    CHECK_EQ_INT(m_Other.script.state.vars.ints[0], 1);
    M_TearDown();
}

TEST(run_jumps_on_int_and_float_comparisons)
{
    M_SetUp("run_compare");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 5, 1, M_F(10004.0f), M_F(1.5f));
    M_INSTR(0, 0, 33, 1, M_F(10004.0f), M_F(2.0f), 5, 48);
    M_INSTR(0, 0, 4, 1, 10000, 1);
    M_INSTR(0, 5, 28, 1, 10000, 0, 7, 48);
    M_INSTR(0, 5, 4, 1, 10001, 1);
    M_INSTR(0, 7, 4, 1, 10002, 1);
    M_Start();
    M_Frames(1);
    CHECK_EQ_INT(M_Ints()[0], 0);
    CHECK_EQ_INT(M_Ints()[1], 0);
    CHECK_EQ_INT(M_Ints()[2], 1);
    CHECK_EQ_INT(m_Enemy.script.state.timer.current, 8);
    M_TearDown();
}

TEST(run_wait_holds_the_instructions_and_the_timer)
{
    M_SetUp("run_wait");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 45, 0, 3);
    M_INSTR(0, 1, 4, 1, 10000, 1);
    M_Start();
    M_Frames(4);
    CHECK_EQ_INT(M_Ints()[0], 0);
    CHECK_EQ_INT(m_Enemy.script.state.timer.current, 1);
    M_Frames(1);
    CHECK_EQ_INT(M_Ints()[0], 1);
    M_TearDown();
}

TEST(run_call_passes_the_shared_variables_and_returns)
{
    M_SetUp("run_call");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10037, 7);
    M_INSTR(0, 0, 5, 1, M_F(10041.0f), M_F(2.5f));
    M_INSTR(0, 0, 4, 1, 10000, 3);
    M_INSTR(0, 0, 41, 0, 1);
    M_INSTR(0, 0, 4, 3, 10001, 10038);
    M_INSTR(1, 0, 4, 3, 10038, 10029);
    M_INSTR(1, 0, 5, 3, M_F(10004.0f), M_F(10033.0f));
    M_INSTR(1, 0, 5, 3, M_F(10042.0f), M_F(10004.0f));
    M_NOARGS(1, 0, 42);
    M_Start();
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.script.depth, 0);
    CHECK_EQ_INT(m_Enemy.script.state.sub, 0);
    CHECK_EQ_INT(M_Ints()[0], 3);
    CHECK_EQ_INT(M_Ints()[1], 7);
    CHECK_EQ_INT(M_Shared(5), 2);
    M_TearDown();
}

TEST(run_call_depth_stops_at_15)
{
    M_SetUp("run_depth");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 17, 1, 10000);
    M_INSTR(0, 1, 41, 0, 0);
    M_Start();
    M_Frames(40);
    CHECK_EQ_INT(m_Enemy.script.depth, 15);
    CHECK_EQ_INT(m_Enemy.script.stack[15].sub, 0);
    CHECK_EQ_INT(m_Enemy.script.stack[14].vars.ints[0], 15);
    M_TearDown();
}

TEST(run_call_without_a_stack_keeps_the_depth)
{
    M_SetUp("run_no_stack");
    for (int32_t i = 0; i < 3; i++) {
        EclBuilder_BeginSub(&m_Builder);
    }
    M_INSTR(0, 0, 41, 0, 1);
    M_INSTR(0, 0, 41, 0, 2);
    M_INSTR(1, 0, 130, 0, 1);
    M_NOARGS(1, 0, 42);
    M_INSTR(2, 0, 4, 1, 10000, 9);
    M_NOARGS(2, 5, 1);
    M_Start();
    M_Frames(1);
    CHECK(m_Enemy.flags.no_call_stack);
    CHECK_EQ_INT(m_Enemy.script.depth, 0);
    CHECK_EQ_INT(m_Enemy.script.state.sub, 2);
    CHECK_EQ_INT(M_Ints()[0], 9);
    CHECK_EQ_INT(m_Enemy.script.stack[0].sub, 0);
    CHECK_EQ_INT(m_Enemy.script.stack[0].pc, 1);
    M_TearDown();
}

TEST(run_interrupt_now_resumes_after_it)
{
    M_SetUp("run_interrupt");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 108, 0, 1, 4);
    M_INSTR(0, 0, 109, 0, 4);
    M_INSTR(0, 0, 4, 1, 10037, 1);
    M_INSTR(1, 0, 4, 1, 10038, 1);
    M_NOARGS(1, 0, 42);
    M_Start();
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.script.interrupts[4], 1);
    CHECK_EQ_INT(m_Enemy.script.pending_interrupt, -1);
    CHECK_EQ_INT(M_Shared(0), 1);
    CHECK_EQ_INT(M_Shared(1), 1);
    CHECK_EQ_INT(m_Enemy.script.depth, 0);
    M_TearDown();
}

TEST(run_pending_interrupt_skips_the_next_instruction)
{
    M_SetUp("run_pending");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 108, 0, 1, 0);
    M_INSTR(0, 0, 4, 1, 10037, 0);
    M_INSTR(0, 0, 4, 1, 10038, 0);
    M_INSTR(0, 2, 4, 1, 10037, 5);
    M_INSTR(0, 2, 4, 1, 10038, 5);
    M_INSTR(0, 2, 145, 0, 1, 3);
    M_NOARGS(1, 0, 42);
    M_Start();
    M_Frames(2);
    m_Enemy.script.pending_interrupt = 0;
    g_Enemies.bosses[1] = &m_Other;
    m_Other.script.pending_interrupt = -1;
    M_Frames(1);
    CHECK_EQ_INT(M_Shared(0), 0);
    CHECK_EQ_INT(M_Shared(1), 5);
    CHECK_EQ_INT(m_Other.script.pending_interrupt, 3);
    M_TearDown();
}

TEST(run_periodic_call_ticks_again_after_each_restart)
{
    M_SetUp("run_periodic");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10037, 0);
    M_INSTR(0, 0, 4, 1, 10000, 4);
    M_INSTR(0, 0, 144, 0, 3, 1);
    M_NOARGS(0, 100, 1);
    M_INSTR(1, 0, 17, 1, 10000);
    M_INSTR(1, 0, 4, 3, 10037, 10000);
    M_NOARGS(1, 0, 42);
    M_Start();
    M_Frames(3);
    CHECK_EQ_INT(M_Shared(0), 0);
    M_Frames(1);
    CHECK_EQ_INT(M_Shared(0), 5);
    CHECK_EQ_INT(m_Enemy.script.periodic.vars.ints[0], 5);
    CHECK_EQ_INT(M_Ints()[0], 4);
    CHECK(!m_Enemy.script.state.in_timed_call);
    M_Frames(2);
    CHECK_EQ_INT(M_Shared(0), 6);
    M_TearDown();
}

TEST(run_timer_add_jumps_ahead)
{
    M_SetUp("run_timer_add");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 123, 0, 5);
    M_INSTR(0, 6, 4, 1, 10000, 1);
    M_Start();
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.script.state.timer.current, 6);
    M_Frames(1);
    CHECK_EQ_INT(M_Ints()[0], 1);
    M_TearDown();
}

TEST(operands_name_different_variables_in_each_table)
{
    M_SetUp("operands_tables");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10018, 5);
    M_INSTR(0, 0, 5, 1, M_F(10018.0f), M_F(3.5f));
    M_INSTR(0, 0, 5, 1, M_F(10005.0f), M_F(2.75f));
    M_INSTR(0, 0, 4, 3, 10000, 10005);
    M_INSTR(0, 0, 4, 3, 10001, 10057);
    M_INSTR(0, 0, 5, 3, M_F(10006.0f), M_F(10000.0f));
    M_INSTR(0, 0, 5, 3, M_F(10007.0f), M_F(10057.0f));
    M_INSTR(0, 0, 4, 3, 10070, 300);
    M_Start();
    m_Enemy.move.delta.x = 1.25f;
    M_Frames(1);
    CHECK_EQ_INT(Ecl_GetInstr(0, 0)->args[0].i, 5);
    CHECK(m_Enemy.move.pos.x == 3.5f);
    CHECK_EQ_INT(M_Ints()[0], 2);
    CHECK_EQ_INT(M_Ints()[1], 10057);
    CHECK(M_Floats()[2] == 2.0f);
    CHECK(M_Floats()[3] == 1.25f);
    CHECK_EQ_INT(Ecl_GetInstr(0, 7)->args[0].i, 10070);
    CHECK_EQ_INT(m_Enemy.death.item, 300);
    M_TearDown();
}

TEST(operands_draw_random_values)
{
    M_SetUp("operands_random");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 3, 10000, 10055);
    M_INSTR(0, 0, 5, 3, M_F(10004.0f), M_F(10060.0f));
    M_INSTR(0, 0, 4, 1, 10029, 10);
    M_INSTR(0, 0, 4, 1, 10030, 100);
    M_INSTR(0, 0, 4, 3, 10001, 10056);
    M_Start();
    RNG expected = g_Rng;
    M_Frames(1);
    CHECK_EQ_INT(M_Ints()[0], (int32_t)Rng_U32(&expected));
    CHECK(M_Floats()[0] == Rng_F32(&expected) * 6.2831855f - 3.1415927f);
    CHECK_EQ_INT(M_Ints()[1], (int32_t)Rng_U32InRange(&expected, 10) + 100);
    M_TearDown();
}

TEST(operands_shared_variables_last_across_loads)
{
    M_SetUp("operands_shared");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10040, 77);
    M_Start();
    M_Frames(1);
    CHECK(M_Loads());
    CHECK_EQ_INT(M_Shared(3), 77);
    M_TearDown();
}

static uint32_t M_Pack(const int32_t low, const int32_t high)
{
    return (uint32_t)(uint16_t)low | (uint32_t)high << 16;
}

static int32_t M_BulletCount(void)
{
    int32_t count = 0;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        if (g_Bullets.bullets[i].state != BULLET_FREE) {
            count++;
        }
    }
    return count;
}

// Appends opcode 64 + aim with type 1, color 3, 3 columns, 2 rows,
// speeds 2 and 1 and angles 0.25 and 0.125.
static void M_Fire(const int32_t sub, const int32_t time, const int32_t aim)
{
    M_INSTR(
        sub, time, (int16_t)(64 + aim), 0, M_Pack(1, 3), 3, 2, M_F(2.0f),
        M_F(1.0f), M_F(0.25f), M_F(0.125f), 0);
}

TEST(fire_grows_the_shot_with_the_rank)
{
    M_SetUp("fire_rank");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 78, 0, M_F(1.0f), M_F(2.0f), M_F(0.0f));
    M_INSTR(0, 0, 131, 0, M_F(0.0f), M_F(2.0f), 0, 4, -2, 2);
    M_Fire(0, 0, 2);
    M_Start();
    g_GameManager.rank.value = 16;
    m_Enemy.move.pos = (VEC3) { 10.0f, 20.0f, 0.0f };
    M_Frames(1);
    const BULLET_SHOOTER *const shooter = &m_Enemy.shots.shooter;
    CHECK_EQ_INT(shooter->type, 1);
    CHECK_EQ_INT(shooter->color, 3);
    CHECK_EQ_INT(shooter->aim, BULLET_AIM_RING_AT_PLAYER);
    CHECK_EQ_INT(shooter->columns, 5);
    CHECK_EQ_INT(shooter->rows, 2);
    CHECK(shooter->speed1 == 3.0f);
    CHECK(shooter->speed2 == 1.5f);
    CHECK(shooter->angle1 == 0.25f);
    CHECK(shooter->angle2 == 0.125f);
    CHECK(shooter->pos.x == 11.0f && shooter->pos.y == 22.0f);
    CHECK(M_BulletCount() > 0);
    M_TearDown();
}

TEST(fire_keeps_the_shot_at_least_one_bullet_and_a_speed_of_03)
{
    M_SetUp("fire_clamp");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 131, 0, M_F(-5.0f), M_F(-5.0f), -5, -5, -5, -5);
    M_Fire(0, 0, 0);
    M_INSTR(
        0, 1, 64, 0, M_Pack(1, 3), 3, 2, M_F(0.0f), M_F(1.0f), M_F(0.0f),
        M_F(0.0f), 0);
    M_Start();
    M_Frames(1);
    const BULLET_SHOOTER *const shooter = &m_Enemy.shots.shooter;
    CHECK_EQ_INT(shooter->columns, 1);
    CHECK_EQ_INT(shooter->rows, 1);
    CHECK(shooter->speed1 == 0.3f);
    CHECK(shooter->speed2 == 0.3f);
    M_Frames(1);
    CHECK(shooter->speed1 == 0.0f);
    CHECK(shooter->speed2 == 0.3f);
    M_TearDown();
}

TEST(fire_during_a_spell_card_skips_the_rank)
{
    M_SetUp("fire_spell");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 131, 0, M_F(0.0f), M_F(2.0f), 0, 4, -2, 2);
    M_Fire(0, 0, 1);
    M_Start();
    g_GameManager.rank.value = 16;
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.shots.shooter.columns, 3);
    CHECK_EQ_INT(m_Enemy.shots.shooter.rows, 2);
    CHECK(m_Enemy.shots.shooter.speed1 == 2.0f);
    CHECK(m_Enemy.shots.shooter.speed2 == 1.0f);
    M_TearDown();
}

TEST(fire_reads_its_variable_operands_from_their_mask_bits)
{
    M_SetUp("fire_vars");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10000, 2);
    M_INSTR(0, 0, 4, 1, 10001, 4);
    M_INSTR(0, 0, 4, 1, 10002, 6);
    M_INSTR(0, 0, 5, 1, M_F(10004.0f), M_F(1.5f));
    M_INSTR(
        0, 0, 65, 0xff, M_Pack(10000, 10001), 10002, 10002, M_F(10004.0f),
        M_F(10004.0f), M_F(10004.0f), M_F(10004.0f), 0);
    M_Start();
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    M_Frames(1);
    const BULLET_SHOOTER *const shooter = &m_Enemy.shots.shooter;
    CHECK_EQ_INT(shooter->type, 2);
    CHECK_EQ_INT(shooter->color, 4);
    CHECK_EQ_INT(shooter->columns, 6);
    CHECK_EQ_INT(shooter->rows, 6);
    CHECK(shooter->speed1 == 1.5f && shooter->speed2 == 1.5f);
    CHECK(shooter->angle1 == 1.5f && shooter->angle2 == 1.5f);
    M_TearDown();
}

TEST(hold_fire_sets_the_shot_up_without_firing)
{
    M_SetUp("hold_fire");
    EclBuilder_BeginSub(&m_Builder);
    M_NOARGS(0, 0, 75);
    M_Fire(0, 0, 1);
    M_NOARGS(0, 1, 76);
    M_NOARGS(0, 1, 77);
    M_Start();
    M_Frames(1);
    CHECK(m_Enemy.flags.hold_fire);
    CHECK_EQ_INT(m_Enemy.shots.shooter.columns, 3);
    CHECK_EQ_INT(M_BulletCount(), 0);
    M_Frames(1);
    CHECK(!m_Enemy.flags.hold_fire);
    CHECK(M_BulletCount() > 0);
    M_TearDown();
}

TEST(fire_of_a_dead_enemy_does_nothing)
{
    M_SetUp("fire_dead");
    EclBuilder_BeginSub(&m_Builder);
    M_Fire(0, 0, 1);
    M_Start();
    m_Enemy.life = 0;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.shots.shooter.columns, 0);
    CHECK_EQ_INT(M_BulletCount(), 0);
    M_TearDown();
}

TEST(interval_shrinks_with_the_rank_and_fires_under_hold_fire)
{
    M_SetUp("interval_73");
    EclBuilder_BeginSub(&m_Builder);
    M_NOARGS(0, 0, 75);
    M_Fire(0, 0, 1);
    M_INSTR(0, 0, 73, 0, 10);
    M_Start();
    g_GameManager.rank.value = 32;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.shots.interval, 8);
    M_Frames(6);
    CHECK_EQ_INT(M_BulletCount(), 0);
    M_Frames(1);
    CHECK(M_BulletCount() > 0);
    CHECK_EQ_INT(m_Enemy.shots.timer.current, 0);
    M_TearDown();
}

TEST(interval_grows_at_rank_0_and_starts_at_a_random_phase)
{
    M_SetUp("interval_74");
    EclBuilder_BeginSub(&m_Builder);
    M_NOARGS(0, 0, 75);
    M_INSTR(0, 0, 74, 0, 100);
    M_INSTR(0, 1, 73, 0, 0);
    M_Start();
    g_GameManager.rank.value = 0;
    RNG expected = g_Rng;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.shots.interval, 120);
    const int32_t phase = (int32_t)Rng_U32InRange(&expected, 120);
    CHECK_EQ_INT(m_Enemy.shots.timer.current, phase + 1);
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.shots.interval, 0);
    CHECK_EQ_INT(m_Enemy.shots.timer.current, phase + 1);
    M_TearDown();
}

TEST(shooter_command_and_sounds_set_the_shooter)
{
    M_SetUp("shooter_79_81");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 79, 0, 2, 0x10, 1, 30, 4, M_F(1.5f), M_F(2.5f));
    M_INSTR(0, 0, 81, 0, 5, 7);
    M_INSTR(0, 1, 81, 0, M_NONE, M_NONE);
    M_Start();
    M_Frames(1);
    const BULLET_SHOOTER *const shooter = &m_Enemy.shots.shooter;
    const BULLET_COMMAND *const command = &shooter->commands[2];
    CHECK_EQ_INT(command->flag, 0x10);
    CHECK(command->concurrent);
    CHECK_EQ_INT(command->frames, 30);
    CHECK_EQ_INT(command->count, 4);
    CHECK(command->args[0] == 1.5f && command->args[1] == 2.5f);
    CHECK_EQ_INT(shooter->sound, 5);
    CHECK((shooter->flags & BULLET_FLAG_SOUND) != 0);
    CHECK_EQ_INT(shooter->bullet_sound, 7);
    M_Frames(1);
    CHECK((shooter->flags & BULLET_FLAG_SOUND) == 0);
    CHECK_EQ_INT(shooter->sound, 5);
    CHECK_EQ_INT(shooter->bullet_sound, -1);
    M_TearDown();
}

TEST(laser_spawn_fills_the_laser_shooter_and_its_slot)
{
    M_SetUp("laser_spawn");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 84, 0, 3);
    M_INSTR(
        0, 0, 82, 2, M_Pack(1, 10000), M_F(0.5f), M_F(2.0f), M_F(1.0f),
        M_F(3.0f), M_F(100.0f), M_F(8.0f), 10, 20, 30, 5, 6, 0);
    M_INSTR(
        0, 1, 83, 0, M_Pack(1, 0), M_F(0.5f), M_F(2.0f), M_F(1.0f), M_F(3.0f),
        M_F(100.0f), M_F(8.0f), 10, 20, 30, 5, 6, 0);
    M_Start();
    M_Ints()[0] = 2;
    M_Frames(1);
    const BULLET_SHOOTER *const shooter = &m_Enemy.shots.laser_shooter;
    CHECK_EQ_INT(shooter->type, 1);
    CHECK_EQ_INT(shooter->color, 2);
    CHECK_EQ_INT(shooter->aim, 1);
    CHECK(shooter->angle1 == 0.5f && shooter->speed1 == 2.0f);
    CHECK(shooter->laser.start_offset == 1.0f);
    CHECK(shooter->laser.end_offset == 3.0f);
    CHECK(shooter->laser.max_length == 100.0f);
    CHECK(shooter->laser.width == 8.0f);
    CHECK_EQ_INT(shooter->laser.start_frames, 10);
    CHECK_EQ_INT(shooter->laser.firing_frames, 20);
    CHECK_EQ_INT(shooter->laser.end_frames, 30);
    CHECK_EQ_INT(shooter->laser.hit_start_frame, 5);
    CHECK_EQ_INT(shooter->laser.end_hit_frames, 6);
    LASER *const laser = m_Enemy.shots.lasers[3];
    CHECK(laser != nullptr && laser->active);
    CHECK(m_Enemy.shots.lasers[0] == nullptr);
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.shots.laser_shooter.aim, 0);
    CHECK_EQ_INT(m_Enemy.shots.laser_shooter.color, 0);
    CHECK(m_Enemy.shots.lasers[3] != laser);
    M_TearDown();
}

TEST(laser_opcodes_change_the_laser_of_a_slot)
{
    M_SetUp("laser_ops");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 85, 0, 1, M_F(3.0f));
    M_INSTR(0, 0, 87, 0, 1, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_INSTR(0, 0, 156, 0, 1, 1);
    M_INSTR(0, 0, 157, 0, 1, M_F(64.0f));
    M_INSTR(0, 0, 158, 0, 1, M_F(4.0f), M_F(12.0f));
    M_INSTR(0, 0, 85, 0, 2, M_F(1.0f));
    M_INSTR(0, 1, 152, 0, 1, M_F(0.25f));
    M_INSTR(0, 2, 86, 0, 1, M_F(0.25f));
    M_Start();
    LASER *const laser = &g_Bullets.lasers[0];
    laser->angle = 1.0f;
    m_Enemy.shots.lasers[1] = laser;
    m_Enemy.move.pos = (VEC3) { 10.0f, 20.0f, 30.0f };
    M_Frames(1);
    CHECK(laser->angle == Angle_AddNormalize(1.0f, 3.0f));
    CHECK(laser->pos.x == 11.0f && laser->pos.y == 22.0f);
    CHECK(laser->pos.z == 33.0f);
    CHECK(laser->hide_origin_while_starting);
    CHECK(laser->max_length == 64.0f);
    CHECK(laser->start_offset == 4.0f && laser->end_offset == 12.0f);
    M_Frames(1);
    CHECK(laser->angle == 0.25f);
    M_Frames(1);
    CHECK(laser->angle == 0.75f);
    M_TearDown();
}

TEST(laser_end_starts_the_ending_from_the_shown_width)
{
    M_SetUp("laser_end");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 89, 0, 0);
    M_INSTR(0, 0, 89, 0, 1);
    M_INSTR(0, 0, 89, 0, 2);
    M_NOARGS(0, 1, 134);
    M_Start();
    LASER *const lasers = g_Bullets.lasers;
    for (int32_t i = 0; i < 3; i++) {
        lasers[i].active = true;
        lasers[i].state = LASER_FIRING;
        lasers[i].width = 10.0f;
        lasers[i].shown_width = 4.0f;
        Timer_Set(&lasers[i].timer, 7);
        m_Enemy.shots.lasers[i] = &lasers[i];
    }
    lasers[1].state = LASER_ENDING;
    lasers[2].active = false;
    M_Frames(1);
    CHECK_EQ_INT(lasers[0].state, LASER_ENDING);
    CHECK_EQ_INT(lasers[0].timer.current, 0);
    CHECK(lasers[0].width == 4.0f);
    CHECK_EQ_INT(lasers[1].timer.current, 7);
    CHECK(lasers[1].width == 10.0f);
    CHECK_EQ_INT(lasers[2].state, LASER_FIRING);
    M_Frames(1);
    for (int32_t i = 0; i < ENEMY_LASER_COUNT; i++) {
        CHECK(m_Enemy.shots.lasers[i] == nullptr);
    }
    CHECK(lasers[0].active);
    M_TearDown();
}

TEST(dead_writes_still_evaluate_their_operand)
{
    M_SetUp("dead_writes");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10029, 1);
    M_INSTR(0, 0, 4, 1, 10030, 0);
    M_INSTR(0, 0, 88, 1, 10056);
    M_INSTR(0, 0, 147, 1, 10055);
    M_Start();
    RNG expected = g_Rng;
    M_Frames(1);
    Rng_U32InRange(&expected, 1);
    Rng_U32(&expected);
    CHECK(memcmp(&expected, &g_Rng, sizeof(RNG)) == 0);
    M_TearDown();
}

TEST(position_is_clamped_to_the_bounds)
{
    M_SetUp("move_46");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 62, 0, M_F(0.0f), M_F(0.0f), M_F(100.0f), M_F(100.0f));
    M_INSTR(0, 0, 46, 0, M_F(150.0f), M_F(50.0f), M_F(0.0f));
    M_NOARGS(0, 1, 63);
    M_INSTR(0, 1, 46, 0, M_F(150.0f), M_F(50.0f), M_F(0.0f));
    M_Start();
    M_Frames(1);
    CHECK(m_Enemy.flags.clamp_pos);
    CHECK(m_Enemy.move.bounds_max.y == 100.0f);
    CHECK(m_Enemy.move.pos.x == 100.0f && m_Enemy.move.pos.y == 50.0f);
    M_Frames(1);
    CHECK(!m_Enemy.flags.clamp_pos);
    CHECK(m_Enemy.move.pos.x == 150.0f);
    M_TearDown();
}

TEST(velocity_sets_the_angle_of_the_move)
{
    M_SetUp("move_47");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 47, 0, M_F(1.0f), M_F(2.0f), M_F(0.0f));
    M_Start();
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_VELOCITY);
    CHECK(m_Enemy.move.velocity.y == 2.0f);
    CHECK(m_Enemy.move.angle == X87_Atan2(2.0f, 1.0f));
    M_TearDown();
}

TEST(angle_move_runs_at_the_game_speed_until_its_timer_ends)
{
    M_SetUp("move_angle");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 49, 0, M_F(2.0f));
    M_INSTR(0, 0, 48, 0, M_F(0.25f));
    M_INSTR(0, 0, 50, 0, M_F(0.5f));
    M_INSTR(0, 0, 59, 0, 2);
    M_Start();
    g_GameSpeed = 0.5f;
    m_Enemy.move.angle = 1.0f;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_ANGLE);
    CHECK(m_Enemy.move.angle == 1.125f);
    CHECK(m_Enemy.move.speed == 2.25f);
    VEC3 velocity = {};
    X87_SinCosScale(1.125f, 2.25f, &velocity.x, &velocity.y);
    CHECK(m_Enemy.move.velocity.x == velocity.x);
    CHECK(m_Enemy.move.velocity.y == velocity.y);
    // The timer ends on its whole frames, half a frame early.
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_ANGLE);
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_VELOCITY);
    M_TearDown();
}

TEST(aim_and_move_at_angle_set_the_angle_move)
{
    M_SetUp("move_53_54");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 53, 0, M_F(0.25f), M_F(3.0f));
    M_INSTR(0, 1, 54, 0, 0, 0, M_F(7.0f), M_F(1.5f));
    M_INSTR(0, 2, 54, 0, 4, 9, M_F(0.0f), M_F(2.0f));
    M_Start();
    M_Frames(1);
    CHECK(m_Enemy.move.angle == 0.75f);
    CHECK(m_Enemy.move.speed == 3.0f);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_ANGLE);
    M_Frames(1);
    CHECK(m_Enemy.move.angle == Angle_AddNormalize(7.0f, 0.0f));
    CHECK(m_Enemy.move.speed == 1.5f);
    CHECK_EQ_INT(m_Enemy.move.duration, 0);
    m_Enemy.move.pos = (VEC3) { 5.0f, 6.0f, 0.0f };
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.flags.ease, ENEMY_EASE_IN);
    CHECK(m_Enemy.move.delta.x == 8.0f && m_Enemy.move.delta.y == 0.0f);
    CHECK(m_Enemy.move.start.x == 5.0f && m_Enemy.move.start.y == 6.0f);
    CHECK_EQ_INT(m_Enemy.move.duration, 4);
    M_TearDown();
}

TEST(timed_move_at_angle_rounds_each_product_once)
{
    M_SetUp("move_54_rounding");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 54, 0, 60, 0, M_F(1.0f), M_F(1.25f));
    M_Start();
    M_Frames(1);
    float x;
    float y;
    X87_SinCosScale(1.0f, 1.25f, &x, &y);
    CHECK(m_Enemy.move.delta.x == x * 60.0f);
    CHECK(m_Enemy.move.delta.y == y * 60.0f);
    M_TearDown();
}

TEST(polar_operands_round_each_product_once)
{
    M_SetUp("polar_151");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 151, 3, M_F(10004.0f), M_F(10005.0f), M_F(1.0f), M_F(1.25f));
    M_Start();
    M_Frames(1);
    float x;
    float y;
    X87_SinCosScale(1.0f, 1.25f, &x, &y);
    CHECK(M_Floats()[0] == x);
    CHECK(M_Floats()[1] == y);
    M_TearDown();
}

// Returns the velocity.x after the first frame of a move to x 1 over 4
// frames with ease.
static float M_EasedStep(const int32_t ease)
{
    EclBuilder_Init(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 55, 0, 4, (uint32_t)ease, M_F(1.0f), M_F(0.0f), M_F(0.0f));
    M_Start();
    M_Frames(1);
    return m_Enemy.move.velocity.x;
}

TEST(move_to_eases_its_progress)
{
    M_SetUp("move_ease");
    CHECK(M_EasedStep(ENEMY_EASE_LINEAR) == 0.25f);
    CHECK(M_EasedStep(ENEMY_EASE_IN) == 0.0625f);
    CHECK(M_EasedStep(ENEMY_EASE_IN3) == 0.015625f);
    CHECK(M_EasedStep(ENEMY_EASE_IN4) == 0.00390625f);
    CHECK(M_EasedStep(ENEMY_EASE_OUT) == 0.4375f);
    CHECK(M_EasedStep(ENEMY_EASE_OUT3) == 0.578125f);
    CHECK(M_EasedStep(ENEMY_EASE_OUT4) == 0.68359375f);
    CHECK(M_EasedStep(7) == 0.25f);
    CHECK(M_EasedStep(9) == 0.0625f);
    M_TearDown();
}

TEST(move_to_runs_at_the_game_speed_and_lands_on_its_target)
{
    M_SetUp("move_55");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 55, 0, 4, 1, M_F(8.0f), M_F(4.0f), M_F(0.0f));
    M_Start();
    g_GameSpeed = 0.5f;
    m_Enemy.move.velocity = (VEC3) { 9.0f, 9.0f, 9.0f };
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_TO);
    CHECK(m_Enemy.move.velocity.x == 8.0f * 0.015625f);
    CHECK(m_Enemy.move.velocity.y == 4.0f * 0.015625f);
    M_Frames(5);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_TO);
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_VELOCITY);
    CHECK(m_Enemy.move.pos.x == 8.0f && m_Enemy.move.pos.y == 4.0f);
    CHECK(m_Enemy.move.velocity.x == 0.0f);
    M_TearDown();
}

TEST(move_to_of_a_mirrored_enemy_turns_x_twice)
{
    M_SetUp("move_mirrored");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 55, 0, 4, 0, M_F(8.0f), M_F(4.0f), M_F(0.0f));
    M_Start();
    m_Enemy.flags.mirrored = true;
    M_Frames(1);
    CHECK(m_Enemy.move.delta.x == -8.0f);
    CHECK(m_Enemy.move.velocity.x == 2.0f);
    CHECK(m_Enemy.move.velocity.y == 1.0f);
    M_TearDown();
}

TEST(circle_move_turns_and_widens_at_the_game_speed)
{
    M_SetUp("move_circle");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(
        0, 0, 56, 0, 0, M_F(10.0f), M_F(20.0f), M_F(0.0f), M_F(0.0f), M_F(0.5f),
        M_F(4.0f), M_F(1.0f));
    M_INSTR(0, 1, 57, 0, M_F(2.0f), M_F(0.0f));
    M_INSTR(0, 1, 58, 0, M_F(1.0f), M_F(0.0f));
    M_Start();
    g_GameSpeed = 0.5f;
    m_Enemy.move.pos = (VEC3) { 1.0f, 2.0f, 0.0f };
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_CIRCLE);
    CHECK(m_Enemy.move.circle.angle == 0.25f);
    CHECK(m_Enemy.move.circle.radius == 4.5f);
    float x;
    float y;
    X87_SinCosScale(0.25f, 4.5f, &x, &y);
    CHECK(m_Enemy.move.velocity.x == x + 10.0f - 1.0f);
    CHECK(m_Enemy.move.velocity.y == y + 20.0f - 2.0f);
    CHECK(
        m_Enemy.move.angle
        == X87_Atan2(m_Enemy.move.velocity.y, m_Enemy.move.velocity.x));
    M_Frames(2);
    CHECK(m_Enemy.move.circle.radius == 2.0f);
    CHECK(m_Enemy.move.circle.angle == 1.0f);
    M_TearDown();
}

TEST(random_angle_near_the_right_bound_mirrors_the_move_angle)
{
    M_SetUp("move_52");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(
        0, 0, 62, 0, M_F(-1000.0f), M_F(-1000.0f), M_F(200.0f), M_F(1000.0f));
    M_INSTR(0, 0, 52, 1, M_F(10004.0f));
    M_Start();
    m_Enemy.move.pos = (VEC3) { 150.0f, 0.0f, 0.0f };
    m_Enemy.move.angle = 0.25f;
    g_Player.pos.x = 300.0f;
    RNG expected = g_Rng;
    M_Frames(1);
    const float drawn = Rng_F32InRange(&expected, M_HALF_PI) - M_QUARTER_PI;
    CHECK(drawn >= 0.0f);
    CHECK(M_Floats()[0] == M_PI_F - 0.25f);
    M_TearDown();
}

TEST(random_angle_turns_from_the_left_bound_and_the_vertical_ones)
{
    M_SetUp("move_52_left");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 62, 0, M_F(100.0f), M_F(-1000.0f), M_F(1000.0f), M_F(10.0f));
    M_INSTR(0, 0, 52, 1, M_F(10004.0f));
    M_Start();
    m_Enemy.move.pos = (VEC3) { 150.0f, 0.0f, 0.0f };
    g_Player.pos.x = 0.0f;
    RNG expected = g_Rng;
    M_Frames(1);
    const float drawn = Angle_AddNormalize(
        Rng_F32InRange(&expected, M_HALF_PI) + M_THREE_QUARTER_PI, 0.0f);
    float angle = drawn > 0.0f ? M_PI_F - drawn : -M_PI_F - drawn;
    if (angle > 0.0f) {
        angle = -angle;
    }
    CHECK(M_Floats()[0] == angle);
    M_TearDown();
}

TEST(random_angle_155_faces_the_player_or_the_left_edge)
{
    M_SetUp("move_155");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 155, 1, M_F(10004.0f));
    M_INSTR(0, 1, 155, 1, M_F(10004.0f));
    M_INSTR(0, 2, 155, 1, M_F(10004.0f));
    M_Start();
    RNG expected = g_Rng;
    m_Enemy.move.pos.x = 100.0f;
    g_Player.pos.x = 0.0f;
    M_Frames(1);
    CHECK(
        M_Floats()[0]
        == Angle_AddNormalize(
            Rng_F32InRange(&expected, M_HALF_PI) + M_THREE_QUARTER_PI, 0.0f));
    m_Enemy.move.pos.x = 90.0f;
    M_Frames(1);
    CHECK(M_Floats()[0] == Rng_F32InRange(&expected, M_HALF_PI) - M_QUARTER_PI);
    m_Enemy.move.pos.x = 290.0f;
    g_Player.pos.x = 300.0f;
    M_Frames(1);
    CHECK(
        M_Floats()[0]
        == Angle_AddNormalize(
            Rng_F32InRange(&expected, M_HALF_PI) + M_THREE_QUARTER_PI, 0.0f));
    M_TearDown();
}

TEST(curve_runs_at_the_game_speed_and_ends)
{
    M_SetUp("curve_speed");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 27, 0, M_F(10004.0f), 4, 0, 0, M_F(0.0f), M_F(8.0f), 0, 0);
    M_INSTR(0, 0, 27, 0, M_F(10005.0f), 2, 3, 1, M_F(0.0f), M_F(8.0f), 0, 0);
    M_Start();
    g_GameSpeed = 0.5f;
    M_Frames(1);
    CHECK(M_Floats()[0] == 1.0f);
    CHECK(M_Floats()[1] == 0.5f);
    M_Frames(3);
    CHECK(M_Floats()[1] == 8.0f);
    CHECK(!m_Enemy.script.state.interps[1].active);
    CHECK(m_Enemy.script.state.interps[0].active);
    M_Frames(4);
    CHECK(M_Floats()[0] == 8.0f);
    CHECK(!m_Enemy.script.state.interps[0].active);
    M_TearDown();
}

TEST(curve_7_is_a_hermite_curve)
{
    M_SetUp("curve_hermite");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(
        0, 0, 27, 0, M_F(10004.0f), 2, 7, 0, M_F(0.0f), M_F(1.0f), M_F(4.0f),
        M_F(0.0f));
    M_INSTR(
        0, 0, 27, 0, M_F(10005.0f), 2, 7, 0, M_F(0.0f), M_F(1.0f), M_F(0.0f),
        M_F(4.0f));
    M_Start();
    M_Frames(1);
    CHECK(M_Floats()[0] == 1.0f);
    CHECK(M_Floats()[1] == 0.0f);
    M_TearDown();
}

TEST(curve_resolves_its_ends_again_each_frame)
{
    M_SetUp("curve_resolve");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 5, 1, M_F(10005.0f), M_F(10006.0f));
    M_INSTR(0, 0, 5, 1, M_F(10006.0f), M_F(2.0f));
    M_INSTR(
        0, 0, 27, 0x10, M_F(10004.0f), 4, 0, 0, M_F(10005.0f), M_F(10.0f), 0,
        0);
    M_Start();
    M_Frames(1);
    CHECK(m_Enemy.script.state.interps[0].from == 10006.0f);
    CHECK(M_Floats()[0] == 4.0f);
    M_Floats()[2] = 6.0f;
    M_Frames(1);
    CHECK(M_Floats()[0] == 8.0f);
    M_TearDown();
}

TEST(curve_on_itself_starts_from_its_last_value)
{
    M_SetUp("curve_itself");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 5, 1, M_F(10004.0f), M_F(2.0f));
    M_INSTR(
        0, 0, 27, 0, M_F(10004.0f), 4, 0, 0, M_F(10004.0f), M_F(10.0f), 0, 0);
    M_Start();
    M_Frames(1);
    CHECK(M_Floats()[0] == 4.0f);
    M_Frames(1);
    CHECK(M_Floats()[0] == 7.0f);
    M_Frames(1);
    CHECK(M_Floats()[0] == 9.25f);
    M_TearDown();
}

TEST(curve_on_the_position_becomes_the_velocity)
{
    M_SetUp("curve_pos");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 27, 0, M_F(10018.0f), 4, 0, 0, M_F(0.0f), M_F(8.0f), 0, 0);
    M_INSTR(0, 0, 27, 0, M_F(10019.0f), 4, 0, 0, M_F(0.0f), M_F(4.0f), 0, 0);
    M_Start();
    m_Enemy.move.pos = (VEC3) { 1.0f, 1.0f, 0.0f };
    M_Frames(1);
    CHECK(m_Enemy.move.pos.x == 1.0f && m_Enemy.move.pos.y == 1.0f);
    CHECK(m_Enemy.move.velocity.x == 1.0f);
    CHECK(m_Enemy.move.velocity.y == 0.0f);
    CHECK(m_Enemy.move.angle == 0.0f);
    M_TearDown();
}

TEST(curve_of_a_variable_replaces_its_running_curve)
{
    M_SetUp("curve_slots");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 27, 0, M_F(10004.0f), 8, 0, 0, M_F(0.0f), M_F(8.0f), 0, 0);
    M_INSTR(0, 0, 27, 0, M_F(10005.0f), 8, 0, 0, M_F(0.0f), M_F(8.0f), 0, 0);
    M_INSTR(0, 1, 27, 0, M_F(10004.0f), 2, 0, 0, M_F(0.0f), M_F(8.0f), 0, 0);
    M_Start();
    M_Frames(2);
    const ECL_INTERP *const interps = m_Enemy.script.state.interps;
    CHECK_EQ_INT(interps[0].duration, 2);
    CHECK(M_Floats()[0] == 4.0f);
    CHECK(interps[1].var == 10005.0f);
    CHECK(!interps[2].active);
    M_TearDown();
}

// Starts a spell card on boss 0, whose timer callback is 420 frames.
static void M_StartSpellCard(void)
{
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 99, 0, 0);
    M_INSTR(0, 0, 114, 0, 420);
    EclBuilder_SpellCard(&m_Builder, 0, 0, 3, 0, "abc");
}

TEST(spell_card_start_sets_the_card_and_its_circle_up)
{
    M_SetUp("spell_start");
    M_StartSpellCard();
    M_Start();
    m_Enemy.shots.rank_columns_high = 5;
    g_Stage.spell_card.frames = 9;
    M_Frames(1);
    CHECK_EQ_INT(m_Spell.names, 1);
    CHECK_EQ_INT(m_Spell.sprite, 3);
    CHECK_EQ_STR(m_Spell.name, "abc");
    CHECK_EQ_INT(g_Stage.spell_card.state, 1);
    CHECK_EQ_INT(g_Stage.spell_card.frames, 0);
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_RUNNING);
    CHECK(g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(g_Enemies.spell_card.index, 0);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus_step, 2000000 / 17);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 2000000);
    CHECK(m_Enemy.shots.rank_speed_low == -0.5f);
    CHECK(m_Enemy.shots.rank_speed_high == 0.5f);
    CHECK_EQ_INT(m_Enemy.shots.rank_columns_high, 0);
    const EFFECT *const circle = m_Enemy.orbits.effects[ENEMY_SPELL_CIRCLE];
    CHECK(circle != nullptr && circle->active);
    CHECK_EQ_INT(circle->type, EFFECT_SPELL_CIRCLE);
    CHECK_EQ_INT(circle->vm.interp_length[ANM_INTERP_SCALE], 420);
    CHECK(circle->vm.scale_to.x == 0.125f && circle->vm.scale_to.y == 0.125f);
    CHECK_EQ_INT(m_Spell.attempts, 1);
    CHECK_EQ_INT(m_Spell.attempt_index, 0);
    CHECK_EQ_STR(m_Spell.attempt_name, "abc");
    M_TearDown();
}

TEST(spell_card_start_in_a_replay_records_no_attempt)
{
    M_SetUp("spell_replay");
    M_StartSpellCard();
    M_Start();
    g_GameManager.mode.in_replay = true;
    M_Frames(1);
    CHECK_EQ_INT(m_Spell.names, 1);
    CHECK_EQ_INT(m_Spell.attempts, 0);
    M_TearDown();
}

TEST(spell_card_bonus_falls_by_the_frame_and_drops_its_last_digit)
{
    M_SetUp("spell_bonus");
    M_StartSpellCard();
    M_Start();
    M_Frames(2);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 1998030);
    g_GameSpeed = 0.5f;
    M_Frames(1);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 1996070);
    M_Frames(1);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 1995090);
    g_Enemies.spell_card.capturable = false;
    M_Frames(1);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 1995090);
    M_TearDown();
}

TEST(spell_card_bonus_holds_while_the_boss_keeps_the_card)
{
    M_SetUp("spell_keep");
    M_StartSpellCard();
    M_INSTR(0, 1, 135, 0, 1);
    M_Start();
    M_Frames(3);
    CHECK(m_Enemy.flags.keep_spell_card);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 2000000);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus_timer.current, 3);
    M_TearDown();
}

TEST(spell_card_end_captures_the_card)
{
    M_SetUp("spell_end");
    M_StartSpellCard();
    M_NOARGS(0, 1, 91);
    M_Start();
    M_Frames(1);
    const EFFECT *const circle = m_Enemy.orbits.effects[ENEMY_SPELL_CIRCLE];
    g_Enemies.spell_card.graze_bonus = 500;
    g_Enemies.spell_card.bonus = 2000000;
    const uint32_t score = g_GameManager.score.value;
    Sound_Reset();
    M_Frames(1);
    CHECK_EQ_INT(m_Spell.ends, 1);
    CHECK_EQ_INT(m_Spell.bonuses, 1);
    CHECK_EQ_INT(m_Spell.bonus, 2000500);
    CHECK_EQ_INT(m_Spell.captures, 1);
    CHECK_EQ_INT(m_Spell.capture_bonus, 2000500);
    CHECK_EQ_INT(g_GameManager.stats.spell_cards_captured, 1);
    CHECK_EQ_INT(g_GameManager.score.value - score, 2000500 / 10);
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_NONE);
    CHECK_EQ_INT(g_Stage.spell_card.state, 0);
    CHECK(m_Enemy.orbits.effects[ENEMY_SPELL_CIRCLE] == nullptr);
    CHECK(!circle->active);
    CHECK_EQ_INT(g_SoundEvents.queue[0], 0xf);
    M_TearDown();
}

TEST(spell_card_end_without_a_capture_shows_no_bonus)
{
    M_SetUp("spell_end_lost");
    M_StartSpellCard();
    M_NOARGS(0, 1, 91);
    M_NOARGS(0, 2, 91);
    M_Start();
    M_Frames(1);
    g_Enemies.spell_card.state = ENEMY_SPELL_TIMED_OUT;
    M_Frames(1);
    CHECK_EQ_INT(m_Spell.ends, 1);
    CHECK_EQ_INT(m_Spell.bonuses, 0);
    CHECK_EQ_INT(m_Spell.captures, 0);
    M_Frames(1);
    CHECK_EQ_INT(m_Spell.ends, 1);
    M_TearDown();
}

// Returns whether boss 0 of stage hides during a bomb in spell card
// index.
static bool M_HiddenByBomb(const int32_t stage, const int32_t index)
{
    M_InitEnemy(&m_Enemy, 0);
    m_Enemy.flags.boss = true;
    g_GameManager.run.stage = stage;
    g_Player.bomb.running = true;
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.index = index;
    M_Frames(1);
    return m_Enemy.flags.hidden_by_bomb;
}

TEST(late_spell_cards_of_the_extra_stages_hide_the_boss_during_a_bomb)
{
    M_SetUp("hidden_by_bomb");
    EclBuilder_BeginSub(&m_Builder);
    CHECK(M_Loads());
    CHECK(M_HiddenByBomb(7, 0x76));
    CHECK(M_HiddenByBomb(8, 0x80));
    CHECK(!M_HiddenByBomb(6, 0x76));
    CHECK(!M_HiddenByBomb(7, 0x75));
    CHECK(M_HiddenByBomb(7, 0x76));
    g_Player.bomb.running = false;
    M_Frames(1);
    CHECK(m_Enemy.flags.hidden_by_bomb);
    CHECK_EQ_INT(m_Enemy.bomb_hide_frames, 0);
    M_Frames(1);
    CHECK(!m_Enemy.flags.hidden_by_bomb);
    M_TearDown();
}

TEST(boss_slot_sets_the_boss_bar_and_its_marker)
{
    M_SetUp("boss_slot");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 99, 0, 2);
    M_INSTR(0, 1, 99, 0, M_NONE);
    M_Start();
    M_Frames(1);
    CHECK(g_Enemies.bosses[2] == &m_Enemy);
    CHECK(m_Enemy.flags.boss);
    CHECK_EQ_INT(m_Enemy.boss_index, 2);
    CHECK(g_Gui.boss.present);
    CHECK(g_Gui.boss.health == 1.0f);
    CHECK_EQ_INT(g_AsciiText.boss_markers[2].pending_interrupt, 1);
    M_Frames(1);
    CHECK(g_Enemies.bosses[2] == nullptr);
    CHECK(!m_Enemy.flags.boss);
    CHECK(!g_Gui.boss.present);
    CHECK_EQ_INT(g_AsciiText.boss_markers[2].pending_interrupt, 2);
    M_TearDown();
}

TEST(boss_slot_past_the_markers_keeps_them_and_the_bar)
{
    M_SetUp("boss_slot_5");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 99, 0, 5);
    M_INSTR(0, 1, 99, 0, M_NONE);
    M_Start();
    M_Frames(1);
    CHECK(g_Enemies.bosses[5] == &m_Enemy);
    for (int32_t i = 0; i < ASCII_BOSS_MARKER_COUNT; i++) {
        CHECK_EQ_INT(g_AsciiText.boss_markers[i].pending_interrupt, 0);
    }
    M_Frames(1);
    CHECK(g_Enemies.bosses[5] == nullptr);
    CHECK(g_Gui.boss.present);
    M_TearDown();
}

TEST(life_clears_the_bar_sections_of_boss_0)
{
    M_SetUp("boss_110_139");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 110, 0, 200);
    M_INSTR(0, 0, 139, 0, 1, 50, 150, 0xff);
    M_INSTR(0, 1, 110, 0, 100);
    M_Start();
    g_Gui.boss.sections[3].end = 0.5f;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.life, 200);
    CHECK_EQ_INT(m_Enemy.max_life, 200);
    CHECK(g_Gui.boss.sections[3].end == 0.5f);
    CHECK(g_Gui.boss.sections[1].end == 0.25f);
    CHECK(g_Gui.boss.sections[1].start == 0.75f);
    CHECK_EQ_INT(g_Gui.boss.sections[1].color, 0xff);
    m_Enemy.flags.boss = true;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.life, 100);
    CHECK(g_Gui.boss.sections[1].end == 0.0f);
    CHECK(g_Gui.boss.sections[1].start == 0.0f);
    CHECK(g_Gui.boss.sections[3].end == 0.0f);
    M_TearDown();
}

TEST(boss_lives_add_half_a_minute_of_progress)
{
    M_SetUp("boss_126");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 126, 0, 3);
    M_Start();
    g_GameManager.run.progress_frames = 10;
    M_Frames(1);
    CHECK_EQ_INT(g_Gui.boss.lives, 3);
    CHECK_EQ_INT(g_GameManager.run.progress_frames, 1810);
    M_TearDown();
}

TEST(orbit_spawns_a_pale_orbit_effect)
{
    M_SetUp("effect_100");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 100, 0, 5, M_F(1.0f), M_F(2.0f), M_F(3.0f), M_F(16.0f));
    M_Start();
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.orbits.count, 1);
    CHECK(m_Enemy.orbits.radius == 16.0f);
    const EFFECT *const orbit = m_Enemy.orbits.effects[0];
    CHECK_EQ_INT(orbit->type, EFFECT_ORBIT);
    CHECK(orbit->axis.x == 1.0f && orbit->axis.z == 3.0f);
    CHECK_EQ_INT(orbit->vm.color, COLOR_ARGB(0xff, 0xff, 0xe0, 0xe0));
    M_TearDown();
}

TEST(effect_reads_its_color_through_the_variable_table)
{
    M_SetUp("effect_117");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10000, COLOR_ARGB(0xff, 0x00, 0xff, 0x00));
    M_INSTR(0, 1, 117, 4, 0, 1, 10000);
    M_INSTR(0, 2, 117, 4, 0, 1, 10018);
    M_INSTR(0, 3, 118, 4, 0, 1, 10000, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_Start();
    m_Enemy.move.pos = (VEC3) { 7.0f, 8.0f, 0.0f };
    M_Frames(1);
    EFFECT *const before = Effect_Spawn(EFFECT_EXPLOSION, (VEC3) {}, 1, 0);
    M_Frames(1);
    CHECK_EQ_INT(before[1].type, EFFECT_EXPLOSION);
    CHECK(before[1].pos.x == 7.0f);
    CHECK_EQ_INT(before[1].vm.color, COLOR_ARGB(0xff, 0x00, 0xff, 0x00));
    M_Frames(1);
    CHECK_EQ_INT(before[2].vm.color, 10018);
    M_Frames(1);
    CHECK_EQ_INT(before[3].vm.color, COLOR_ARGB(0xff, 0x00, 0xff, 0x00));
    CHECK(before[3].velocity.y == 2.0f);
    M_TearDown();
}

TEST(drops_spread_power_or_point_items)
{
    M_SetUp("items_119_154");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 119, 0, 3);
    M_INSTR(0, 1, 119, 0, 1);
    M_INSTR(0, 2, 154, 0, 1);
    M_INSTR(0, 3, 124, 0, ITEM_BOMB);
    M_Start();
    m_Enemy.move.pos = (VEC3) { 100.0f, 200.0f, 0.0f };
    g_GameManager.player.power = 127.9f;
    RNG expected = g_Rng;
    M_Frames(1);
    CHECK_EQ_INT(m_ItemCount, 3);
    CHECK_EQ_INT(m_Items[0].type, ITEM_BIG_POWER);
    CHECK_EQ_INT(m_Items[1].type, ITEM_POWER);
    CHECK_EQ_INT(m_Items[2].type, ITEM_POWER);
    CHECK_EQ_INT(m_Items[0].state, ITEM_FALLING);
    const float x = Rng_F32InRange(&expected, 128.0f) - 64.0f;
    const float y = Rng_F32InRange(&expected, 128.0f) - 64.0f;
    CHECK(m_Items[0].pos.x == 100.0f + x);
    CHECK(m_Items[0].pos.y == 200.0f + y);
    g_GameManager.player.power = 128.0f;
    M_Frames(1);
    CHECK_EQ_INT(m_Items[3].type, ITEM_POINT);
    g_GameManager.player.power = 0.0f;
    M_Frames(1);
    CHECK_EQ_INT(m_Items[4].type, ITEM_POINT);
    M_Frames(1);
    CHECK_EQ_INT(m_ItemCount, 6);
    CHECK_EQ_INT(m_Items[5].type, ITEM_BOMB);
    CHECK(m_Items[5].pos.x == 100.0f);
    M_TearDown();
}

TEST(child_spawns_with_the_variables_absolute_or_relative)
{
    M_SetUp("spawn_92_93");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10000, 42);
    M_INSTR(0, 0, 92, 0, 1, M_F(10.0f), M_F(20.0f), M_F(0.0f), 5, 3, 100);
    M_INSTR(0, 1, 93, 0, 1, M_F(10.0f), M_F(20.0f), M_F(0.0f), 5, 3, 100);
    M_INSTR(1, 0, 17, 1, 10000);
    M_Start();
    m_Enemy.move.pos = (VEC3) { 1.0f, 2.0f, 0.0f };
    M_Frames(1);
    const ENEMY *const child = &g_Enemies.enemies[0];
    CHECK(child->flags.active);
    CHECK_EQ_INT(child->script.state.sub, 1);
    CHECK_EQ_INT(child->script.state.vars.ints[0], 43);
    CHECK(child->move.pos.x == 10.0f && child->move.pos.y == 20.0f);
    CHECK_EQ_INT(child->life, 5);
    CHECK_EQ_INT(child->death.item, 3);
    CHECK_EQ_INT(child->score, 100);
    M_Frames(1);
    CHECK(g_Enemies.enemies[1].move.pos.x == 11.0f);
    CHECK(g_Enemies.enemies[1].move.pos.y == 22.0f);
    M_TearDown();
}

TEST(child_spawn_of_a_dead_enemy_does_nothing)
{
    M_SetUp("spawn_dead");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 92, 0, 0, M_F(10.0f), M_F(20.0f), M_F(0.0f), 5, 3, 100);
    M_Start();
    m_Enemy.life = 0;
    M_Frames(1);
    CHECK(!g_Enemies.enemies[0].flags.active);
    M_TearDown();
}

TEST(bonus_clear_kills_the_other_enemies)
{
    M_SetUp("spawn_94");
    EclBuilder_BeginSub(&m_Builder);
    M_NOARGS(0, 0, 94);
    M_Start();
    ENEMY *const other = &g_Enemies.enemies[5];
    other->flags.active = true;
    other->life = 10;
    other->script.death_sub = -1;
    M_Frames(1);
    CHECK_EQ_INT(other->life, 0);
    M_TearDown();
}

TEST(patterns_run_once_or_each_frame_with_word_1)
{
    M_SetUp("patterns_121_122");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 4, 1, 10000, ECL_FUNC_SLOW_DOWN);
    M_INSTR(0, 0, 121, 1, 10000, 4);
    M_INSTR(1, 0, 122, 0, ECL_FUNC_SLOW_DOWN, 2);
    M_INSTR(1, 1, 122, 0, M_NONE, 0);
    M_Start();
    M_Frames(1);
    CHECK(g_GameSpeed == 0.25f);
    CHECK_EQ_INT(m_Enemy.script.state.frame_func, ECL_FUNC_NONE);
    M_InitEnemy(&m_Enemy, 1);
    g_GameSpeed = 1.0f;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.script.state.frame_func, ECL_FUNC_SLOW_DOWN);
    CHECK_EQ_INT(m_Enemy.script.state.frame_func_arg, 2);
    CHECK(g_GameSpeed == 0.5f);
    g_GameSpeed = 1.0f;
    M_Frames(1);
    CHECK(g_GameSpeed == 0.5f);
    g_GameSpeed = 1.0f;
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.script.state.frame_func, ECL_FUNC_NONE);
    CHECK(g_GameSpeed == 1.0f);
    M_TearDown();
}

TEST(cherry_label_and_sound_reach_their_modules)
{
    M_SetUp("world_160_125");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 160, 0, 500);
    M_INSTR(0, 0, 125, 0, 6);
    M_INSTR(0, 0, 105, 0, 9);
    M_Start();
    const int32_t plus = g_GameManager.cherry.plus;
    Sound_Reset();
    M_Frames(1);
    CHECK_EQ_INT(g_GameManager.cherry.plus - plus, 500);
    CHECK_EQ_INT(g_Stage.script.label, 6);
    CHECK_EQ_INT(g_SoundEvents.queue[0], 9);
    M_TearDown();
}

TEST(flag_setters_take_the_low_bits)
{
    M_SetUp("flags");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 102, 0, 3);
    M_INSTR(0, 0, 103, 0, 2);
    M_INSTR(0, 0, 104, 0, 1);
    M_INSTR(0, 0, 106, 0, 9);
    M_INSTR(0, 0, 116, 0, 5);
    M_INSTR(0, 0, 120, 0, 7);
    M_INSTR(0, 0, 132, 0, 1);
    M_INSTR(0, 0, 136, 0, 1);
    M_INSTR(0, 0, 137, 0, 1);
    M_INSTR(0, 0, 4, 1, 10000, 3);
    M_INSTR(0, 0, 161, 1, 10000);
    M_Start();
    m_Enemy.flags.takes_damage = true;
    M_Frames(1);
    CHECK(m_Enemy.flags.contact_damage);
    CHECK(!m_Enemy.flags.takes_damage);
    CHECK(m_Enemy.flags.hit_by_shots);
    CHECK_EQ_INT(m_Enemy.flags.death, ENEMY_DEATH_KEEP);
    CHECK(m_Enemy.flags.shootable);
    CHECK(m_Enemy.flags.rotate_sprite);
    CHECK(m_Enemy.flags.hidden);
    CHECK(m_Enemy.flags.grazeable);
    CHECK_EQ_INT(m_Enemy.draw_list, ENEMY_DRAW_HIGH);
    CHECK(m_Enemy.flags.stays_off_screen);
    CHECK(m_Enemy.flags.halted_by_bomb);
    M_TearDown();
}

TEST(death_sub_is_a_byte_and_the_timer_callback_can_take_it)
{
    M_SetUp("death_107_133");
    EclBuilder_BeginSub(&m_Builder);
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 107, 0, 0x7f01);
    M_INSTR(0, 0, 111, 0, 9);
    M_INSTR(0, 1, 133, 0, 0);
    M_INSTR(0, 1, 142, 0, 30);
    M_INSTR(0, 1, 148, 0, 2, 500, 1);
    M_Start();
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.script.death_sub, 1);
    M_Frames(1);
    CHECK_EQ_INT(m_Enemy.callbacks.time_sub, 1);
    CHECK_EQ_INT(m_Enemy.callbacks.timer.current, 0);
    CHECK_EQ_INT(m_Enemy.invulnerable_timer.current, 30);
    CHECK_EQ_INT(m_Enemy.callbacks.life[2], 500);
    CHECK_EQ_INT(m_Enemy.callbacks.life_sub[2], 1);
    M_TearDown();
}

TEST(bullet_clears_free_the_bullets_near_or_all)
{
    M_SetUp("clears_143_146");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 143, 0, M_F(10.0f));
    M_NOARGS(0, 1, 146);
    M_Start();
    BULLET *const bullets = g_Bullets.bullets;
    bullets[0].state = BULLET_FLYING;
    bullets[0].pos = (VEC3) { 6.0f, 8.0f, 0.0f };
    bullets[1].state = BULLET_FLYING;
    bullets[1].pos = (VEC3) { 7.0f, 8.0f, 0.0f };
    M_Frames(1);
    CHECK_EQ_INT(bullets[0].state, BULLET_FREE);
    CHECK_EQ_INT(m_ItemCount, 1);
    CHECK_EQ_INT(m_Items[0].type, ITEM_STAR);
    CHECK_EQ_INT(bullets[1].state, BULLET_FLYING);
    M_Frames(1);
    CHECK(bullets[1].state != BULLET_FLYING);
    CHECK_EQ_INT(m_ItemCount, 1);
    M_TearDown();
}

TEST(spell_circle_detaches_or_moves_to_a_place)
{
    M_SetUp("circle_149");
    EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 0, 149, 0, 1, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_INSTR(0, 1, 149, 0, 2, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_Start();
    EFFECT *const circle = Effect_Spawn(EFFECT_EXPLOSION, (VEC3) {}, 1, 0);
    m_Enemy.orbits.effects[ENEMY_SPELL_CIRCLE] = circle;
    M_Frames(1);
    CHECK(m_Enemy.flags.spell_circle_detached);
    CHECK(circle->pos.x == 0.0f);
    M_Frames(1);
    CHECK(!m_Enemy.flags.spell_circle_detached);
    CHECK(circle->pos.x == 1.0f && circle->pos.z == 3.0f);
    M_TearDown();
}
