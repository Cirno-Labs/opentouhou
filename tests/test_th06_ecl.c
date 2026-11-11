#include "th06/ecl.h"

#include "anm_builder.h"
#include "ecl_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"
#include "th06_scene_stubs.h"

#include "th06/angle.h"
#include "th06/anm.h"
#include "th06/bullet.h"
#include "th06/effect.h"
#include "th06/enemy.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"
#include "th06/sound.h"
#include "th06/stage.h"

#include "core/base/memory.h"
#include "core/base/utils.h"
#include "core/math/common.h"
#include "core/math/vec.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_PI 3.1415927f
#define M_TWO_PI 6.2831855f
#define M_HALF_PI 1.5707964f
#define M_QUARTER_PI 0.7853982f
#define M_MAX_CALLS 64
#define M_ALL 0xff

// The test links these instead of the other modules. Each records its
// calls; the player is at m_AimAngle from everything and at m_AwayAngle
// to everything.

typedef struct {
    EFFECT_TYPE type;
    VEC3 pos;
    int32_t count;
    uint32_t color;
} M_EFFECT_CALL;

typedef struct {
    VEC3 pos;
    ITEM_TYPE type;
    ITEM_STATE state;
} M_ITEM_CALL;

typedef struct {
    int32_t sub;
    VEC3 pos;
    int16_t life;
    int16_t item;
    int32_t score;
} M_SPAWN_CALL;

PLAYER g_Player;
BULLET_MANAGER g_Bullets;
ENEMY_MANAGER g_Enemies;
GUI g_Gui;
STAGE g_Stage;

static char *m_Directory;
static ECL_BUILDER m_Builder;
static int32_t m_Sub;
static ENEMY m_Enemy;

static float m_AimAngle;
static float m_AwayAngle;
static VEC3 m_AngleFrom;
static BULLET_SHOOTER m_Fired[M_MAX_CALLS];
static int32_t m_FiredCount;
static BULLET_SHOOTER m_LaserShots[M_MAX_CALLS];
static LASER m_Lasers[M_MAX_CALLS];
static int32_t m_LaserCount;
static int32_t m_ClearToItems;
static int32_t m_ClearWithBonus;
static int32_t m_ClearMaxBonus;
static bool m_ClearItems;
static M_EFFECT_CALL m_EffectCalls[M_MAX_CALLS];
static EFFECT m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static M_ITEM_CALL m_ItemCalls[M_MAX_CALLS];
static int32_t m_ItemCount;
static M_SPAWN_CALL m_SpawnCall;
static int32_t m_SpawnCount;
static ENEMY m_SpareEnemy;
static int32_t m_ClampCalls;
static int32_t m_PresentationRenewals;
static int32_t m_ReleaseOrbitCalls;
static int32_t m_KillAllCalls;
static int32_t m_NameSprite;
static char m_Name[64];
static int32_t m_NameCalls;
static int32_t m_EndNameCalls;
static int32_t m_BonusShown;
static int32_t m_BonusCalls;
static ANM_SPRITE m_LargeSprite;
static ANM_SPRITE m_SmallSprite;

float Player_AngleFrom(const VEC3 pos)
{
    m_AngleFrom = pos;
    return m_AimAngle;
}

float Player_AngleTo(const VEC3 pos)
{
    return m_AwayAngle;
}

void Bullet_FirePattern(const BULLET_SHOOTER *const shooter)
{
    if (m_FiredCount < M_MAX_CALLS) {
        m_Fired[m_FiredCount] = *shooter;
    }
    m_FiredCount++;
}

LASER *Bullet_SpawnLaser(const BULLET_SHOOTER *const shooter)
{
    const int32_t index = MIN(m_LaserCount, M_MAX_CALLS - 1);
    m_LaserShots[index] = *shooter;
    m_LaserCount++;
    return &m_Lasers[index];
}

void Bullet_ClearAllToItems(void)
{
    m_ClearToItems++;
}

void Bullet_ClearAllWithBonus(const int32_t max_bonus, const bool to_items)
{
    m_ClearWithBonus++;
    m_ClearMaxBonus = max_bonus;
    m_ClearItems = to_items;
}

EFFECT *Effect_Spawn(
    const EFFECT_TYPE type, const VEC3 pos, const int32_t count,
    const uint32_t color)
{
    const int32_t index = MIN(m_EffectCount, M_MAX_CALLS - 1);
    m_EffectCalls[index] = (M_EFFECT_CALL) { type, pos, count, color };
    m_EffectCount++;
    return &m_Effects[index];
}

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
    if (m_ItemCount < M_MAX_CALLS) {
        m_ItemCalls[m_ItemCount] = (M_ITEM_CALL) { pos, type, state };
    }
    m_ItemCount++;
}

ENEMY *Enemy_Spawn(
    const int32_t sub, const VEC3 pos, const int16_t life, const int16_t item,
    const int32_t score)
{
    m_SpawnCall = (M_SPAWN_CALL) { sub, pos, life, item, score };
    m_SpawnCount++;
    return &m_SpareEnemy;
}

void Enemy_RenewPresentation(ENEMY *const enemy)
{
    m_PresentationRenewals++;
}

void Enemy_ClampPos(ENEMY *const enemy)
{
    m_ClampCalls++;
}

void Enemy_ReleaseOrbits(ENEMY *const enemy)
{
    m_ReleaseOrbitCalls++;
}

void Enemy_KillAll(void)
{
    m_KillAllCalls++;
}

void Gui_ShowSpellCardName(const int32_t sprite, const char *const name)
{
    m_NameCalls++;
    m_NameSprite = sprite;
    SDL_strlcpy(m_Name, name, sizeof(m_Name));
}

void Gui_EndSpellCardName(void)
{
    m_EndNameCalls++;
}

void Gui_ShowSpellCardBonus(const int32_t bonus)
{
    m_BonusCalls++;
    m_BonusShown = bonus;
}

static uint32_t M_F(const float value)
{
    return EclBuilder_Float(value);
}

static uint32_t M_U(const int32_t value)
{
    return (uint32_t)value;
}

// Return the operand of int variable n (0 to 7) and of float variable n
// (0 to 3), as an int operand, and the float variable as a float operand.
static uint32_t M_IntVar(const int32_t n)
{
    return M_U(n < 4 ? -10001 - n : -10009 - (n - 4));
}

static uint32_t M_FloatVar(const int32_t n)
{
    return M_U(-10005 - n);
}

static uint32_t M_FloatVarF(const int32_t n)
{
    return M_F((float)(-10005 - n));
}

static float M_Bits(const uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t M_FloatBits(const float value)
{
    return EclBuilder_Float(value);
}

#define M_WORDS(...)                                                           \
    (const uint32_t[]) { __VA_ARGS__ },                                        \
        (int32_t)(sizeof((uint32_t[]) { __VA_ARGS__ }) / sizeof(uint32_t))

// Appends an instruction for every difficulty to the current subroutine.
#define M_INSTR(time_, opcode_, ...)                                           \
    EclBuilder_Instr(                                                          \
        &m_Builder, m_Sub, (time_), (opcode_), M_ALL, M_WORDS(__VA_ARGS__))

static int32_t M_Instr0(const int32_t time, const int16_t opcode)
{
    return EclBuilder_Instr(&m_Builder, m_Sub, time, opcode, M_ALL, nullptr, 0);
}

// Points the jump at offset to the instruction at target.
static void M_Target(const int32_t offset, const int32_t target)
{
    EclBuilder_SetArg(&m_Builder, m_Sub, offset, 1, M_U(target - offset));
}

static void M_WriteSprites(void)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 256, 256);
    for (int32_t i = 0; i < 32; i++) {
        AnmBuilder_AddSprite(&builder, i, 0.0f, 0.0f, 8.0f, 8.0f);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, "sprites.anm", bytes, size);
}

static void M_ResetCalls(void)
{
    m_FiredCount = 0;
    m_LaserCount = 0;
    m_EffectCount = 0;
    m_ItemCount = 0;
    m_SpawnCount = 0;
    m_ClampCalls = 0;
    m_PresentationRenewals = 0;
    m_ReleaseOrbitCalls = 0;
    m_KillAllCalls = 0;
    m_NameCalls = 0;
    m_EndNameCalls = 0;
    m_BonusCalls = 0;
    m_ClearToItems = 0;
    m_ClearWithBonus = 0;
    memset(m_Effects, 0, sizeof(m_Effects));
    memset(m_Lasers, 0, sizeof(m_Lasers));
}

static void M_Begin(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    M_WriteSprites();
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    CHECK(Result_Log(Anm_Load(0, "sprites.anm", 0), LOG_LEVEL_ERROR));
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = 1;
    g_GameManager.run.difficulty = 1;
    memset(&g_Player, 0, sizeof(g_Player));
    memset(&g_Bullets, 0, sizeof(g_Bullets));
    memset(&g_Enemies, 0, sizeof(g_Enemies));
    memset(&g_Gui, 0, sizeof(g_Gui));
    memset(&g_Stage, 0, sizeof(g_Stage));
    SceneStubCalls_Reset();
    M_ResetCalls();
    m_AimAngle = 0.0f;
    m_AwayAngle = 0.0f;
    m_LargeSprite = (ANM_SPRITE) { .slot = -1, .height = 32.0f };
    m_SmallSprite = (ANM_SPRITE) { .slot = -1, .height = 16.0f };
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
}

