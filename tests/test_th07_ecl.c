#include "th07/ecl.h"
#include "th07/ecl/private.h"

#include "th07/ascii.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/stage.h"
#include "th07/stubs.h"

#include "test.h"
#include "test_dir.h"
#include "th07_data.h"
#include "th07_ecl_builder.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include "stg/chain.h"
#include "stg/rng.h"

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

// The test links these instead of the modules they stand for.

RNG g_Rng;
CHAIN g_Chain;
float g_GameSpeed;
PLAYER g_Player;
ASCII_TEXT g_AsciiText;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;
static ECL_BUILDER m_Builder;
static uint8_t m_File[ECL_BUILDER_MAX_SIZE];

float Player_AngleFrom(const VEC3 pos)
{
    return 0.5f;
}

PLAYER_CONTACT Player_CheckGraze(const VEC3 pos, const VEC3 size)
{
    return PLAYER_CONTACT_NONE;
}

PLAYER_CONTACT Player_CheckHit(const VEC3 pos, const VEC3 size)
{
    return PLAYER_CONTACT_NONE;
}

void Player_CheckLaserHit(
    const VEC3 pos, const VEC3 size, const VEC3 origin, const float angle,
    const bool can_graze)
{
}

int32_t Player_CalcDamage(
    const VEC3 pos, const VEC3 size, bool *const hit_by_bomb)
{
    return 0;
}

void Player_StartBorder(void)
{
}

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
}

void Item_Reset(void)
{
}

void Item_Update(void)
{
}

void Item_Draw(void)
{
}

void AsciiText_AddPopup(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
}

void Replay_MarkEvent(const REPLAY_EVENT event)
{
}

SCENE Scene_GetRequested(void)
{
    return SCENE_GAMEPLAY;
}

GUI g_Gui;
uint8_t g_EffectsQuality = 2;

void Gui_ShowSpellCardName(const int32_t sprite, const char *const name)
{
}

void Gui_EndSpellCardName(void)
{
}

void Gui_ShowSpellCardBonus(const int32_t bonus)
{
}

void ResultScreen_AddSpellCardAttempt(
    const int32_t index, const char *const name)
{
}

void ResultScreen_AddSpellCardCapture(const int32_t index, const int32_t bonus)
{
}

void Gui_StartMsg(const int32_t index)
{
}

bool Gui_MsgHoldsTimeline(void)
{
    return false;
}

bool Gui_IsMsgRunning(void)
{
    return false;
}

void Gui_ShowBonusPopup(const int32_t bonus)
{
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
}

bool Gui_IsLoadingShown(void)
{
    return false;
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
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    CHECK(SDL_CreateDirectory(data));
    Mem_Free(data);
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, M_SEED);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = 1;
    g_GameManager.run.difficulty = 1;
    memset(&g_Player, 0, sizeof(g_Player));
    memset(&g_Stage, 0, sizeof(g_Stage));
    g_GameSpeed = 1.0f;
    EclBuilder_Init(&m_Builder);
}

static void M_TearDown(void)
{
    Ecl_Release();
    Th07Data_Unmount();
    Mem_Free(m_Directory);
}

// Writes size bytes of m_File as the stage's script file and loads it.
static bool M_LoadBytes(const size_t size)
{
    Th07Data_WriteFile(m_Directory, M_PATH, m_File, size);
    Th07Data_Unmount();
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
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