static void M_End(void)
{
    Ecl_Release();
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

// Writes the file of stage 1. The data directory is listed when it is
// mounted, so this mounts it again.
static void M_WriteFile(const uint8_t *const bytes, const size_t size)
{
    Th06Data_WriteFile(m_Directory, "data/ecldata1.ecl", bytes, size);
    Files_Shutdown();
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
}

// Sets enemy up as Enemy_Spawn would leave it, at frame 0 of sub.
static void M_InitEnemy(ENEMY *const enemy, const int16_t sub)
{
    memset(enemy, 0, sizeof(*enemy));
    Anm_CreateVm(&enemy->vm);
    for (int32_t i = 0; i < ENEMY_SPRITE_COUNT; i++) {
        Anm_CreateVm(&enemy->vms[i]);
    }
    enemy->script.death_sub = -1;
    enemy->script.pending_interrupt = -1;
    for (int32_t i = 0; i < ENEMY_INTERRUPT_COUNT; i++) {
        enemy->script.interrupts[i] = -1;
    }
    enemy->script.state.frame_func = ECL_FUNC_NONE;
    enemy->turns.left = -1;
    enemy->life = 100;
    Ecl_StartSub(&enemy->script.state, sub);
}

// Builds the file, loads it and starts m_Enemy at subroutine 0.
static void M_Load(void)
{
    static uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    const size_t size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    M_WriteFile(bytes, size);
    CHECK(Result_Log(Ecl_Load(), LOG_LEVEL_ERROR));
    M_InitEnemy(&m_Enemy, 0);
}

static ECL_RUN M_Frame(void)
{
    return Ecl_RunEnemy(&m_Enemy);
}

static ECL_STATE *M_State(void)
{
    return &m_Enemy.script.state;
}

static BULLET *M_AddBullet(
    const int32_t index, const BULLET_STATE state, const ANM_SPRITE *sprite,
    const VEC3 pos, const float speed)
{
    BULLET *const bullet = &g_Bullets.bullets[index];
    bullet->state = state;
    bullet->sprites.flying.sprite_data = sprite;
    bullet->sprites.flying.base_sprite = 0;
    bullet->pos = pos;
    bullet->speed = speed;
    Timer_Set(&bullet->timer, 50);
    return bullet;
}

// Returns true when loading bytes fails.
static bool M_LoadFails(const uint8_t *const bytes, const size_t size)
{
    M_WriteFile(bytes, size);
    return !Result_Log(Ecl_Load(), LOG_LEVEL_INFO);
}

static void M_PutU32(uint8_t *const out, const uint32_t value)
{
    out[0] = (uint8_t)(value & 0xff);
    out[1] = (uint8_t)((value >> 8) & 0xff);
    out[2] = (uint8_t)((value >> 16) & 0xff);
    out[3] = (uint8_t)((value >> 24) & 0xff);
}

// Loading

TEST(loading_parses_the_timeline)
{
    M_Begin("timeline");
    M_INSTR(0, 4, M_IntVar(0), 1);
    EclBuilder_Entry(
        &m_Builder, 10, 0, ECL_TIMELINE_SPAWN, 28,
        (const uint32_t[]) { M_F(1.5f), M_F(-990.0f), M_F(2.0f), 0x00030064,
                             1000 });
    EclBuilder_Entry(
        &m_Builder, 20, 0, ECL_TIMELINE_INTERRUPT_BOSS, 16,
        (const uint32_t[]) { 2, 5, 0, 0, 0 });
    EclBuilder_Entry(
        &m_Builder, 30, 4, ECL_TIMELINE_START_MSG, 8,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    M_Load();
    const ECL_TIMELINE_ENTRY *const timeline = Ecl_GetTimeline();
    CHECK_EQ_INT(timeline[0].time, 10);
    CHECK_EQ_INT(timeline[0].opcode, ECL_TIMELINE_SPAWN);
    CHECK_EQ_INT(timeline[0].arg, 0);
    CHECK(timeline[0].args.spawn.pos.x == 1.5f);
    CHECK(timeline[0].args.spawn.pos.y == -990.0f);
    CHECK(timeline[0].args.spawn.pos.z == 2.0f);
    CHECK_EQ_INT(timeline[0].args.spawn.life, 100);
    CHECK_EQ_INT(timeline[0].args.spawn.item, 3);
    CHECK_EQ_INT(timeline[0].args.spawn.score, 1000);
    CHECK_EQ_INT(timeline[1].args.interrupt.boss, 2);
    CHECK_EQ_INT(timeline[1].args.interrupt.interrupt, 5);
    CHECK(timeline[1].args.spawn.pos.x == M_Bits(2));
    CHECK_EQ_INT(timeline[2].opcode, ECL_TIMELINE_START_MSG);
    CHECK_EQ_INT(timeline[2].arg, 4);
    CHECK(timeline[3].time < 0);
    M_End();
}

TEST(timeline_entries_hold_only_the_arguments_of_their_size)
{
    M_Begin("timeline_sizes");
    M_INSTR(0, 4, M_IntVar(0), 1);
    EclBuilder_Entry(
        &m_Builder, 5, 7, ECL_TIMELINE_WAIT_MSG, 8,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    EclBuilder_Entry(
        &m_Builder, 6, 0, ECL_TIMELINE_INTERRUPT_BOSS, 16,
        (const uint32_t[]) { 1, 3, 0, 0, 0 });
    EclBuilder_Entry(
        &m_Builder, 7, 0, ECL_TIMELINE_SPAWN_MIRRORED, 28,
        (const uint32_t[]) { M_F(4.0f), M_F(5.0f), M_F(6.0f), 0x00020009,
                             300 });
    EclBuilder_Entry(
        &m_Builder, 8, 2, ECL_TIMELINE_WAIT_BOSS, 8,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    size_t timeline;
    const size_t size = EclBuilder_Build(&m_Builder, bytes, &timeline);
    CHECK_EQ_INT(size, timeline + 8 + 16 + 28 + 8 + 4);
    CHECK(bytes[size - 4] == 0xff && bytes[size - 3] == 0xff);
    CHECK(bytes[size - 2] == 4 && bytes[size - 1] == 0);
    M_WriteFile(bytes, size);
    CHECK(Result_Log(Ecl_Load(), LOG_LEVEL_ERROR));
    const ECL_TIMELINE_ENTRY *const entries = Ecl_GetTimeline();
    // The bytes after an 8-byte entry belong to the next one.
    CHECK_EQ_INT(entries[0].opcode, ECL_TIMELINE_WAIT_MSG);
    CHECK_EQ_INT(entries[0].arg, 7);
    CHECK_EQ_INT(entries[0].args.interrupt.boss, 0);
    CHECK_EQ_INT(entries[0].args.spawn.score, 0);
    CHECK_EQ_INT(entries[1].args.interrupt.boss, 1);
    CHECK_EQ_INT(entries[1].args.interrupt.interrupt, 3);
    CHECK_EQ_INT(entries[1].args.spawn.life, 0);
    CHECK_EQ_INT(entries[1].args.spawn.score, 0);
    CHECK(entries[2].args.spawn.pos.z == 6.0f);
    CHECK_EQ_INT(entries[2].args.spawn.life, 9);
    CHECK_EQ_INT(entries[2].args.spawn.item, 2);
    CHECK_EQ_INT(entries[2].args.spawn.score, 300);
    CHECK_EQ_INT(entries[3].time, 8);
    CHECK_EQ_INT(entries[3].arg, 2);
    CHECK(entries[4].time < 0);
    // The terminator needs only its time in the file.
    M_WriteFile(bytes, size - 2);
    CHECK(Result_Log(Ecl_Load(), LOG_LEVEL_ERROR));
    CHECK(Ecl_GetTimeline()[4].time < 0);
    CHECK(M_LoadFails(bytes, size - 3));
    M_End();
}

TEST(loading_reads_the_file_of_the_stage)
{
    M_Begin("stage_file");
    M_INSTR(0, 4, M_IntVar(0), 1);
    uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    const size_t size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    Th06Data_WriteFile(m_Directory, "data/ecldata3.ecl", bytes, size);
    Files_Shutdown();
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    g_GameManager.run.stage = 3;
    CHECK(Result_Log(Ecl_Load(), LOG_LEVEL_ERROR));
    g_GameManager.run.stage = 2;
    CHECK(!Result_Log(Ecl_Load(), LOG_LEVEL_INFO));
    M_End();
}

TEST(loading_converts_the_spell_card_name_to_utf8)
{
    M_Begin("spell_name");
    // テスト in Shift-JIS, then the end of the name.
    const uint8_t args[] = {
        5, 0, 3, 0, 0x83, 0x65, 0x83, 0x58, 0x83, 0x67, 0, 0,
    };
    EclBuilder_InstrBytes(&m_Builder, m_Sub, 0, 93, M_ALL, args, sizeof(args));
    M_Load();
    M_Frame();
    CHECK_EQ_INT(m_NameCalls, 1);
    CHECK_EQ_INT(m_NameSprite, 5);
    CHECK_EQ_STR(m_Name, "テスト");
    M_End();
}

TEST(loading_fails_on_a_subroutine_table_past_the_end)
{
    M_Begin("bad_table");
    uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    size_t size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    bytes[0] = 0x00;
    bytes[1] = 0x40;
    CHECK(M_LoadFails(bytes, size));
    size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    M_PutU32(&bytes[16], 0x7fff0000u);
    CHECK(M_LoadFails(bytes, size));
    M_End();
}

TEST(loading_fails_on_an_instruction_past_the_end)
{
    M_Begin("bad_instr");
    const int32_t offset = M_INSTR(0, 4, M_IntVar(0), 1);
    uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    size_t size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    const size_t start = 16 + 4 + (size_t)offset;
    bytes[start + 6] = 0x00;
    bytes[start + 7] = 0x70;
    CHECK(M_LoadFails(bytes, size));
    size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    bytes[start + 6] = 8;
    bytes[start + 7] = 0;
    CHECK(M_LoadFails(bytes, size));
    // The subroutine has no end before the file does.
    size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    CHECK(M_LoadFails(bytes, start + 20));
    M_End();
}

TEST(loading_fails_on_a_jump_into_an_instruction)
{
    M_Begin("bad_jump");
    const int32_t jump = M_INSTR(0, 2, 0, 0);
    const int32_t target = M_INSTR(0, 4, M_IntVar(0), 1);
    EclBuilder_SetArg(&m_Builder, m_Sub, jump, 1, M_U(target - jump + 4));
    uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    const size_t size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    CHECK(M_LoadFails(bytes, size));
    M_End();
}

TEST(loading_fails_on_a_missing_subroutine)
{
    static const int16_t opcodes[] = { 35, 37, 42, 95, 108, 109, 114, 116 };
    M_Begin("bad_sub");
    uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    for (size_t i = 0; i < ARRAY_SIZE(opcodes); i++) {
        EclBuilder_Init(&m_Builder);
        m_Sub = EclBuilder_BeginSub(&m_Builder);
        M_INSTR(0, opcodes[i], 1, 0, 0, 0, 0, 0);
        const size_t size = EclBuilder_Build(&m_Builder, bytes, nullptr);
        CHECK(M_LoadFails(bytes, size));
    }
    // A negative subroutine is none for the callbacks.
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 108, M_U(-1));
    M_INSTR(0, 116, M_U(-1));
    M_Load();
    M_End();
}

TEST(loading_fails_on_a_bad_timeline)
{
    M_Begin("bad_timeline");
    uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    size_t timeline;
    size_t size = EclBuilder_Build(&m_Builder, bytes, &timeline);
    M_PutU32(&bytes[4], (uint32_t)size - 1);
    CHECK(M_LoadFails(bytes, size));
    EclBuilder_Entry(
        &m_Builder, 0, 3, ECL_TIMELINE_SPAWN_RANDOM, 28,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    CHECK(M_LoadFails(bytes, size));
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    EclBuilder_Entry(
        &m_Builder, 0, 0, ECL_TIMELINE_SPAWN, 0,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    size = EclBuilder_Build(&m_Builder, bytes, &timeline);
    CHECK(M_LoadFails(bytes, size));
    // An entry below the size of its header. Its arg, read as the time of
    // a next entry, would end the timeline.
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    EclBuilder_Entry(
        &m_Builder, 0, -1, ECL_TIMELINE_WAIT_MSG, 2,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    EclBuilder_Entry(
        &m_Builder, 0, 0, ECL_TIMELINE_WAIT_MSG, 28,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    size = EclBuilder_Build(&m_Builder, bytes, &timeline);
    CHECK(M_LoadFails(bytes, size));
    // The header fits, but the size runs past the file.
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    EclBuilder_Entry(
        &m_Builder, 0, 0, ECL_TIMELINE_INTERRUPT_BOSS, 16,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    size = EclBuilder_Build(&m_Builder, bytes, &timeline);
    CHECK(M_LoadFails(bytes, timeline + 12));
    CHECK(M_LoadFails(bytes, timeline + 6));
    // The entry runs past the file.
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    EclBuilder_Entry(
        &m_Builder, 0, 0, ECL_TIMELINE_SPAWN, 28,
        (const uint32_t[]) { 0, 0, 0, 0, 0 });
    size = EclBuilder_Build(&m_Builder, bytes, &timeline);
    CHECK(M_LoadFails(bytes, timeline + 20));
    M_End();
}

TEST(loading_fails_on_a_bad_spell_card_name)
{
    M_Begin("bad_name");
    uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    const uint8_t unended[] = { 0, 0, 0, 0, 'a', 'b', 'c', 'd' };
    EclBuilder_InstrBytes(
        &m_Builder, m_Sub, 0, 93, M_ALL, unended, sizeof(unended));
    size_t size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    CHECK(M_LoadFails(bytes, size));
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    const uint8_t invalid[] = { 0, 0, 0, 0, 0x81, 0x20, 0, 0 };
    EclBuilder_InstrBytes(
        &m_Builder, m_Sub, 0, 93, M_ALL, invalid, sizeof(invalid));
    size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    CHECK(M_LoadFails(bytes, size));
    // A failed load leaves nothing loaded, and the next load works.
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 4, M_IntVar(0), 7);
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[0], 7);
    M_End();
}

// Timing and control flow

TEST(an_instruction_runs_on_its_frame)
{
    M_Begin("frame");
    M_INSTR(0, 4, M_IntVar(0), 5);
    M_INSTR(2, 4, M_IntVar(1), 7);
    M_Load();
    CHECK_EQ_INT(M_Frame(), ECL_RUN_CONTINUE);
    CHECK_EQ_INT(M_State()->int_vars[0], 5);
    CHECK_EQ_INT(M_State()->int_vars[1], 0);
    CHECK_EQ_INT(M_State()->pc, 1);
    CHECK_EQ_INT(M_State()->timer.current, 1);
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[1], 0);
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[1], 7);
    CHECK_EQ_INT(M_State()->pc, 2);
    M_End();
}

TEST(the_difficulty_mask_skips_an_instruction)
{
    M_Begin("mask");
    EclBuilder_Instr(&m_Builder, m_Sub, 0, 4, 0x02, M_WORDS(M_IntVar(0), 1));
    EclBuilder_Instr(&m_Builder, m_Sub, 0, 4, 0x01, M_WORDS(M_IntVar(1), 1));
    EclBuilder_Instr(&m_Builder, m_Sub, 0, 4, 0xfd, M_WORDS(M_IntVar(2), 1));
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[0], 1);
    CHECK_EQ_INT(M_State()->int_vars[1], 0);
    CHECK_EQ_INT(M_State()->int_vars[2], 0);
    M_End();
}

TEST(delete_returns_before_the_frame_ends)
{
    M_Begin("delete");
    M_INSTR(0, 4, M_IntVar(0), 1);
    M_Instr0(0, 1);
    M_INSTR(0, 4, M_IntVar(1), 1);
    M_Load();
    CHECK_EQ_INT(M_Frame(), ECL_RUN_DELETE);
    CHECK_EQ_INT(M_State()->int_vars[0], 1);
    CHECK_EQ_INT(M_State()->int_vars[1], 0);
    CHECK_EQ_INT(M_State()->pc, 0);
    CHECK_EQ_INT(M_State()->timer.current, 0);
    M_End();
}

TEST(a_jump_sets_the_time_and_runs_on_in_the_same_frame)
{
    M_Begin("jump");
    const int32_t jump = M_INSTR(0, 2, 5, 0);
    M_INSTR(0, 4, M_IntVar(0), 1);
    const int32_t target = M_INSTR(5, 4, M_IntVar(1), 1);
    M_Target(jump, target);
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[0], 0);
    CHECK_EQ_INT(M_State()->int_vars[1], 1);
    CHECK_EQ_INT(M_State()->pc, 3);
    CHECK_EQ_INT(M_State()->timer.current, 6);
    M_End();
}

TEST(a_loop_counts_down_its_counter)
{
    M_Begin("loop");
    M_INSTR(0, 4, M_IntVar(0), 3);
    const int32_t label = M_INSTR(1, 18, M_IntVar(1));
    const int32_t loop = M_INSTR(1, 3, 1, 0, M_IntVar(0));
    M_Target(loop, label);
    M_INSTR(1, 4, M_IntVar(2), 1);
    M_Load();
    M_Frame();
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[1], 3);
    CHECK_EQ_INT(M_State()->int_vars[0], 0);
    CHECK_EQ_INT(M_State()->int_vars[2], 1);
    M_End();
}

TEST(conditional_jumps_follow_the_comparison)
{
    static const struct {
        int16_t opcode;
        bool jumps[3]; // for less, equal and greater
    } cases[] = {
        { 29, { true, false, false } }, { 30, { true, true, false } },
        { 31, { false, true, false } }, { 32, { false, false, true } },
        { 33, { false, true, true } },  { 34, { true, false, true } },
    };
    M_Begin("cond_jump");
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        if (i > 0) {
            m_Sub = EclBuilder_BeginSub(&m_Builder);
        }
        M_INSTR(0, 27, M_IntVar(0), M_IntVar(1));
        const int32_t jump = M_INSTR(0, cases[i].opcode, 0, 0);
        M_INSTR(0, 4, M_IntVar(2), 1);
        M_Target(jump, M_INSTR(0, 4, M_IntVar(3), 1));
    }
    M_Load();
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        for (int32_t c = 0; c < 3; c++) {
            M_InitEnemy(&m_Enemy, (int16_t)i);
            M_State()->int_vars[0] = c;
            M_State()->int_vars[1] = 1;
            M_Frame();
            CHECK_EQ_INT(M_State()->compare, c - 1);
            CHECK_EQ_INT(M_State()->int_vars[2] == 0, cases[i].jumps[c]);
            CHECK_EQ_INT(M_State()->int_vars[3], 1);
        }
    }
    M_End();
}

TEST(a_float_comparison_with_nan_is_greater)
{
    M_Begin("compare_float");
    M_INSTR(0, 28, M_FloatVarF(0), M_F(1.0f));
    M_Load();
    const float values[] = { 0.5f, 1.0f, 2.0f, M_Bits(0x7fc00000u) };
    const int32_t expected[] = { -1, 0, 1, 1 };
    for (int32_t i = 0; i < 4; i++) {
        M_InitEnemy(&m_Enemy, 0);
        M_State()->float_vars[0] = values[i];
        M_Frame();
        CHECK_EQ_INT(M_State()->compare, expected[i]);
    }
    M_End();
}

TEST(a_call_runs_the_subroutine_and_a_return_resumes)
{
    M_Begin("call");
    M_INSTR(0, 35, 1, 42, M_F(2.5f));
    M_INSTR(0, 4, M_IntVar(3), 9);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 4, M_IntVar(1), M_IntVar(0));
    M_Instr0(1, 36);
    M_Load();
    M_State()->int_vars[0] = 7;
    M_Frame();
    CHECK_EQ_INT(M_State()->sub, 1);
    CHECK_EQ_INT(M_State()->int_vars[0], 42);
    CHECK(M_State()->float_vars[0] == 2.5f);
    CHECK_EQ_INT(M_State()->int_vars[1], 42);
    CHECK_EQ_INT(m_Enemy.script.depth, 1);
    CHECK_EQ_INT(m_Enemy.script.stack[0].sub, 0);
    CHECK_EQ_INT(m_Enemy.script.stack[0].pc, 1);
    CHECK_EQ_INT(m_Enemy.script.stack[0].timer.current, 0);
    M_Frame();
    CHECK_EQ_INT(M_State()->sub, 0);
    CHECK_EQ_INT(m_Enemy.script.depth, 0);
    CHECK_EQ_INT(M_State()->int_vars[0], 7);
    CHECK_EQ_INT(M_State()->int_vars[1], 0);
    CHECK_EQ_INT(M_State()->int_vars[3], 9);
    CHECK_EQ_INT(M_State()->timer.current, 1);
    M_End();
}

TEST(calls_stop_counting_at_depth_seven)
{
    M_Begin("depth");
    for (int32_t i = 0; i < 9; i++) {
        if (i > 0) {
            m_Sub = EclBuilder_BeginSub(&m_Builder);
        }
        M_INSTR(0, 35, M_U(i + 1), M_U(i), 0);
    }
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(5, 4, M_IntVar(0), 1);
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_State()->sub, 9);
    CHECK_EQ_INT(m_Enemy.script.depth, 7);
    CHECK_EQ_INT(m_Enemy.script.stack[6].sub, 6);
    CHECK_EQ_INT(m_Enemy.script.stack[7].sub, 8);
    CHECK_EQ_INT(m_Enemy.script.stack[7].int_vars[0], 7);
    M_End();
}

TEST(without_a_call_stack_a_return_still_pops)
{
    M_Begin("no_stack");
    M_INSTR(0, 35, 1, 0, 0);
    M_INSTR(0, 4, M_IntVar(0), 1);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 130, 1);
    M_INSTR(0, 35, 2, 0, 0);
    M_INSTR(0, 4, M_IntVar(1), 1);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Instr0(0, 36);
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_State()->sub, 0);
    CHECK_EQ_INT(m_Enemy.script.depth, 0);
    CHECK_EQ_INT(M_State()->int_vars[0], 1);
    CHECK_EQ_INT(M_State()->int_vars[1], 0);
    M_End();
}

TEST(an_interrupt_counts_its_depth_without_a_call_stack)
{
    M_Begin("interrupt");
    M_INSTR(0, 130, 1);
    M_INSTR(0, 109, 1, 2);
    M_INSTR(0, 110, 2);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(5, 4, M_IntVar(0), 1);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 109, 1, 2);
    M_INSTR(0, 110, 2);
    M_Load();
    m_Enemy.script.stack[0].pc = 99;
    M_Frame();
    CHECK_EQ_INT(M_State()->sub, 1);
    CHECK_EQ_INT(m_Enemy.script.depth, 1);
    CHECK_EQ_INT(m_Enemy.script.pending_interrupt, -1);
    CHECK_EQ_INT(m_Enemy.script.stack[0].pc, 99);
    M_InitEnemy(&m_Enemy, 2);
    M_Frame();
    CHECK_EQ_INT(M_State()->sub, 1);
    CHECK_EQ_INT(m_Enemy.script.depth, 1);
    CHECK_EQ_INT(m_Enemy.script.stack[0].sub, 2);
    CHECK_EQ_INT(m_Enemy.script.stack[0].pc, 2);
    M_End();
}

TEST(a_pending_interrupt_skips_the_instruction_due_next)
{
    M_Begin("pending");
    M_INSTR(0, 109, 1, 0);
    M_INSTR(1, 4, M_IntVar(0), 1);
    M_INSTR(1, 4, M_IntVar(1), 1);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Instr0(0, 36);
    M_Load();
    M_Frame();
    m_Enemy.script.pending_interrupt = 0;
    M_Frame();
    CHECK_EQ_INT(M_State()->sub, 0);
    CHECK_EQ_INT(m_Enemy.script.pending_interrupt, -1);
    CHECK_EQ_INT(M_State()->int_vars[0], 0);
    CHECK_EQ_INT(M_State()->int_vars[1], 1);
    M_End();
}

TEST(conditional_calls_compare_a_value)
{
    static const struct {
        int16_t opcode;
        bool calls[3]; // for less, equal and greater
    } cases[] = {
        { 37, { true, false, false } }, { 38, { true, true, false } },
        { 39, { false, true, false } }, { 40, { false, false, true } },
        { 41, { false, true, true } },  { 42, { true, false, true } },
    };
    M_Begin("cond_call");
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        if (i > 0) {
            m_Sub = EclBuilder_BeginSub(&m_Builder);
        }
        M_INSTR(0, cases[i].opcode, 6, 3, M_F(1.5f), M_IntVar(0), 5);
    }
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(5, 4, M_IntVar(1), 1);
    M_Load();
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        for (int32_t c = 0; c < 3; c++) {
            M_InitEnemy(&m_Enemy, (int16_t)i);
            M_State()->int_vars[0] = 4 + c;
            M_Frame();
            CHECK_EQ_INT(M_State()->sub == 6, cases[i].calls[c]);
            if (cases[i].calls[c]) {
                CHECK_EQ_INT(M_State()->int_vars[0], 3);
                CHECK(M_State()->float_vars[0] == 1.5f);
            }
        }
    }
    M_End();
}

// Variables and operands

TEST(set_writes_variables_only)
{
    M_Begin("set");
    M_INSTR(0, 4, M_IntVar(0), 7);
    M_INSTR(0, 4, M_IntVar(1), M_IntVar(0));
    M_INSTR(0, 5, M_FloatVar(0), M_F(2.5f));
    M_INSTR(0, 4, M_U(-10013), 3);
    M_INSTR(0, 4, 42, 3);
    // The source of opcode 5 is an int operand, so a float operand that
    // names a variable is copied as it is.
    M_INSTR(0, 5, M_FloatVar(1), M_FloatVarF(0));
    M_INSTR(0, 5, M_FloatVar(2), M_FloatVar(0));
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[0], 7);
    CHECK_EQ_INT(M_State()->int_vars[1], 7);
    CHECK(M_State()->float_vars[0] == 2.5f);
    CHECK(M_State()->float_vars[1] == -10005.0f);
    CHECK(M_State()->float_vars[2] == 2.5f);
    CHECK_EQ_INT(g_GameManager.run.difficulty, 1);
    M_End();
}

TEST(random_values_follow_the_generator)
{
    M_Begin("random");
    M_INSTR(0, 6, M_IntVar(0), 10);
    M_INSTR(0, 7, M_IntVar(1), 10, 100);
    M_INSTR(0, 8, M_FloatVar(0), M_F(2.0f));
    M_INSTR(0, 9, M_FloatVar(1), M_F(2.0f), M_F(5.0f));
    M_INSTR(0, 6, M_IntVar(2), 0);
    M_Load();
    RNG expected = g_Rng;
    M_State()->int_vars[2] = 9;
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[0], Rng_U32InRange(&expected, 10));
    CHECK_EQ_INT(M_State()->int_vars[1], Rng_U32InRange(&expected, 10) + 100);
    CHECK(M_State()->float_vars[0] == Rng_F32InRange(&expected, 2.0f));
    CHECK(M_State()->float_vars[1] == Rng_F32InRange(&expected, 2.0f) + 5.0f);
    CHECK_EQ_INT(M_State()->int_vars[2], 0);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_End();
}

TEST(the_position_copies_its_bits_into_any_variable)
{
    M_Begin("get_pos");
    M_INSTR(0, 10, M_FloatVar(0));
    M_INSTR(0, 11, M_FloatVar(1));
    M_INSTR(0, 12, M_IntVar(0));
    M_Load();
    m_Enemy.move.pos = (VEC3) { 1.5f, 2.5f, 3.5f };
    M_Frame();
    CHECK(M_State()->float_vars[0] == 1.5f);
    CHECK(M_State()->float_vars[1] == 2.5f);
    CHECK_EQ_INT((uint32_t)M_State()->int_vars[0], M_FloatBits(3.5f));
    M_End();
}

TEST(int_arithmetic_wraps_and_truncates)
{
    M_Begin("int_math");
    M_INSTR(0, 13, M_IntVar(0), 0x7fffffff, 1);
    M_INSTR(0, 14, M_IntVar(1), 5, 7);
    M_INSTR(0, 15, M_IntVar(2), 3, M_U(-4));
    M_INSTR(0, 16, M_IntVar(3), M_U(-7), 2);
    M_INSTR(0, 17, M_IntVar(4), M_U(-7), 2);
    M_INSTR(0, 15, M_IntVar(5), 0x10000, 0x10000);
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[0], INT32_MIN);
    CHECK_EQ_INT(M_State()->int_vars[1], -2);
    CHECK_EQ_INT(M_State()->int_vars[2], -12);
    CHECK_EQ_INT(M_State()->int_vars[3], -3);
    CHECK_EQ_INT(M_State()->int_vars[4], -1);
    CHECK_EQ_INT(M_State()->int_vars[5], 0);
    M_End();
}

TEST(the_destination_decides_between_int_and_float)
{
    M_Begin("float_math");
    M_INSTR(0, 13, M_FloatVar(0), M_F(1.5f), M_F(2.25f));
    M_INSTR(0, 20, M_IntVar(0), 2, 3);
    M_INSTR(0, 21, M_FloatVar(1), M_F(1.5f), M_F(4.0f));
    M_INSTR(0, 22, M_FloatVar(2), M_F(1.5f), M_F(4.0f));
    M_INSTR(0, 23, M_FloatVar(3), M_F(1.0f), M_F(3.0f));
    M_INSTR(0, 24, M_FloatVar(3), M_FloatVarF(3), M_F(0.125f));
    M_INSTR(0, 20, M_U(-10014), 2, 3);
    M_Load();
    g_GameManager.rank.value = 16;
    M_Frame();
    CHECK(M_State()->float_vars[0] == 3.75f);
    CHECK_EQ_INT(M_State()->int_vars[0], 5);
    CHECK(M_State()->float_vars[1] == -2.5f);
    CHECK(M_State()->float_vars[2] == 6.0f);
    CHECK(M_State()->float_vars[3] == Math_Fmod(1.0f / 3.0f, 0.125f));
    CHECK_EQ_INT(g_GameManager.rank.value, 16);
    M_End();
}

TEST(increment_writes_the_int_bits_of_any_operand)
{
    M_Begin("increment");
    M_INSTR(0, 18, M_FloatVar(0));
    M_INSTR(0, 19, M_U(-10014));
    const int32_t decrement = M_INSTR(0, 19, M_U(-10000));
    // The second time, the argument that the first time changed names
    // int_vars[0].
    M_Target(M_INSTR(1, 2, 0, 0), decrement);
    M_Load();
    M_State()->float_vars[0] = 1.0f;
    g_GameManager.rank.value = 16;
    M_Frame();
    CHECK_EQ_INT(M_FloatBits(M_State()->float_vars[0]), M_FloatBits(1.0f) + 1);
    CHECK_EQ_INT(g_GameManager.rank.value, 15);
    CHECK_EQ_INT(M_State()->int_vars[0], 0);
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[0], -1);
    M_End();
}

TEST(angle_and_normalize_write_float_variables)
{
    M_Begin("angle");
    M_INSTR(0, 25, M_FloatVar(0), M_F(1.0f), M_F(2.0f), M_F(4.0f), M_F(3.0f));
    M_INSTR(0, 25, M_IntVar(0), M_F(1.0f), M_F(2.0f), M_F(4.0f), M_F(3.0f));
    M_INSTR(0, 26, M_FloatVar(1));
    M_Load();
    M_State()->float_vars[1] = 4.0f;
    M_Frame();
    CHECK(M_State()->float_vars[0] == X87_Atan2(1.0f, 3.0f));
    CHECK_EQ_INT(M_State()->int_vars[0], 0);
    CHECK(M_State()->float_vars[1] == Angle_AddNormalize(4.0f, 0.0f));
    M_End();
}

TEST(operands_read_the_game_state)
{
    M_Begin("operands");
    M_INSTR(0, 4, M_IntVar(0), M_U(-10013));
    M_INSTR(0, 4, M_IntVar(1), M_U(-10014));
    M_INSTR(0, 4, M_IntVar(2), M_U(-10022));
    M_INSTR(0, 4, M_IntVar(3), M_U(-10024));
    M_INSTR(0, 4, M_IntVar(4), M_U(-10025));
    M_INSTR(0, 4, M_U(-10012), 9);
    M_INSTR(0, 5, M_FloatVar(0), M_U(-10018));
    M_INSTR(0, 5, M_FloatVar(1), M_U(-10021));
    M_INSTR(0, 5, M_FloatVar(2), M_U(-10023));
    M_INSTR(0, 5, M_FloatVar(3), M_U(-10016));
    M_INSTR(0, 4, M_IntVar(5), M_U(-10020));
    M_INSTR(0, 4, M_IntVar(6), M_U(-10017));
    M_Load();
    g_GameManager.rank.value = 16;
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 1;
    g_Player.pos = (VEC3) { 13.0f, 14.0f, 0.5f };
    m_Enemy.move.pos = (VEC3) { 10.0f, 10.0f, 2.0f };
    m_Enemy.callbacks.timer.current = 33;
    m_Enemy.life = 77;
    m_AimAngle = 0.75f;
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[0], 1);
    CHECK_EQ_INT(M_State()->int_vars[1], 16);
    CHECK_EQ_INT(M_State()->int_vars[2], 33);
    CHECK_EQ_INT(M_State()->int_vars[3], 77);
    CHECK_EQ_INT(M_State()->int_vars[4], 3);
    CHECK_EQ_INT(M_State()->int_vars[7], 9);
    CHECK(M_State()->float_vars[0] == 13.0f);
    CHECK(M_State()->float_vars[1] == 0.75f);
    CHECK(m_AngleFrom.x == 10.0f && m_AngleFrom.y == 10.0f);
    CHECK(
        M_State()->float_vars[2]
        == X87_Sqrt(3.0f * 3.0f + 4.0f * 4.0f + -1.5f * -1.5f));
    CHECK(M_State()->float_vars[3] == 10.0f);
    CHECK_EQ_INT((uint32_t)M_State()->int_vars[5], M_FloatBits(0.5f));
    CHECK_EQ_INT((uint32_t)M_State()->int_vars[6], M_FloatBits(2.0f));
    M_End();
}

TEST(a_float_operand_truncates_to_a_variable)
{
    M_Begin("float_operand");
    M_INSTR(0, 45, M_F(-10005.9f), M_F(-10001.0f));
    M_Load();
    M_State()->float_vars[0] = 1.25f;
    M_State()->int_vars[0] = (int32_t)M_FloatBits(3.0f);
    M_Frame();
    CHECK(m_Enemy.move.angle == 1.25f);
    CHECK(m_Enemy.move.speed == 3.0f);
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_ANGLE);
    M_End();
}

TEST(add_time_moves_the_frame_of_the_script)
{
    M_Begin("add_time");
    M_INSTR(0, 123, M_IntVar(0));
    M_INSTR(6, 4, M_IntVar(1), 1);
    M_Load();
    M_State()->int_vars[0] = 5;
    M_Frame();
    CHECK_EQ_INT(M_State()->timer.current, 6);
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[1], 1);
    M_End();
}

// Movement

TEST(set_pos_resolves_each_component_in_place)
{
    M_Begin("set_pos");
    M_INSTR(0, 43, M_F(-10016.0f), M_F(7.0f), M_F(-10015.0f));
    M_Load();
    m_Enemy.move.pos = (VEC3) { 1.0f, 2.0f, 3.0f };
    M_Frame();
    CHECK(m_Enemy.move.pos.x == 7.0f);
    CHECK(m_Enemy.move.pos.y == 7.0f);
    CHECK(m_Enemy.move.pos.z == 7.0f);
    CHECK_EQ_INT(m_ClampCalls, 1);
    CHECK_EQ_INT(m_PresentationRenewals, 1);
    M_End();
}

TEST(an_angle_move_turns_and_speeds_up_each_frame)
{
    M_Begin("angle_move");
    M_INSTR(0, 44, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_INSTR(1, 45, M_F(0.5f), M_F(2.0f));
    M_INSTR(1, 46, M_F(0.1f));
    M_INSTR(1, 48, M_F(0.25f));
    M_INSTR(2, 47, M_F(4.0f));
    M_Load();
    M_Frame();
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_VELOCITY);
    CHECK(m_Enemy.move.velocity.x == 1.0f && m_Enemy.move.velocity.z == 3.0f);
    M_Frame();
    CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_ANGLE);
    const float angle = Angle_AddNormalize(0.5f, 0.1f);
    const float speed = 0.25f + 2.0f;
    float x;
    float y;
    X87_SinCosScale(angle, speed, &x, &y);
    CHECK(m_Enemy.move.angle == angle);
    CHECK(m_Enemy.move.speed == speed);
    CHECK(m_Enemy.move.velocity.x == x && m_Enemy.move.velocity.y == y);
    CHECK(m_Enemy.move.velocity.z == 0.0f);
    M_Frame();
    CHECK(m_Enemy.move.speed == 0.25f + 4.0f);
    M_End();
}

TEST(aiming_at_the_player_adds_an_unresolved_offset)
{
    M_Begin("aim");
    M_INSTR(0, 51, M_F(-10005.0f), M_FloatVarF(0));
    M_Load();
    m_AimAngle = 0.5f;
    M_State()->float_vars[0] = 3.0f;
    M_Frame();
    CHECK(m_Enemy.move.angle == Angle_AddNormalize(0.5f + -10005.0f, 0.0f));
    CHECK(m_Enemy.move.speed == 3.0f);
    M_End();
}

TEST(a_move_to_eases_to_its_target)
{
    M_Begin("move_to");
    for (int32_t i = 0; i < 5; i++) {
        if (i > 0) {
            m_Sub = EclBuilder_BeginSub(&m_Builder);
        }
        M_INSTR(0, (int16_t)(56 + i), 4, M_F(100.3f), M_F(50.0f), M_F(0.0f));
    }
    M_Load();
    const float t = 3.0f / 4.0f;
    const float dx = 100.3f - 20.0f;
    const float eased[5] = {
        1.0f - t,
        1.0f - t * t,
        1.0f - t * t * t * t,
        (1.0f - t) * (1.0f - t),
        (1.0f - t) * (1.0f - t) * (1.0f - t) * (1.0f - t),
    };
    for (int32_t i = 0; i < 5; i++) {
        M_InitEnemy(&m_Enemy, (int16_t)i);
        m_Enemy.move.pos = (VEC3) { 20.0f, 10.0f, 0.0f };
        M_Frame();
        CHECK_EQ_INT(m_Enemy.flags.ease, i);
        CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_TO);
        const float vx = eased[i] * dx + 20.0f - 20.0f;
        const float vy = eased[i] * 40.0f + 10.0f - 10.0f;
        CHECK(m_Enemy.move.velocity.x == vx);
        CHECK(m_Enemy.move.velocity.y == vy);
        CHECK(m_Enemy.move.angle == X87_Atan2(vy, vx));
        // The enemy module moves the enemy in between.
        m_Enemy.move.pos = (VEC3) { 21.1f, 10.3f, 0.0f };
        M_Frame();
        const float t2 = 2.0f / 4.0f;
        const float eased2[5] = {
            1.0f - t2,
            1.0f - t2 * t2,
            1.0f - t2 * t2 * t2 * t2,
            (1.0f - t2) * (1.0f - t2),
            (1.0f - t2) * (1.0f - t2) * (1.0f - t2) * (1.0f - t2),
        };
        CHECK(m_Enemy.move.velocity.x == eased2[i] * dx + 20.0f - 21.1f);
        CHECK(m_Enemy.move.velocity.y == eased2[i] * 40.0f + 10.0f - 10.3f);
        for (int32_t f = 0; f < 2; f++) {
            M_Frame();
        }
        CHECK_EQ_INT(m_Enemy.flags.move, ENEMY_MOVE_VELOCITY);
        CHECK(m_Enemy.move.pos.x == 20.0f + dx && m_Enemy.move.pos.y == 50.0f);
        CHECK(m_Enemy.move.velocity.x == 0.0f);
    }
    M_End();
}

TEST(angle_and_time_moves_set_their_distance)
{
    M_Begin("move_angle");
    M_INSTR(0, 53, 5, M_FloatVarF(0), M_F(1.7f));
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 64, 8);
    M_Load();
    M_State()->float_vars[0] = 0.5f;
    m_Enemy.move.pos = (VEC3) { 5.0f, 6.0f, 7.0f };
    M_Frame();
    CHECK_EQ_INT(m_Enemy.flags.ease, ENEMY_EASE_OUT4);
    CHECK_EQ_INT(m_Enemy.move.duration, 5);
    CHECK(m_Enemy.move.delta.x == X87_Cos(0.5f) * 1.7f * 5.0f / 2.0f);
    CHECK(m_Enemy.move.delta.y == X87_Sin(0.5f) * 1.7f * 5.0f / 2.0f);
    CHECK(m_Enemy.move.delta.z == 0.0f);
    CHECK(m_Enemy.move.start.z == 7.0f);
    CHECK_EQ_INT(m_Enemy.move.timer.current, 4);
    // The angle of the enemy is resolved as a float operand.
    M_InitEnemy(&m_Enemy, 1);
    M_State()->float_vars[0] = 0.75f;
    m_Enemy.move.angle = -10005.0f;
    m_Enemy.move.speed = 2.0f;
    M_Frame();
    CHECK_EQ_INT(m_Enemy.flags.ease, ENEMY_EASE_IN4);
    CHECK(m_Enemy.move.delta.x == X87_Cos(0.75f) * 2.0f * 8.0f / 2.0f);
    CHECK(m_Enemy.move.delta.y == X87_Sin(0.75f) * 2.0f * 8.0f / 2.0f);
    M_End();
}

TEST(a_random_angle_turns_away_from_the_bounds)
{
    M_Begin("random_angle");
    M_INSTR(0, 65, M_F(0.0f), M_F(0.0f), M_F(384.0f), M_F(448.0f));
    M_INSTR(0, 50, M_F(2.0f), M_F(3.0f));
    M_INSTR(1, 49, M_F(-1.0f), M_F(1.0f));
    M_Instr0(1, 66);
    M_Load();
    m_Enemy.move.pos = (VEC3) { 50.0f, 224.0f, 0.0f };
    RNG expected = g_Rng;
    M_Frame();
    CHECK(m_Enemy.flags.clamp_pos);
    CHECK(m_Enemy.move.bounds_max.y == 448.0f);
    CHECK(
        m_Enemy.move.angle == M_PI - (Rng_F32InRange(&expected, 1.0f) + 2.0f));
    M_Frame();
    CHECK(m_Enemy.move.angle == Rng_F32InRange(&expected, 2.0f) + -1.0f);
    CHECK(!m_Enemy.flags.clamp_pos);
    // Near the top, a negative angle turns downwards.
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 65, M_F(0.0f), M_F(0.0f), M_F(384.0f), M_F(448.0f));
    M_INSTR(0, 50, M_F(-2.0f), M_F(-1.0f));
    M_Load();
    m_Enemy.move.pos = (VEC3) { 200.0f, 10.0f, 0.0f };
    expected = g_Rng;
    M_Frame();
    CHECK(m_Enemy.move.angle == -(Rng_F32InRange(&expected, 1.0f) + -2.0f));
    M_End();
}

// Shots

TEST(fire_sets_the_shooter_up_with_the_rank)
{
    M_Begin("fire");
    M_INSTR(0, 131, M_F(-1.3f), M_F(0.9f), 0, 4, 2, M_U(-2));
    M_INSTR(
        0, 68, 0xd8ef0003, 3, 0, M_F(2.0f), M_F(0.1f), M_F(4.0f), M_F(0.2f),
        0x10);
    M_INSTR(0, 81, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_INSTR(
        0, 75, 0x00020001, M_U(-9), 1, M_F(0.0f), M_F(1.0f), M_F(0.0f),
        M_F(0.0f), 0);
    M_Instr0(1, 78);
    M_INSTR(1, 67, 0x00020001, 0, 1, M_F(1.0f), M_F(1.0f), 0, 0, 0);
    M_Instr0(2, 79);
    M_Instr0(2, 80);
    M_Load();
    g_GameManager.rank.value = 7;
    M_State()->int_vars[0] = 7;
    m_Enemy.move.pos = (VEC3) { 100.0f, 50.0f, 0.0f };
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 2);
    const BULLET_SHOOTER *shooter = &m_Fired[0];
    CHECK_EQ_INT(shooter->type, 3);
    CHECK_EQ_INT(shooter->color, 7);
    CHECK_EQ_INT(shooter->aim, BULLET_AIM_FAN);
    CHECK_EQ_INT(shooter->columns, 3);
    CHECK_EQ_INT(shooter->rows, 2);
    const float lerp = (float)7 * (0.9f - -1.3f) / 32.0f + -1.3f;
    CHECK(shooter->speed1 == 2.0f + lerp);
    CHECK(shooter->speed2 == 0.3f);
    CHECK(shooter->angle1 == Angle_AddNormalize(4.0f, 0.0f));
    CHECK(shooter->angle2 == 0.2f);
    CHECK_EQ_INT(shooter->flags, 0x10);
    CHECK(shooter->pos.x == 100.0f && shooter->pos.y == 50.0f);
    shooter = &m_Fired[1];
    CHECK_EQ_INT(shooter->aim, BULLET_AIM_RANDOM);
    CHECK_EQ_INT(shooter->columns, 1);
    CHECK_EQ_INT(shooter->rows, 3);
    CHECK_EQ_INT(shooter->color, 2);
    CHECK(shooter->speed1 == 0.0f);
    CHECK(shooter->speed2 == 1.0f + lerp / 2.0f);
    CHECK(shooter->pos.x == 101.0f && shooter->pos.z == 3.0f);
    M_Frame();
    CHECK(m_Enemy.flags.hold_fire);
    CHECK_EQ_INT(m_FiredCount, 2);
    CHECK_EQ_INT(m_Enemy.shots.shooter.aim, BULLET_AIM_FAN_AT_PLAYER);
    CHECK_EQ_INT(m_Enemy.shots.shooter.columns, 1);
    M_Frame();
    CHECK(!m_Enemy.flags.hold_fire);
    CHECK_EQ_INT(m_FiredCount, 3);
    CHECK_EQ_INT(m_Fired[2].aim, BULLET_AIM_FAN_AT_PLAYER);
    M_End();
}

TEST(timed_shots_fire_every_interval)
{
    M_Begin("interval");
    M_INSTR(0, 76, 20);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 77, 20);
    M_Load();
    for (int32_t i = 0; i < 23; i++) {
        M_Frame();
    }
    CHECK_EQ_INT(m_Enemy.shots.interval, 24);
    CHECK_EQ_INT(m_FiredCount, 0);
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 1);
    CHECK_EQ_INT(m_Enemy.shots.timer.current, 0);
    m_Enemy.life = 0;
    for (int32_t i = 0; i < 20; i++) {
        M_Frame();
    }
    CHECK_EQ_INT(m_FiredCount, 1);
    // The rank shortens the interval, and opcode 77 starts it at random.
    M_InitEnemy(&m_Enemy, 1);
    g_GameManager.rank.value = 32;
    RNG expected = g_Rng;
    M_Frame();
    CHECK_EQ_INT(m_Enemy.shots.interval, 16);
    const int32_t start = (int32_t)Rng_U32InRange(&expected, 16) + 1;
    CHECK_EQ_INT(m_Enemy.shots.timer.current, start == 16 ? 0 : start);
    CHECK_EQ_INT(m_FiredCount, start == 16 ? 2 : 1);
    M_End();
}

TEST(shot_extras_and_sound_set_the_shooter)
{
    M_Begin("extras");
    M_INSTR(
        0, 82, 1, 2, M_IntVar(0), 4, M_F(0.5f), M_FloatVarF(0), M_F(1.5f),
        M_F(2.5f));
    M_INSTR(0, 84, 5);
    M_INSTR(1, 84, M_U(-1));
    M_Load();
    M_State()->int_vars[0] = 30;
    M_State()->float_vars[0] = 9.0f;
    M_Frame();
    const BULLET_SHOOTER *const shooter = &m_Enemy.shots.shooter;
    CHECK_EQ_INT(shooter->ex_ints[2], 30);
    CHECK_EQ_INT(shooter->ex_ints[3], 4);
    CHECK(shooter->ex_floats[1] == 9.0f && shooter->ex_floats[3] == 2.5f);
    CHECK_EQ_INT(shooter->sound, 5);
    CHECK_EQ_INT(shooter->flags, BULLET_FLAG_SOUND);
    M_Frame();
    CHECK_EQ_INT(shooter->flags, 0);
    M_End();
}

TEST(play_sound_queues_its_argument)
{
    M_Begin("play_sound");
    M_INSTR(0, 106, 7);
    M_Load();
    Sound_Reset();
    M_Frame();
    CHECK_EQ_INT(g_SoundEvents.queue[0], 7);
    CHECK_EQ_INT(g_SoundEvents.queue[1], -1);
    M_End();
}

TEST(lasers_start_turn_move_and_end)
{
    M_Begin("lasers");
    M_INSTR(0, 87, M_IntVar(0));
    M_INSTR(
        0, 85, 0x00040002, M_F(0.5f), M_F(3.0f), M_F(1.0f), M_F(2.0f),
        M_FloatVarF(0), M_F(9.0f), 10, 20, 30, 40, 50, 0x20);
    M_INSTR(1, 86, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    M_INSTR(2, 88, 3, M_F(0.25f));
    M_INSTR(3, 89, 3, M_F(0.125f));
    M_INSTR(4, 90, 3, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_INSTR(5, 91, 3);
    M_INSTR(5, 92, 3);
    M_INSTR(6, 91, 4);
    M_Instr0(6, 134);
    M_Load();
    M_State()->int_vars[0] = 3;
    M_State()->float_vars[0] = 7.0f;
    m_Enemy.move.pos = (VEC3) { 10.0f, 20.0f, 0.0f };
    M_Frame();
    CHECK_EQ_INT(m_Enemy.shots.laser_index, 3);
    CHECK_EQ_INT(m_LaserCount, 1);
    const BULLET_SHOOTER *const shooter = &m_LaserShots[0];
    CHECK_EQ_INT(shooter->type, 2);
    CHECK_EQ_INT(shooter->color, 4);
    CHECK(shooter->angle1 == 0.5f && shooter->speed1 == 3.0f);
    CHECK(shooter->ex_floats[2] == 7.0f && shooter->ex_floats[3] == 9.0f);
    CHECK_EQ_INT(shooter->ex_ints[0], 10);
    CHECK_EQ_INT(shooter->ex_ints[3], 40);
    CHECK_EQ_INT(shooter->laser_end_hit_frames, 50);
    CHECK_EQ_INT(shooter->flags, 0x20);
    CHECK_EQ_INT(shooter->aim, BULLET_AIM_FAN);
    CHECK(shooter->pos.x == 10.0f);
    CHECK(m_Enemy.shots.lasers[3] == &m_Lasers[0]);
    M_Frame();
    CHECK_EQ_INT(m_LaserShots[1].aim, BULLET_AIM_FAN_AT_PLAYER);
    LASER *const laser = m_Enemy.shots.lasers[3];
    CHECK(laser == &m_Lasers[1]);
    laser->angle = 1.0f;
    laser->active = true;
    laser->pos = (VEC3) { 4.0f, 5.0f, 0.0f };
    M_Frame();
    CHECK(laser->angle == 1.0f + 0.25f);
    m_AimAngle = 2.0f;
    M_Frame();
    CHECK(laser->angle == 2.0f + 0.125f);
    CHECK(m_AngleFrom.x == 4.0f);
    M_Frame();
    CHECK(laser->pos.x == 11.0f && laser->pos.y == 22.0f);
    Timer_Set(&laser->timer, 30);
    M_Frame();
    CHECK_EQ_INT(M_State()->compare, 0);
    CHECK_EQ_INT(laser->state, LASER_ENDING);
    CHECK_EQ_INT(laser->timer.current, 0);
    M_Frame();
    CHECK_EQ_INT(M_State()->compare, 1);
    CHECK(m_Enemy.shots.lasers[3] == nullptr);
    M_End();
}

// Spell cards

TEST(a_spell_card_starts_and_counts_an_attempt)
{
    M_Begin("spell_start");
    const uint8_t args[] = { 2, 0, 9, 0, 'S', 'p', 'e', 'l', 'l', 0, 0, 0 };
    EclBuilder_InstrBytes(&m_Builder, m_Sub, 0, 93, M_ALL, args, sizeof(args));
    EclBuilder_InstrBytes(&m_Builder, m_Sub, 1, 93, M_ALL, args, sizeof(args));
    M_Load();
    m_Enemy.shots.rank_columns_high = 5;
    M_Frame();
    CHECK_EQ_INT(m_NameCalls, 1);
    CHECK_EQ_INT(m_NameSprite, 2);
    CHECK_EQ_STR(m_Name, "Spell");
    CHECK(g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_RUNNING);
    CHECK_EQ_INT(g_Enemies.spell_card.index, 9);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 250000);
    CHECK_EQ_INT(m_ClearToItems, 1);
    CHECK_EQ_INT(g_Stage.spell_card.state, 1);
    CHECK(m_Enemy.shots.rank_speed_low == -0.5f);
    CHECK(m_Enemy.shots.rank_speed_high == 0.5f);
    CHECK_EQ_INT(m_Enemy.shots.rank_columns_high, 0);
    CHECK_EQ_INT(g_SceneStubCalls.spell_card_attempt, 1);
    CHECK_EQ_INT(g_SceneStubCalls.spell_card_attempt_index, 9);
    CHECK_EQ_STR(g_SceneStubCalls.spell_card_attempt_name, "Spell");
    CHECK_EQ_INT(g_SceneStubCalls.spell_card_attempt_bonus, 250000);
    g_GameManager.mode.in_replay = true;
    M_Frame();
    CHECK_EQ_INT(m_NameCalls, 2);
    CHECK_EQ_INT(g_SceneStubCalls.spell_card_attempt, 1);
    M_End();
}

TEST(a_captured_spell_card_scores_its_bonus)
{
    M_Begin("spell_capture");
    M_Instr0(0, 94);
    M_Load();
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.bonus = 300000;
    g_Enemies.spell_card.index = 20;
    g_Gui.boss.seconds = 25;
    g_GameManager.score.value = 1000;
    g_GameManager.run.character = 1;
    g_Stage.spell_card.state = 2;
    M_Frame();
    CHECK_EQ_INT(m_EndNameCalls, 1);
    CHECK_EQ_INT(m_ClearWithBonus, 1);
    CHECK_EQ_INT(m_ClearMaxBonus, 0x3200);
    CHECK(m_ClearItems);
    CHECK_EQ_INT(m_BonusShown, 300000 + 300000 * 25 / 10);
    CHECK_EQ_INT(g_GameManager.score.value, 1000 + 1050000);
    CHECK_EQ_INT(g_GameManager.stats.spell_cards_captured, 1);
    CHECK_EQ_INT(g_SceneStubCalls.spell_card_capture, 1);
    CHECK_EQ_INT(g_SceneStubCalls.spell_card_capture_index, 20);
    CHECK_EQ_INT(g_SceneStubCalls.spell_card_capture_shot_type, 2);
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_NONE);
    CHECK_EQ_INT(g_Stage.spell_card.state, 0);
    M_End();
}

TEST(an_ended_spell_card_scores_only_when_captured)
{
    M_Begin("spell_end");
    M_Instr0(0, 94);
    M_Load();
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.capturable = false;
    g_GameManager.mode.in_replay = true;
    M_Frame();
    CHECK_EQ_INT(m_ClearWithBonus, 1);
    CHECK_EQ_INT(m_BonusCalls, 0);
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_NONE);
    M_InitEnemy(&m_Enemy, 0);
    g_Enemies.spell_card.state = ENEMY_SPELL_TIMED_OUT;
    g_Enemies.spell_card.capturable = true;
    M_Frame();
    CHECK_EQ_INT(m_EndNameCalls, 2);
    CHECK_EQ_INT(m_ClearWithBonus, 1);
    CHECK_EQ_INT(m_BonusCalls, 0);
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_NONE);
    M_InitEnemy(&m_Enemy, 0);
    g_Stage.spell_card.state = 2;
    M_Frame();
    CHECK_EQ_INT(m_EndNameCalls, 2);
    CHECK_EQ_INT(g_Stage.spell_card.state, 0);
    M_InitEnemy(&m_Enemy, 0);
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.capturable = true;
    M_Frame();
    CHECK_EQ_INT(m_BonusCalls, 1);
    CHECK_EQ_INT(g_SceneStubCalls.spell_card_capture, 0);
    CHECK_EQ_INT(g_GameManager.stats.spell_cards_captured, 1);
    M_End();
}

// Enemies, sprites and the rest

TEST(spawn_and_kill_all_reach_the_enemies)
{
    M_Begin("spawn");
    M_INSTR(0, 95, 1, M_F(-10015.0f), M_F(5.0f), M_F(0.0f), 0x00030032, 1000);
    M_Instr0(0, 96);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Instr0(0, 1);
    M_Load();
    m_Enemy.move.pos.x = 70.0f;
    M_Frame();
    CHECK_EQ_INT(m_SpawnCount, 1);
    CHECK_EQ_INT(m_SpawnCall.sub, 1);
    CHECK(m_SpawnCall.pos.x == 70.0f && m_SpawnCall.pos.y == 5.0f);
    CHECK_EQ_INT(m_SpawnCall.life, 50);
    CHECK_EQ_INT(m_SpawnCall.item, 3);
    CHECK_EQ_INT(m_SpawnCall.score, 1000);
    CHECK_EQ_INT(m_KillAllCalls, 1);
    M_End();
}

TEST(sprite_opcodes_start_and_interrupt_scripts)
{
    M_Begin("sprites");
    M_INSTR(0, 97, 5);
    M_INSTR(0, 99, 2, 7);
    M_INSTR(0, 128, 0x00090004);
    M_INSTR(0, 129, 1, 0x00080006);
    M_Load();
    M_Frame();
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x105);
    CHECK_EQ_INT(m_Enemy.vms[2].script_index, 0x107);
    CHECK_EQ_INT(m_Enemy.vm.pending_interrupt, 4);
    CHECK_EQ_INT(m_Enemy.vms[1].pending_interrupt, 6);
    M_End();
}

TEST(the_sprite_turns_with_the_velocity)
{
    M_Begin("turns");
    M_INSTR(0, 98, 0x00020001, 0x00040003, 0x00000005);
    M_INSTR(1, 44, M_F(-1.0f), M_F(0.0f), M_F(0.0f));
    M_INSTR(2, 44, M_F(0.0f), M_F(0.0f), M_F(0.0f));
    M_INSTR(3, 44, M_F(1.0f), M_F(0.0f), M_F(0.0f));
    M_INSTR(4, 44, M_F(0.0f), M_F(0.0f), M_F(0.0f));
    M_Load();
    M_Frame();
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x101);
    CHECK_EQ_INT(m_Enemy.flags.facing, ENEMY_FACING_STILL);
    M_Frame();
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x104);
    CHECK_EQ_INT(m_Enemy.flags.facing, ENEMY_FACING_LEFT);
    M_Frame();
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x102);
    M_Frame();
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x105);
    M_Frame();
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x103);
    // A dead enemy does not turn.
    m_Enemy.life = 0;
    m_Enemy.move.velocity.x = -1.0f;
    M_Frame();
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x103);
    M_End();
}

TEST(flag_opcodes_set_the_enemy)
{
    M_Begin("flags");
    M_INSTR(0, 100, 0x00030201);
    M_INSTR(0, 103, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_INSTR(0, 104, 3);
    M_INSTR(0, 105, 2);
    M_INSTR(0, 107, 0xf);
    M_INSTR(0, 108, 1);
    M_INSTR(0, 109, 1, 4);
    M_INSTR(0, 111, 500);
    M_INSTR(0, 113, 200);
    M_INSTR(0, 114, 1);
    M_INSTR(0, 115, 900);
    M_INSTR(0, 112, 40);
    M_INSTR(0, 117, 1);
    M_INSTR(0, 120, 1);
    M_INSTR(0, 132, 1);
    M_INSTR(0, 135, 1);
    M_INSTR(0, 130, 0);
    M_INSTR(1, 116, 0);
    M_Instr0(2, 133);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Instr0(0, 1);
    M_Load();
    m_Enemy.death.item = 4;
    M_Frame();
    CHECK_EQ_INT(m_Enemy.death.effect, 1);
    CHECK_EQ_INT(m_Enemy.death.particle_color, 2);
    CHECK_EQ_INT(m_Enemy.death.item, 4);
    CHECK(m_Enemy.hitbox_size.y == 2.0f);
    CHECK(m_Enemy.flags.contact_damage);
    CHECK(!m_Enemy.flags.takes_damage);
    CHECK_EQ_INT(m_Enemy.flags.death, 7);
    CHECK_EQ_INT(m_Enemy.script.death_sub, 1);
    CHECK_EQ_INT(m_Enemy.script.interrupts[4], 1);
    CHECK_EQ_INT(m_Enemy.life, 500);
    CHECK_EQ_INT(m_Enemy.max_life, 500);
    CHECK_EQ_INT(m_Enemy.callbacks.life, 200);
    CHECK_EQ_INT(m_Enemy.callbacks.life_sub, 1);
    CHECK_EQ_INT(m_Enemy.callbacks.time, 900);
    CHECK_EQ_INT(m_Enemy.callbacks.timer.current, 40);
    CHECK(m_Enemy.flags.shootable);
    CHECK(m_Enemy.flags.rotate_sprite);
    CHECK(m_Enemy.flags.hidden);
    CHECK(m_Enemy.flags.keep_spell_card);
    CHECK(!m_Enemy.flags.no_call_stack);
    M_Frame();
    CHECK_EQ_INT(m_Enemy.callbacks.time_sub, 0);
    m_Enemy.callbacks.timer.current = 70;
    M_Frame();
    CHECK_EQ_INT(m_Enemy.callbacks.time_sub, 1);
    CHECK_EQ_INT(m_Enemy.callbacks.timer.current, 0);
    M_End();
}

TEST(the_boss_opcode_names_and_releases_a_boss)
{
    M_Begin("boss");
    M_INSTR(0, 101, 2);
    M_INSTR(1, 101, M_U(-1));
    M_Load();
    M_Frame();
    CHECK(g_Enemies.bosses[2] == &m_Enemy);
    CHECK(g_Gui.boss.present);
    CHECK(g_Gui.boss.health == 1.0f);
    CHECK(m_Enemy.flags.boss);
    CHECK_EQ_INT(m_Enemy.boss_index, 2);
    M_Frame();
    CHECK(g_Enemies.bosses[2] == nullptr);
    CHECK(!g_Gui.boss.present);
    CHECK(!m_Enemy.flags.boss);
    M_End();
}

TEST(an_orbit_follows_the_enemy)
{
    M_Begin("orbit");
    M_INSTR(0, 102, 5, M_F(0.0f), M_F(0.5f), M_F(1.0f), M_F(40.0f));
    M_Load();
    m_Enemy.move.pos = (VEC3) { 3.0f, 4.0f, 0.0f };
    M_Frame();
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK_EQ_INT(m_EffectCalls[0].type, EFFECT_ORBIT);
    CHECK_EQ_INT(m_EffectCalls[0].color, 0xffff2020u);
    CHECK(m_EffectCalls[0].pos.x == 3.0f);
    CHECK(m_Enemy.orbits.effects[0] == &m_Effects[0]);
    CHECK(m_Effects[0].axis.y == 0.5f && m_Effects[0].axis.z == 1.0f);
    CHECK(m_Enemy.orbits.radius == 40.0f);
    CHECK_EQ_INT(m_Enemy.orbits.count, 1);
    M_End();
}

TEST(effects_and_items_drop_at_the_enemy)
{
    M_Begin("drops");
    M_INSTR(0, 118, 4, 3, 0xff00ff00u);
    M_INSTR(0, 119, 3);
    M_INSTR(0, 124, 5);
    M_INSTR(1, 119, 1);
    M_Load();
    m_Enemy.move.pos = (VEC3) { 100.0f, 200.0f, 0.0f };
    g_GameManager.player.power = 0x10;
    RNG expected = g_Rng;
    M_Frame();
    CHECK_EQ_INT(m_EffectCalls[0].type, 4);
    CHECK_EQ_INT(m_EffectCalls[0].count, 3);
    CHECK_EQ_INT(m_EffectCalls[0].color, 0xff00ff00u);
    CHECK_EQ_INT(m_ItemCount, 4);
    for (int32_t i = 0; i < 3; i++) {
        const float x = 100.0f + (Rng_F32InRange(&expected, 144.0f) - 72.0f);
        const float y = 200.0f + (Rng_F32InRange(&expected, 144.0f) - 72.0f);
        CHECK(m_ItemCalls[i].pos.x == x && m_ItemCalls[i].pos.y == y);
        CHECK_EQ_INT(m_ItemCalls[i].type, i == 0 ? ITEM_BIG_POWER : ITEM_POWER);
    }
    CHECK_EQ_INT(m_ItemCalls[3].type, 5);
    CHECK(m_ItemCalls[3].pos.x == 100.0f);
    g_GameManager.player.power = 0x80;
    M_Frame();
    CHECK_EQ_INT(m_ItemCalls[4].type, ITEM_POINT);
    M_End();
}

TEST(the_stage_and_the_boss_bar_follow_the_script)
{
    M_Begin("stage");
    M_Instr0(0, 125);
    M_INSTR(0, 126, 3);
    M_Instr0(0, 83);
    M_Load();
    g_GameManager.run.progress_frames = 100;
    M_Frame();
    CHECK(g_Stage.script.resume);
    CHECK_EQ_INT(g_Gui.boss.lives, 3);
    CHECK_EQ_INT(g_GameManager.run.progress_frames, 1900);
    CHECK_EQ_INT(m_ClearToItems, 1);
    M_End();
}

TEST(a_frame_function_runs_while_the_enemy_has_life)
{
    M_Begin("frame_func");
    M_INSTR(0, 122, 6);
    M_INSTR(2, 122, M_U(-1));
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_State()->frame_func, ECL_FUNC_SPARK_CIRCLE);
    CHECK(m_Enemy.sparks.angle == 0.017453292f);
    m_Enemy.life = 0;
    M_Frame();
    CHECK(m_Enemy.sparks.angle == 0.017453292f);
    m_Enemy.life = 1;
    M_Frame();
    CHECK_EQ_INT(M_State()->frame_func, ECL_FUNC_NONE);
    CHECK(m_Enemy.sparks.angle == 0.017453292f);
    M_End();
}

// Pattern functions

static void M_Call(const ECL_FUNC func, const int32_t arg)
{
    M_INSTR(0, 121, M_U(func), M_U(arg));
}

TEST(freeze_bullets_stops_or_scatters_them)
{
    M_Begin("freeze");
    M_Call(ECL_FUNC_FREEZE_BULLETS, 0);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Call(ECL_FUNC_FREEZE_BULLETS, 1);
    M_Load();
    const VEC3 pos = { 1.0f, 2.0f, 0.0f };
    BULLET *const a = M_AddBullet(0, BULLET_FLYING, &m_SmallSprite, pos, 2.0f);
    BULLET *const b = M_AddBullet(1, BULLET_FADING, &m_SmallSprite, pos, 2.0f);
    a->velocity = (VEC3) { 1.0f, 1.0f, 0.0f };
    M_Frame();
    CHECK_EQ_INT(m_EffectCalls[0].type, EFFECT_SHOCKWAVE);
    CHECK_EQ_INT(a->color, 15);
    CHECK_EQ_INT(a->sprites.flying.sprite, 15);
    CHECK(a->speed == 0.0f && a->velocity.x == 0.0f);
    CHECK_EQ_INT(b->color, 0);
    M_InitEnemy(&m_Enemy, 1);
    M_AddBullet(2, BULLET_SPAWNING_1, &m_SmallSprite, pos, 2.0f);
    RNG expected = g_Rng;
    M_Frame();
    for (int32_t i = 0; i < 3; i += 2) {
        const BULLET *const bullet = &g_Bullets.bullets[i];
        float x;
        float y;
        X87_SinCosScale(
            Rng_F32InRange(&expected, M_TWO_PI) - M_PI, 0.01f, &x, &y);
        CHECK_EQ_INT(bullet->flags, BULLET_FLAG_ACCELERATE);
        CHECK_EQ_INT(bullet->change.frames, 220);
        CHECK_EQ_INT(bullet->timer.current, 0);
        CHECK(bullet->change.accel.x == x && bullet->change.accel.y == y);
    }
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_End();
}

TEST(shoot_from_random_point_fires_from_a_box)
{
    M_Begin("random_point");
    M_Call(ECL_FUNC_SHOOT_FROM_RANDOM_POINT, 100);
    M_Load();
    m_Enemy.move.pos = (VEC3) { 50.0f, 60.0f, 0.0f };
    m_Enemy.shots.offset = (VEC3) { 1.0f, 1.0f, 4.0f };
    RNG expected = g_Rng;
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 1);
    const float x = Rng_F32InRange(&expected, 100.0f) + 50.0f - 100.0f / 2.0f;
    const float y = Rng_F32InRange(&expected, 75.0f) + 60.0f - 75.0f / 2.0f;
    CHECK(m_Fired[0].pos.x == x && m_Fired[0].pos.y == y);
    CHECK(m_Fired[0].pos.z == 4.0f);
    M_End();
}

TEST(star_pattern_queues_its_sound_on_the_frames_it_fires)
{
    M_Begin("star_sound");
    M_INSTR(0, 122, ECL_FUNC_STAR_PATTERN);
    M_Load();
    M_State()->int_vars[3] = 12;
    Sound_Reset();
    M_Frame();
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_SHOT_0_QUIET);
    Sound_Reset();
    M_Frame();
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    M_End();
}

TEST(star_pattern_fires_five_points_every_sixth_frame)
{
    M_Begin("star");
    M_INSTR(0, 122, ECL_FUNC_STAR_PATTERN);
    M_Load();
    M_State()->int_vars[3] = 12;
    M_State()->float_vars[3] = 50.0f;
    m_Enemy.shots.shooter.speed1 = 1.0f;
    m_Enemy.shots.shooter.speed2 = 0.5f;
    m_Enemy.move.pos = (VEC3) { 100.0f, 100.0f, 5.0f };
    g_Player.pos = (VEC3) { 200.0f, 300.0f, 0.0f };
    RNG expected = g_Rng;
    float angles[6];
    angles[0] = Rng_F32InRange(&expected, M_TWO_PI) - M_PI;
    angles[1] = Angle_AddNormalize(angles[0], 2.5132742f);
    for (int32_t frame = 0; frame < 13; frame++) {
        const int32_t fired = m_FiredCount;
        M_Frame();
        if (frame % 30 == 0) {
            angles[0] = angles[1];
            for (int32_t i = 1; i < 6; i++) {
                angles[i] = Angle_AddNormalize(angles[i - 1], 2.5132742f);
            }
        }
        if (frame == 12) {
            CHECK_EQ_INT(m_FiredCount, fired);
            CHECK_EQ_INT(M_State()->frame_func, ECL_FUNC_NONE);
            break;
        }
        if (frame % 6 != 0) {
            CHECK_EQ_INT(m_FiredCount, fired);
            continue;
        }
        CHECK_EQ_INT(m_FiredCount, fired + 5);
        float progress = (float)frame / 12.0f;
        const float t = progress * 0.1f;
        const VEC3 centre = {
            (200.0f - 100.0f) * t + 100.0f,
            (300.0f - 100.0f) * t + 100.0f,
            0.0f,
        };
        progress += 0.5f;
        float angle = 1.0471976f * progress;
        for (int32_t i = 0; i < 5; i++) {
            const float s = (float)(frame % 30) / 30.0f;
            VEC3 from = {};
            VEC3 to = {};
            X87_SinCosScale(angles[i], 50.0f, &from.x, &from.y);
            X87_SinCosScale(angles[i + 1], 50.0f, &to.x, &to.y);
            const BULLET_SHOOTER *const shooter = &m_Fired[fired + i];
            CHECK(shooter->pos.x == centre.x + ((to.x - from.x) * s + from.x));
            CHECK(shooter->pos.y == centre.y + ((to.y - from.y) * s + from.y));
            CHECK(shooter->pos.z == 0.0f);
            CHECK(shooter->speed1 == 1.0f + Rng_F32InRange(&expected, 0.5f));
            CHECK(shooter->angle1 == angle);
            angle -= 0.5235988f * progress;
        }
        CHECK(m_Enemy.shots.shooter.speed1 == 1.0f);
    }
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_End();
}

TEST(shot_type_values_fill_three_variables)
{
    M_Begin("shot_values");
    M_Call(ECL_FUNC_GET_SHOT_TYPE_VALUES, 0);
    M_Load();
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 0;
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[1], 1);
    CHECK_EQ_INT(M_State()->int_vars[2], 4);
    CHECK_EQ_INT(M_State()->int_vars[3], 0);
    M_End();
}

TEST(freeze_or_send_off_freezes_with_a_low_argument)
{
    M_Begin("freeze_game");
    M_Call(ECL_FUNC_FREEZE_OR_SEND_OFF, 1);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Call(ECL_FUNC_FREEZE_OR_SEND_OFF, -256);
    M_Load();
    M_State()->int_vars[2] = 5;
    M_Frame();
    CHECK(g_GameManager.mode.frozen);
    CHECK_EQ_INT(m_EffectCalls[0].type, EFFECT_SHOCKWAVE);
    CHECK_EQ_INT(M_State()->int_vars[2], 0);
    // The game manager keeps the argument in a byte, so -256 is 0.
    M_InitEnemy(&m_Enemy, 1);
    M_Frame();
    CHECK(!g_GameManager.mode.frozen);
    M_End();
}

TEST(freeze_or_send_off_sends_large_bullets_off)
{
    M_Begin("send_off");
    M_Call(ECL_FUNC_FREEZE_OR_SEND_OFF, 2);
    M_Load();
    for (int32_t difficulty = 0; difficulty < 4; difficulty += 3) {
        g_GameManager.run.difficulty = difficulty;
        memset(&g_Bullets, 0, sizeof(g_Bullets));
        for (int32_t i = 0; i < 300; i++) {
            const VEC3 pos = { i % 2 == 0 ? 0.0f : 300.0f, 0.0f, 0.0f };
            M_AddBullet(i, BULLET_FLYING, &m_LargeSprite, pos, 2.0f);
        }
        M_AddBullet(300, BULLET_FLYING, &m_SmallSprite, (VEC3) {}, 2.0f);
        g_Bullets.bullets[3].color = 5;
        m_AwayAngle = 0.5f;
        M_InitEnemy(&m_Enemy, 0);
        RNG expected = g_Rng;
        M_Frame();
        int32_t count = difficulty <= 1 ? 14 : 52;
        int32_t sent = 0;
        for (int32_t i = 0; i < 300 && count > 0; i++) {
            const BULLET *const bullet = &g_Bullets.bullets[i];
            if (i == 3) {
                CHECK_EQ_INT(bullet->color, 5);
                continue;
            }
            if (Rng_U16(&expected) % 4 != 0) {
                CHECK_EQ_INT(bullet->color, 0);
                continue;
            }
            float angle;
            if (bullet->pos.x > 128.0f) {
                angle = difficulty <= 1
                    ? Rng_F32InRange(&expected, 2.3561945f) + M_QUARTER_PI
                    : Rng_F32InRange(&expected, M_TWO_PI);
            } else {
                const float turn = Rng_F32InRange(&expected, M_TWO_PI);
                angle = 0.5f + M_HALF_PI + turn;
            }
            float x;
            float y;
            X87_SinCosScale(angle, 2.0f, &x, &y);
            CHECK_EQ_INT(bullet->color, 5);
            CHECK(bullet->angle == angle);
            CHECK(bullet->velocity.x == x && bullet->velocity.y == y);
            count--;
            sent++;
        }
        CHECK_EQ_INT(sent, difficulty <= 1 ? 14 : 52);
        CHECK_EQ_INT(g_Bullets.bullets[299].color == 5, false);
        CHECK_EQ_INT(g_Bullets.bullets[300].color, 0);
        CHECK_EQ_INT(g_Rng.seed, expected.seed);
    }
    M_End();
}

TEST(side_arc_fans_queue_their_sound_on_the_frames_they_fire)
{
    M_Begin("side_arc_sound");
    M_INSTR(0, 122, ECL_FUNC_SIDE_ARC_FANS);
    M_Load();
    M_State()->int_vars[2] = 9;
    Sound_Reset();
    M_Frame();
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_SHOT_0);
    Sound_Reset();
    M_Frame();
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    M_End();
}

TEST(side_arc_fans_fire_nine_bullets_every_ninth_frame)
{
    M_Begin("side_arc");
    M_INSTR(0, 122, ECL_FUNC_SIDE_ARC_FANS);
    M_Load();
    m_Enemy.move.pos = (VEC3) { 100.0f, 100.0f, 0.0f };
    g_Player.pos = (VEC3) { 150.0f, 300.0f, 0.0f };
    g_GameManager.run.difficulty = 0;
    M_State()->int_vars[2] = 9;
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 9);
    CHECK_EQ_INT(M_State()->int_vars[2], 10);
    const float scale = 0.5f - (float)1 * 0.5f / 9.0f;
    VEC3 to_player = { 50.0f, 200.0f, 0.0f };
    VEC3 perp = Vec3_Scale(Vec3_Normalize(to_player), -256.0f);
    to_player = Vec3_Scale(to_player, scale);
    const VEC3 offset = Vec3_Add(to_player, perp);
    perp = (VEC3) { -perp.x, -perp.y, -perp.z };
    float c = X87_Cos(M_QUARTER_PI);
    float s = X87_Sin(M_QUARTER_PI);
    VEC3 old = perp;
    perp.x = old.x * c + old.y * s;
    perp.y = -old.x * s + old.y * c;
    c = X87_Cos(-0.17453294f);
    s = X87_Sin(-0.17453294f);
    float angle = -M_QUARTER_PI;
    for (int32_t i = 0; i < 9; i++, angle += 0.17453294f) {
        old = perp;
        perp.x = old.x * c + old.y * s;
        perp.y = -old.x * s + old.y * c;
        const BULLET_SHOOTER *const shooter = &m_Fired[i];
        CHECK(shooter->pos.x == 100.0f + perp.x + offset.x);
        CHECK(shooter->pos.y == 100.0f + perp.y + offset.y);
        CHECK(shooter->angle1 == angle);
        CHECK(shooter->speed1 == 2.0f);
        CHECK_EQ_INT(shooter->type, 8);
        CHECK_EQ_INT(shooter->color, 3);
        CHECK_EQ_INT(shooter->columns, 1);
        CHECK(shooter->angle2 == 0.5235988f);
    }
    for (int32_t i = 0; i < 8; i++) {
        M_Frame();
    }
    CHECK_EQ_INT(m_FiredCount, 9);
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 18);
    CHECK(m_Fired[17].angle1 == 0.0f);
    CHECK_EQ_INT(m_Fired[17].columns, 1);
    g_GameManager.run.difficulty = 2;
    for (int32_t i = 0; i < 9; i++) {
        M_Frame();
    }
    CHECK_EQ_INT(m_FiredCount, 27);
    CHECK(m_Fired[26].angle1 == 0.0f);
    CHECK_EQ_INT(m_Fired[26].columns, 3);
    M_End();
}

TEST(spark_circle_spawns_pairs_of_sparks)
{
    M_Begin("sparks");
    M_Call(ECL_FUNC_SPARK_CIRCLE, 0);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, 132, 1);
    M_Call(ECL_FUNC_SPARK_CIRCLE, 0);
    M_Load();
    m_Enemy.move.pos = (VEC3) { 100.0f, 100.0f, 0.0f };
    m_Enemy.sparks.angle = 0.78f;
    m_Enemy.sparks.timer = (TIMER) { .previous = 124, .current = 125 };
    RNG expected = g_Rng;
    M_Frame();
    const float spark_angle = 0.78f + 0.017453292f - M_HALF_PI;
    CHECK(m_Enemy.sparks.angle == spark_angle);
    CHECK_EQ_INT(m_Enemy.sparks.timer.current, 126);
    CHECK_EQ_INT(m_EffectCount, 2);
    const int32_t k = Rng_U16InRange(&expected, 6) + 6;
    const float r = (float)k * 160.0f / 16.0f + 32.0f;
    const float angle = spark_angle - (float)k * M_PI / 40.0f;
    CHECK(m_EffectCalls[0].pos.x == 100.0f + X87_Cos(angle) * r);
    CHECK(m_EffectCalls[1].pos.x == 100.0f - X87_Cos(angle) * r);
    CHECK(m_EffectCalls[1].pos.y == 100.0f + X87_Sin(angle) * r);
    CHECK_EQ_INT(m_EffectCalls[0].type, EFFECT_SPARK);
    CHECK_EQ_INT(m_EffectCalls[0].color, 0xff3030ffu);
    for (int32_t i = 0; i < 2; i++) {
        const float vx = (Rng_F32(&expected) * 40.0f - 20.0f) / 60.0f;
        const float vy = 8.0f * (float)k / 60.0f - 0.26666668f;
        CHECK(m_Effects[i].velocity.x == vx);
        CHECK(m_Effects[i].velocity.y == vy);
        CHECK(m_Effects[i].acceleration.x == -vx * (1.0f / 120.0f));
        CHECK(m_Effects[i].acceleration.y == -vy * (1.0f / 120.0f));
    }
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    // At frame 33 the timer is not a multiple of 4 or 8: no sparks.
    M_InitEnemy(&m_Enemy, 0);
    m_Enemy.sparks.timer = (TIMER) { .previous = 32, .current = 33 };
    M_Frame();
    CHECK_EQ_INT(m_EffectCount, 2);
    M_InitEnemy(&m_Enemy, 1);
    M_Frame();
    CHECK_EQ_INT(m_ReleaseOrbitCalls, 1);
    CHECK(m_Enemy.sparks.angle == 0.0f);
    M_End();
}

// Computes the 48 points of ECL_FUNC_TWO_RING_SHOTS around pos, from
// origin, and their angles.
static void M_TwoRings(
    const VEC3 pos, const float origin, VEC3 *const points, float *const angles,
    float *const radii)
{
    int32_t n = 0;
    for (int32_t half = 0; half < 2; half++) {
        const float start = half == 0 ? -M_PI + origin : -2.7488937f + origin;
        const float step = half == 0 ? M_QUARTER_PI : -M_QUARTER_PI;
        VEC3 at[8];
        float angle = start;
        for (int32_t i = 0; i < 8; i++) {
            at[i] = pos;
            at[i].x += X87_Cos(angle) * 32.0f;
            at[i].y += X87_Sin(angle) * 32.0f;
            angle += M_QUARTER_PI;
        }
        angle = start;
        for (int32_t k = 0; k < 3; k++) {
            const float radius = k < 2 ? 112.0f : 480.0f;
            for (int32_t i = 0; i < 8; i++) {
                points[n] = at[i];
                angles[n] = angle;
                radii[n] = radius;
                n++;
                at[i].x += X87_Cos(angle) * radius;
                at[i].y += X87_Sin(angle) * radius;
                angle += M_QUARTER_PI;
            }
            angle += step - M_TWO_PI;
        }
    }
}

TEST(two_ring_shots_fire_lasers_or_the_shooter)
{
    M_Begin("two_rings");
    M_Call(ECL_FUNC_TWO_RING_SHOTS, 0);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Call(ECL_FUNC_TWO_RING_SHOTS, 1);
    M_Load();
    m_Enemy.move.pos = (VEC3) { 192.0f, 100.0f, 0.0f };
    g_GameManager.run.difficulty = 0;
    RNG expected = g_Rng;
    M_Frame();
    VEC3 points[48];
    float angles[48];
    float radii[48];
    M_TwoRings(
        m_Enemy.move.pos, Rng_F32InRange(&expected, M_TWO_PI), points, angles,
        radii);
    CHECK_EQ_INT(m_LaserCount, 48);
    for (int32_t i = 0; i < 48; i++) {
        const BULLET_SHOOTER *const shooter = &m_LaserShots[i];
        CHECK(shooter->pos.x == points[i].x && shooter->pos.y == points[i].y);
        CHECK(shooter->angle1 == angles[i]);
        CHECK(shooter->ex_floats[1] == radii[i]);
        CHECK(shooter->ex_floats[3] == 28.0f);
        CHECK_EQ_INT(shooter->ex_ints[0], (i % 24) / 8 * 16 + 60);
        CHECK_EQ_INT(shooter->color, 2);
        CHECK_EQ_INT(shooter->aim, BULLET_AIM_FAN);
        CHECK_EQ_INT(shooter->flags, BULLET_FLAG_SPAWN_1);
    }
    M_InitEnemy(&m_Enemy, 1);
    m_Enemy.move.pos = (VEC3) { 192.0f, 100.0f, 0.0f };
    m_Enemy.shots.shooter.type = 9;
    expected = g_Rng;
    M_Frame();
    M_TwoRings(
        m_Enemy.move.pos, Rng_F32InRange(&expected, M_TWO_PI), points, angles,
        radii);
    CHECK_EQ_INT(m_FiredCount, 48);
    for (int32_t i = 0; i < 48; i++) {
        CHECK(
            m_Fired[i].pos.x == points[i].x && m_Fired[i].pos.y == points[i].y);
        CHECK_EQ_INT(m_Fired[i].type, 9);
    }
    M_End();
}

TEST(shoot_from_large_bullets_counts_them)
{
    M_Begin("from_large");
    M_Call(ECL_FUNC_SHOOT_FROM_LARGE_BULLETS, 0);
    M_Load();
    M_AddBullet(
        2, BULLET_FLYING, &m_LargeSprite, (VEC3) { 1.0f, 2.0f, 0.0f }, 1.0f);
    M_AddBullet(5, BULLET_FLYING, &m_SmallSprite, (VEC3) {}, 1.0f);
    M_AddBullet(7, BULLET_FADING, &m_LargeSprite, (VEC3) {}, 1.0f);
    M_AddBullet(
        9, BULLET_SPAWNING_2, &m_LargeSprite, (VEC3) { 3.0f, 4.0f, 0.0f },
        1.0f);
    RNG expected = g_Rng;
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 2);
    CHECK_EQ_INT(M_State()->int_vars[3], 2);
    CHECK(m_Fired[0].pos.x == 1.0f && m_Fired[1].pos.y == 4.0f);
    for (int32_t i = 0; i < 2; i++) {
        CHECK(m_Fired[i].angle1 == Rng_F32InRange(&expected, M_TWO_PI) - M_PI);
        CHECK_EQ_INT(m_Fired[i].type, 3);
        CHECK_EQ_INT(m_Fired[i].color, 1);
        CHECK_EQ_INT(m_Fired[i].flags, BULLET_FLAG_SPAWN_3);
        CHECK_EQ_INT(m_Fired[i].aim, BULLET_AIM_FAN);
        CHECK_EQ_INT(m_Fired[i].columns, 1);
        CHECK_EQ_INT(m_Fired[i].rows, 1);
    }
    M_End();
}

TEST(swirl_and_scatter_start_the_small_stopped_bullets)
{
    M_Begin("swirl");
    M_Call(ECL_FUNC_SWIRL_STOPPED_BULLETS, 0);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Call(ECL_FUNC_SCATTER_STOPPED_BULLETS, 0);
    M_Load();
    m_Enemy.move.pos = (VEC3) { 10.0f, 20.0f, 0.0f };
    M_AddBullet(
        0, BULLET_FLYING, &m_SmallSprite, (VEC3) { 13.0f, 24.0f, 0.0f }, 0.0f);
    M_AddBullet(
        1, BULLET_FLYING, &m_SmallSprite, (VEC3) { 10.0f, 20.0f, 0.0f }, 0.0f);
    M_AddBullet(2, BULLET_FLYING, &m_SmallSprite, (VEC3) {}, 1.0f);
    M_AddBullet(3, BULLET_FLYING, &m_LargeSprite, (VEC3) {}, 0.0f);
    RNG expected = g_Rng;
    M_Frame();
    const float base = Rng_F32InRange(&expected, M_TWO_PI) - M_PI;
    const float dists[2] = { X87_Sqrt(9.0f + 16.0f), 0.0f };
    for (int32_t i = 0; i < 2; i++) {
        const BULLET *const bullet = &g_Bullets.bullets[i];
        float x;
        float y;
        X87_SinCosScale(dists[i] * M_PI / 256.0f + base, 0.01f, &x, &y);
        CHECK(bullet->change.accel.x == x && bullet->change.accel.y == y);
        CHECK_EQ_INT(bullet->flags, BULLET_FLAG_ACCELERATE);
        CHECK_EQ_INT(bullet->color, 2);
        CHECK(bullet->speed == 0.01f);
        CHECK_EQ_INT(bullet->change.frames, 120);
        CHECK_EQ_INT(bullet->timer.current, 0);
    }
    CHECK_EQ_INT(g_Bullets.bullets[2].flags, 0);
    CHECK_EQ_INT(g_Bullets.bullets[3].flags, 0);
    CHECK_EQ_INT(m_EffectCalls[0].type, EFFECT_SHOCKWAVE);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    memset(&g_Bullets, 0, sizeof(g_Bullets));
    M_AddBullet(0, BULLET_FLYING, &m_SmallSprite, (VEC3) {}, 0.0f);
    M_AddBullet(4, BULLET_FLYING, &m_SmallSprite, (VEC3) {}, 0.0f);
    M_InitEnemy(&m_Enemy, 1);
    expected = g_Rng;
    M_Frame();
    Rng_F32InRange(&expected, M_TWO_PI);
    for (int32_t i = 0; i < 5; i += 4) {
        float x;
        float y;
        X87_SinCosScale(
            Rng_F32InRange(&expected, M_TWO_PI) - M_PI, 0.01f, &x, &y);
        CHECK(g_Bullets.bullets[i].change.accel.x == x);
        CHECK(g_Bullets.bullets[i].change.accel.y == y);
    }
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_End();
}

TEST(the_bomb_shield_holds_until_sixty_frames_after_a_bomb)
{
    M_Begin("shield");
    M_INSTR(0, 122, ECL_FUNC_SPARKS_AND_BOMB_SHIELD);
    M_Load();
    m_Enemy.turns.left = 3;
    m_Enemy.flags.shootable = true;
    g_Player.bomb.running = true;
    M_Frame();
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x1a5);
    CHECK_EQ_INT(m_Enemy.turns.left, -1);
    CHECK(!m_Enemy.flags.shootable);
    CHECK_EQ_INT(m_Enemy.shield_timer.current, 60);
    CHECK(m_Enemy.sparks.angle == 0.017453292f);
    g_Player.bomb.running = false;
    for (int32_t i = 0; i < 59; i++) {
        M_Frame();
    }
    CHECK(!m_Enemy.flags.shootable);
    CHECK_EQ_INT(m_Enemy.shield_timer.current, 1);
    M_Frame();
    CHECK(m_Enemy.flags.shootable);
    CHECK_EQ_INT(m_Enemy.vm.script_index, 0x1a0);
    CHECK_EQ_INT(m_Enemy.turns.left, 0xa1);
    M_End();
}

TEST(shoot_from_lasers_fires_along_the_first_eight)
{
    M_Begin("from_lasers");
    M_Call(ECL_FUNC_SHOOT_FROM_LASERS, 0);
    M_Load();
    LASER lasers[10] = {};
    lasers[0] = (LASER) { .active = true, .angle = 0.5f };
    lasers[2] = (LASER) { .active = false };
    lasers[9] = (LASER) { .active = true };
    m_Enemy.shots.lasers[0] = &lasers[0];
    m_Enemy.shots.lasers[2] = &lasers[2];
    m_Enemy.shots.lasers[9] = &lasers[9];
    m_Enemy.move.pos = (VEC3) { 10.0f, 20.0f, 0.0f };
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 1);
    VEC3 expected = { 64.0f, 0.0f, 0.0f };
    Angle_Rotate2D(&expected, &expected, 0.5f);
    CHECK(m_Fired[0].pos.x == expected.x + 10.0f);
    CHECK(m_Fired[0].pos.y == expected.y + 20.0f);
    M_End();
}

TEST(arena_circle_shots_fire_every_sixth_call)
{
    M_Begin("arena");
    const int32_t call = M_INSTR(0, 121, ECL_FUNC_ARENA_CIRCLE_SHOTS, 4);
    M_Target(M_INSTR(1, 2, 0, 0), call);
    M_Load();
    M_State()->float_vars[1] = 0.25f;
    M_State()->float_vars[2] = 0.5f;
    M_State()->float_vars[3] = 100.0f;
    m_Enemy.shots.shooter.pos.z = 3.0f;
    m_Enemy.shots.shooter.type = 6;
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 4);
    CHECK_EQ_INT(M_State()->int_vars[3], 1);
    float angle = 0.5f;
    for (int32_t i = 0; i < 4; i++, angle += M_TWO_PI / 4.0f) {
        float x;
        float y;
        X87_SinCosScale(angle, 100.0f, &x, &y);
        CHECK(m_Fired[i].pos.x == x + 192.0f && m_Fired[i].pos.y == y + 224.0f);
        CHECK(m_Fired[i].pos.z == 3.0f);
        CHECK(m_Fired[i].angle1 == angle + 0.25f);
        CHECK_EQ_INT(m_Fired[i].type, 6);
    }
    for (int32_t i = 0; i < 5; i++) {
        M_Frame();
        CHECK_EQ_INT(m_FiredCount, 4);
    }
    CHECK_EQ_INT(M_State()->int_vars[3], 6);
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 8);
    M_End();
}

TEST(shoot_along_lasers_fires_every_48_pixels)
{
    M_Begin("along");
    M_Call(ECL_FUNC_SHOOT_ALONG_LASERS, 0);
    M_Load();
    LASER laser = {
        .active = true,
        .angle = 0.3f,
        .start_offset = 10.0f,
        .end_offset = 110.0f,
        .pos = { 5.0f, 6.0f, 9.0f },
    };
    m_Enemy.shots.lasers[1] = &laser;
    M_State()->int_vars[3] = 7;
    M_Frame();
    CHECK_EQ_INT(m_FiredCount, 3);
    CHECK_EQ_INT(M_State()->int_vars[3], 1);
    float s;
    float c;
    X87_SinCos(0.3f, &s, &c);
    for (int32_t i = 0; i < 3; i++) {
        const float r = 10.0f + 48.0f * (float)i;
        CHECK(m_Fired[i].pos.x == c * r + 5.0f);
        CHECK(m_Fired[i].pos.y == s * r + 6.0f);
        CHECK(m_Fired[i].pos.z == 0.0f);
    }
    M_End();
}

TEST(burst_around_large_bullets_bends_the_small_ones)
{
    M_Begin("burst");
    M_INSTR(0, 122, ECL_FUNC_BURST_AROUND_LARGE_BULLETS);
    M_Load();
    m_Enemy.sparks.timer = (TIMER) { .previous = 1, .current = 1 };
    M_AddBullet(
        0, BULLET_FLYING, &m_LargeSprite, (VEC3) { 100.0f, 100.0f, 0.0f },
        1.0f);
    BULLET *const near = M_AddBullet(
        1, BULLET_FLYING, &m_SmallSprite, (VEC3) { 110.0f, 130.0f, 0.0f },
        0.0f);
    near->color = 3;
    M_AddBullet(
        2, BULLET_FLYING, &m_SmallSprite, (VEC3) { 300.0f, 300.0f, 0.0f },
        0.0f);
    const RNG expected = g_Rng;
    M_Frame();
    CHECK_EQ_INT(M_State()->int_vars[3], 1);
    const float large = X87_Atan2(100.0f, 100.0f);
    const float angle = (X87_Atan2(130.0f, 110.0f) - large) * 2.2f + large;
    float x;
    float y;
    X87_SinCosScale(angle, 0.01f, &x, &y);
    CHECK(near->angle == angle);
    CHECK(near->change.accel.x == x && near->change.accel.y == y);
    CHECK_EQ_INT(near->flags, BULLET_FLAG_ACCELERATE);
    CHECK_EQ_INT(near->color, 4);
    CHECK_EQ_INT(near->sprites.flying.sprite, 4);
    CHECK(near->speed == 0.01f);
    CHECK_EQ_INT(near->change.frames, 120);
    CHECK_EQ_INT(g_Bullets.bullets[2].flags, 0);
    CHECK(m_Enemy.sparks.angle == 0.017453292f);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_End();
}

TEST(follow_life_sets_values_from_the_life)
{
    M_Begin("follow_life");
    M_Call(ECL_FUNC_FOLLOW_LIFE, 0);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Call(ECL_FUNC_FOLLOW_LIFE, 1);
    M_Load();
    m_Enemy.life = 3000;
    M_Frame();
    CHECK(M_State()->float_vars[3] == 2.0f - (float)3000 * 1.0f / 6000.0f);
    CHECK_EQ_INT(M_State()->int_vars[5], 160);
    M_InitEnemy(&m_Enemy, 0);
    m_Enemy.life = 3000;
    m_Enemy.callbacks.timer.current = 7200;
    M_Frame();
    CHECK(M_State()->float_vars[3] == 2.0f);
    CHECK_EQ_INT(M_State()->int_vars[5], 40);
    M_InitEnemy(&m_Enemy, 1);
    m_Enemy.life = 1500;
    RNG expected = g_Rng;
    M_Frame();
    float range = 320.0f - (float)1500 * 160.0f / 6000.0f;
    CHECK(
        M_State()->float_vars[2]
        == 192.0f - range / 2.0f + Rng_F32InRange(&expected, range));
    range = 128.0f - (float)1500 * 64.0f / 6000.0f;
    CHECK(
        M_State()->float_vars[3]
        == 96.0f - range / 2.0f + Rng_F32InRange(&expected, range));
    M_End();
}

TEST(instant_position_changes_renew_presentation_but_repeats_do_not)
{
    M_Begin("position_presentation");
    M_INSTR(0, 43, M_F(1.0f), M_F(2.0f), M_F(3.0f));
    M_INSTR(1, 43, M_F(4.0f), M_F(2.0f), M_F(3.0f));
    M_Load();
    m_Enemy.move.pos = (VEC3) { 1, 2, 3 };
    M_Frame();
    CHECK_EQ_INT(m_PresentationRenewals, 0);
    M_Frame();
    CHECK_EQ_INT(m_PresentationRenewals, 1);
    M_End();
}
