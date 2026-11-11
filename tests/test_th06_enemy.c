#include "th06/enemy.h"

#include "anm_builder.h"
#include "ecl_builder.h"
#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"
#include "th06_scene_stubs.h"

#include "th06/angle.h"
#include "th06/anm.h"
#include "th06/bullet.h"
#include "th06/chain.h"
#include "th06/ecl.h"
#include "th06/effect.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"
#include "th06/stage.h"

#include "core/base/memory.h"
#include "core/gfx/private.h"
#include "core/base/utils.h"
#include "core/math/vec.h"

#include "stg/rng.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_MAX_CALLS 64
#define M_ALL 0xff

// The ECL opcodes the tests use.
#define M_OP_DELETE 1
#define M_OP_SET_VELOCITY 44
#define M_OP_SET_BOUNDS 65
#define M_OP_START_SCRIPT 97
#define M_OP_START_SPRITE_SCRIPT 99
#define M_OP_SET_DEATH_EFFECTS 100
#define M_OP_SET_BOSS 101
#define M_OP_ADD_ORBIT 102
#define M_OP_SET_CONTACT_DAMAGE 104
#define M_OP_SET_TAKES_DAMAGE 105
#define M_OP_SET_DEATH 107
#define M_OP_SET_DEATH_SUB 108
#define M_OP_SET_LIFE 111
#define M_OP_SET_LIFE_CALLBACK 113
#define M_OP_SET_LIFE_CALLBACK_SUB 114
#define M_OP_SET_TIME_CALLBACK 115
#define M_OP_SET_TIME_CALLBACK_SUB 116
#define M_OP_SET_SHOOTABLE 117
#define M_OP_SET_ROTATE_SPRITE 120
#define M_OP_SET_HIDDEN 132
#define M_OP_SET_KEEP_SPELL_CARD 135

// The ANM opcodes and scripts of the enemy sprites.
#define M_ANM_END 0
#define M_ANM_SPRITE 1
#define M_ANM_COLOR 4
#define M_SCRIPT_SMALL 0 // sprite 0, 32 by 32
#define M_SCRIPT_LARGE 1 // sprite 1, 64 by 64, turns green
#define M_SCRIPT_SHORT 2 // ends at frame 2

// The test links these instead of the other modules. Each records its
// calls; the player deals m_Damage to every enemy.

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

PLAYER g_Player;
BULLET_MANAGER g_Bullets;
GUI g_Gui;
STAGE g_Stage;

static char *m_Directory;
static ECL_BUILDER m_Builder;
static int32_t m_Sub;

static int32_t m_Damage;
static bool m_ByBomb;
static int32_t m_DamageCalls;
static PLAYER_CONTACT m_Contact;
static VEC3 m_ContactSize;
static int32_t m_ClearAll;
static int32_t m_ClearWithBonus;
static int32_t m_ClearMaxBonus;
static bool m_ClearItems;
static M_EFFECT_CALL m_EffectCalls[M_MAX_CALLS];
static EFFECT m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static M_ITEM_CALL m_ItemCalls[M_MAX_CALLS];
static int32_t m_ItemCount;
static LASER m_Laser;
static bool m_MsgRunning;
static bool m_MsgHolds;
static int32_t m_MsgStarted;

float Player_AngleFrom(const VEC3 pos)
{
    return 0.0f;
}

float Player_AngleTo(const VEC3 pos)
{
    return 0.0f;
}

PLAYER_CONTACT Player_CheckHit(const VEC3 pos, const VEC3 size)
{
    m_ContactSize = size;
    return m_Contact;
}

int32_t Player_CalcDamage(const VEC3 pos, const VEC3 size, bool *hit_by_bomb)
{
    m_DamageCalls++;
    *hit_by_bomb = m_ByBomb;
    return m_Damage;
}

void Bullet_FirePattern(const BULLET_SHOOTER *const shooter)
{
}

LASER *Bullet_SpawnLaser(const BULLET_SHOOTER *const shooter)
{
    return &m_Laser;
}

void Bullet_ClearAll(void)
{
    m_ClearAll++;
}

void Bullet_ClearAllToItems(void)
{
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

void Gui_StartMsg(const int32_t index)
{
    m_MsgStarted = index;
}

bool Gui_IsMsgRunning(void)
{
    return m_MsgRunning;
}

bool Gui_MsgHoldsTimeline(void)
{
    return m_MsgHolds;
}

void Gui_ShowSpellCardName(const int32_t sprite, const char *const name)
{
}

void Gui_EndSpellCardName(void)
{
}

void Gui_ShowSpellCardBonus(const int32_t bonus)
{
}

static uint32_t M_F(const float value)
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

static void M_AnmInstr(
    ANM_BUILDER *const b, const int32_t script, const int16_t time,
    const uint8_t opcode, const uint32_t arg)
{
    AnmBuilder_Instr(b, script, time, opcode, &arg, opcode == 0 ? 0 : 1);
}

static void M_WriteAnm(const char *const name, const int32_t first_id)
{
    ANM_BUILDER b;
    AnmBuilder_Init(&b, 256, 256);
    AnmBuilder_AddSprite(&b, first_id, 0.0f, 0.0f, 32.0f, 32.0f);
    AnmBuilder_AddSprite(&b, first_id + 1, 0.0f, 0.0f, 64.0f, 64.0f);
    int32_t script = AnmBuilder_BeginScript(&b, first_id + M_SCRIPT_SMALL);
    M_AnmInstr(&b, script, 0, M_ANM_SPRITE, (uint32_t)first_id);
    M_AnmInstr(&b, script, 1000, M_ANM_END, 0);
    script = AnmBuilder_BeginScript(&b, first_id + M_SCRIPT_LARGE);
    M_AnmInstr(&b, script, 0, M_ANM_SPRITE, (uint32_t)first_id + 1);
    M_AnmInstr(&b, script, 0, M_ANM_COLOR, 0x00ff00);
    M_AnmInstr(&b, script, 1000, M_ANM_END, 0);
    script = AnmBuilder_BeginScript(&b, first_id + M_SCRIPT_SHORT);
    M_AnmInstr(&b, script, 0, M_ANM_SPRITE, (uint32_t)first_id);
    M_AnmInstr(&b, script, 2, M_ANM_END, 0);
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&b, bytes);
    Th06Data_WriteFile(m_Directory, name, bytes, size);
}

static void M_ResetCalls(void)
{
    m_Damage = 0;
    m_ByBomb = false;
    m_DamageCalls = 0;
    m_Contact = PLAYER_CONTACT_NONE;
    m_ClearAll = 0;
    m_ClearWithBonus = 0;
    m_EffectCount = 0;
    m_ItemCount = 0;
    m_MsgRunning = false;
    m_MsgHolds = false;
    m_MsgStarted = -1;
    memset(m_Effects, 0, sizeof(m_Effects));
}

// Starts a test whose subroutine 0 shows the small sprite at frame 0.
static void M_Begin(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    M_WriteAnm("data/stg1enm.anm", 0);
    M_WriteAnm("data/stg1enm2.anm", 10);
    M_WriteAnm("data/stg3enm.anm", 0);
    EclBuilder_Init(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, M_OP_START_SCRIPT, M_SCRIPT_SMALL);
}

// Builds the ECL file of stage, loads it and registers the enemies.
static void M_LoadStage(const int32_t stage)
{
    static uint8_t bytes[ECL_BUILDER_MAX_SIZE];
    const size_t size = EclBuilder_Build(&m_Builder, bytes, nullptr);
    Th06Data_WriteFile(m_Directory, "data/ecldata1.ecl", bytes, size);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = 1;
    g_GameManager.run.difficulty = 1;
    g_GameManager.player_area = (PLAY_RECT) { 8.0f, 16.0f, 368.0f, 416.0f };
    g_GameManager.rank.max = 99;
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.homing_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    memset(&g_Gui, 0, sizeof(g_Gui));
    SceneStubCalls_Reset();
    M_ResetCalls();
    CHECK(Result_Log(Ecl_Load(), LOG_LEVEL_ERROR));
    g_GameManager.run.stage = stage;
    CHECK(Result_Log(Enemy_Register(), LOG_LEVEL_ERROR));
}

static void M_Load(void)
{
    M_LoadStage(1);
}

static void M_End(void)
{
    Enemy_Remove();
    Chain_Release(&g_Chain);
    Ecl_Release();
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

static void M_Frame(void)
{
    Chain_RunCalc(&g_Chain);
}

static void M_Frames(const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        M_Frame();
    }
}

static const VEC3 m_Pos = { 100.0f, 100.0f, 0.0f };

// Spawns subroutine 0 at m_Pos with no item.
static ENEMY *M_Spawn(void)
{
    return Enemy_Spawn(0, m_Pos, -1, -2, -1);
}

// Appends a timeline entry with a position, life, item and score.
static void M_Entry(
    const int16_t time, const int16_t opcode, const int16_t arg, const VEC3 pos,
    const int16_t life, const int16_t item, const int32_t score)
{
    const uint32_t args[5] = {
        M_F(pos.x),
        M_F(pos.y),
        M_F(pos.z),
        (uint32_t)(uint16_t)life | (uint32_t)(uint16_t)item << 16,
        (uint32_t)score,
    };
    EclBuilder_Entry(&m_Builder, time, arg, opcode, 28, args);
}

static int32_t M_ActiveCount(void)
{
    int32_t count = 0;
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        if (g_Enemies.enemies[i].flags.active) {
            count++;
        }
    }
    return count;
}

// Timeline

TEST(a_spawn_entry_gives_its_values)
{
    M_Begin("spawn_entry");
    M_INSTR(0, M_OP_SET_LIFE, 300);
    M_Entry(0, ECL_TIMELINE_SPAWN, 0, m_Pos, 50, 3, 1234);
    M_Load();
    M_Frame();
    const ENEMY *const enemy = &g_Enemies.enemies[0];
    CHECK(enemy->flags.active);
    CHECK(enemy->move.pos.x == 100.0f && enemy->move.pos.y == 100.0f);
    CHECK_EQ_INT(enemy->life, 50);
    CHECK_EQ_INT(enemy->max_life, 50);
    CHECK_EQ_INT(enemy->score, 1234);
    CHECK_EQ_INT(enemy->death.item, 3);
    CHECK(!enemy->flags.mirrored);
    M_End();
}

TEST(a_default_spawn_keeps_the_values_of_the_script)
{
    M_Begin("spawn_default");
    M_INSTR(0, M_OP_SET_LIFE, 300);
    M_Entry(0, ECL_TIMELINE_SPAWN_DEFAULT, 0, m_Pos, 50, 3, 1234);
    M_Load();
    M_Frame();
    const ENEMY *const enemy = &g_Enemies.enemies[0];
    CHECK_EQ_INT(enemy->life, 300);
    CHECK_EQ_INT(enemy->max_life, 300);
    CHECK_EQ_INT(enemy->score, 100);
    CHECK_EQ_INT(enemy->death.item, -1);
    M_End();
}

TEST(a_mirrored_spawn_mirrors_the_horizontal_movement)
{
    M_Begin("spawn_mirrored");
    M_INSTR(0, M_OP_SET_VELOCITY, M_F(2.0f), M_F(1.0f), M_F(0.0f));
    M_Entry(0, ECL_TIMELINE_SPAWN_MIRRORED, 0, m_Pos, -1, -1, -1);
    M_Entry(0, ECL_TIMELINE_SPAWN_MIRRORED_DEFAULT, 0, m_Pos, -1, -1, -1);
    M_Entry(0, ECL_TIMELINE_SPAWN_DEFAULT, 0, m_Pos, -1, -1, -1);
    M_Load();
    M_Frame();
    CHECK(g_Enemies.enemies[0].flags.mirrored);
    CHECK(g_Enemies.enemies[1].flags.mirrored);
    CHECK(!g_Enemies.enemies[2].flags.mirrored);
    CHECK(g_Enemies.enemies[0].move.pos.x == 98.0f);
    CHECK(g_Enemies.enemies[0].move.pos.y == 101.0f);
    CHECK(g_Enemies.enemies[2].move.pos.x == 102.0f);
    M_End();
}

TEST(a_random_spawn_draws_each_axis_at_or_below_minus_990)
{
    M_Begin("spawn_random");
    M_Entry(
        0, ECL_TIMELINE_SPAWN_RANDOM, 0, (VEC3) { -990.0f, -989.0f, -1000.0f },
        -1, -1, -1);
    M_Load();
    RNG expected = g_Rng;
    M_Frame();
    const float x = Rng_F32(&expected) * 368.0f;
    const float z = Rng_F32(&expected) * 800.0f;
    const ENEMY *const enemy = &g_Enemies.enemies[0];
    CHECK(enemy->move.pos.x == x);
    CHECK(enemy->move.pos.y == -989.0f);
    CHECK(enemy->move.pos.z == z);
    CHECK(g_Rng.seed == expected.seed);
    M_End();
}

TEST(a_random_spawn_draws_y_from_the_player_area_height)
{
    M_Begin("spawn_random_y");
    M_Entry(
        0, ECL_TIMELINE_SPAWN_RANDOM_MIRRORED_DEFAULT, 0,
        (VEC3) { 50.0f, -999.0f, 0.0f }, -1, -1, -1);
    M_Load();
    RNG expected = g_Rng;
    M_Frame();
    const ENEMY *const enemy = &g_Enemies.enemies[0];
    CHECK(enemy->move.pos.y == Rng_F32(&expected) * 416.0f);
    CHECK(enemy->flags.mirrored);
    M_End();
}

TEST(spawns_are_skipped_while_a_boss_is_present)
{
    M_Begin("spawn_boss");
    M_Entry(0, ECL_TIMELINE_SPAWN, 0, m_Pos, -1, -1, -1);
    M_Entry(1, ECL_TIMELINE_SET_POWER, 40, m_Pos, -1, -1, -1);
    M_Load();
    g_Gui.boss.present = true;
    M_Frame();
    CHECK_EQ_INT(M_ActiveCount(), 0);
    g_Gui.boss.present = false;
    M_Frames(2);
    CHECK_EQ_INT(M_ActiveCount(), 0);
    CHECK_EQ_INT(g_GameManager.player.power, 40);
    M_End();
}

TEST(start_msg_adds_ten_per_character)
{
    M_Begin("msg");
    M_Entry(0, ECL_TIMELINE_START_MSG, 1, m_Pos, -1, -1, -1);
    M_Load();
    g_GameManager.run.character = 1;
    M_Frame();
    CHECK_EQ_INT(m_MsgStarted, 11);
    M_End();
}

TEST(start_msg_1_on_easy_stage_5_starts_dialogue_3)
{
    M_Begin("msg_easy");
    M_Entry(0, ECL_TIMELINE_START_MSG, 1, m_Pos, -1, -1, -1);
    M_Entry(1, ECL_TIMELINE_START_MSG, 2, m_Pos, -1, -1, -1);
    M_Load();
    g_GameManager.run.character = 1;
    g_GameManager.run.difficulty = 0;
    g_GameManager.run.stage = 5;
    M_Frame();
    CHECK_EQ_INT(m_MsgStarted, 13);
    M_Frame();
    CHECK_EQ_INT(m_MsgStarted, 12);
    M_End();
}

TEST(wait_msg_holds_the_timeline_and_the_progress)
{
    M_Begin("wait_msg");
    M_Entry(5, ECL_TIMELINE_WAIT_MSG, 0, m_Pos, -1, -1, -1);
    M_Entry(5, ECL_TIMELINE_SPAWN, 0, m_Pos, -1, -1, -1);
    M_Load();
    m_MsgHolds = true;
    M_Frames(8);
    CHECK_EQ_INT(M_ActiveCount(), 0);
    CHECK_EQ_INT(g_Enemies.timeline.time.current, 5);
    CHECK_EQ_INT(g_GameManager.run.progress_frames, 5);
    m_MsgHolds = false;
    M_Frame();
    CHECK_EQ_INT(M_ActiveCount(), 1);
    CHECK_EQ_INT(g_Enemies.timeline.time.current, 6);
    CHECK_EQ_INT(g_GameManager.run.progress_frames, 6);
    M_End();
}

TEST(progress_stops_while_a_dialogue_runs)
{
    M_Begin("progress");
    M_Load();
    m_MsgRunning = true;
    M_Frames(3);
    CHECK_EQ_INT(g_GameManager.run.progress_frames, 0);
    CHECK_EQ_INT(g_Enemies.timeline.time.current, 3);
    m_MsgRunning = false;
    M_Frame();
    CHECK_EQ_INT(g_GameManager.run.progress_frames, 1);
    M_End();
}

TEST(interrupt_boss_sets_the_pending_interrupt_of_the_boss)
{
    M_Begin("interrupt_boss");
    const uint32_t args[5] = { 2, 3, 0, 0, 0 };
    EclBuilder_Entry(&m_Builder, 0, 0, ECL_TIMELINE_INTERRUPT_BOSS, 28, args);
    M_Load();
    ENEMY boss = {};
    boss.script.pending_interrupt = -1;
    g_Enemies.bosses[2] = &boss;
    g_Gui.boss.present = true;
    M_Frame();
    CHECK_EQ_INT(boss.script.pending_interrupt, 3);
    M_End();
}

TEST(wait_boss_holds_while_the_boss_is_active)
{
    M_Begin("wait_boss");
    M_Entry(0, ECL_TIMELINE_WAIT_BOSS, 1, m_Pos, -1, -1, -1);
    M_Entry(0, ECL_TIMELINE_SPAWN, 0, m_Pos, -1, -1, -1);
    M_Load();
    ENEMY boss = {};
    boss.flags.active = true;
    g_Enemies.bosses[1] = &boss;
    M_Frames(3);
    CHECK_EQ_INT(M_ActiveCount(), 0);
    CHECK_EQ_INT(g_Enemies.timeline.time.current, 0);
    boss.flags.active = false;
    M_Frame();
    CHECK_EQ_INT(M_ActiveCount(), 1);
    M_End();
}

TEST(wait_boss_does_not_hold_without_a_boss)
{
    M_Begin("wait_no_boss");
    M_Entry(0, ECL_TIMELINE_WAIT_BOSS, 1, m_Pos, -1, -1, -1);
    M_Entry(0, ECL_TIMELINE_SPAWN, 0, m_Pos, -1, -1, -1);
    M_Load();
    M_Frame();
    CHECK_EQ_INT(M_ActiveCount(), 1);
    M_End();
}

TEST(the_rank_rises_every_period_shortened_by_the_lives)
{
    M_Begin("rank");
    M_Load();
    g_GameManager.player.lives = 2;
    g_Enemies.timeline.time = (TIMER) { .previous = 1919, .current = 1920 };
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, 1);
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, 1);
    M_End();
}

TEST(the_rank_does_not_rise_before_the_first_tick)
{
    M_Begin("rank_first");
    M_Load();
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, 0);
    CHECK_EQ_INT(g_Enemies.timeline.time.current, 1);
    M_End();
}

TEST(the_rank_does_not_rise_during_a_dialogue)
{
    M_Begin("rank_msg");
    M_Load();
    m_MsgRunning = true;
    g_Enemies.timeline.time = (TIMER) { .previous = 2399, .current = 2400 };
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, 0);
    M_End();
}

// Spawning

TEST(registering_draws_the_drop_counter_and_index)
{
    M_Begin("register");
    M_Load();
    RNG expected;
    Rng_SetSeed(&expected, 99);
    CHECK_EQ_INT(g_Enemies.drops.counter, Rng_U16(&expected) % 3);
    CHECK_EQ_INT(g_Enemies.drops.index, Rng_U16(&expected) % 8);
    CHECK(Anm_GetScript(0x100 + M_SCRIPT_SMALL) != nullptr);
    CHECK(Anm_GetScript(0x100 + 10 + M_SCRIPT_SMALL) != nullptr);
    Enemy_Remove();
    CHECK(Anm_GetScript(0x100 + M_SCRIPT_SMALL) == nullptr);
    CHECK(Anm_GetScript(0x100 + 10 + M_SCRIPT_SMALL) == nullptr);
    M_End();
}

TEST(stage_3_has_no_second_sprite_file)
{
    M_Begin("register_stage3");
    M_LoadStage(3);
    CHECK(Anm_GetScript(0x100 + M_SCRIPT_SMALL) != nullptr);
    M_End();
}

TEST(spawn_takes_the_first_free_slot)
{
    M_Begin("spawn_slot");
    M_Load();
    CHECK(M_Spawn() == &g_Enemies.enemies[0]);
    CHECK(M_Spawn() == &g_Enemies.enemies[1]);
    g_Enemies.enemies[0].flags.active = false;
    CHECK(M_Spawn() == &g_Enemies.enemies[0]);
    M_End();
}

TEST(spawn_starts_from_the_template)
{
    M_Begin("spawn_template");
    M_Load();
    ENEMY *const enemy = M_Spawn();
    CHECK(enemy->flags.active && enemy->flags.shootable);
    CHECK(enemy->flags.contact_damage && enemy->flags.takes_damage);
    CHECK(enemy->hitbox_size.x == 12.0f && enemy->hitbox_size.z == 12.0f);
    CHECK_EQ_INT(enemy->life, 1);
    CHECK_EQ_INT(enemy->max_life, 1);
    CHECK_EQ_INT(enemy->score, 100);
    CHECK_EQ_INT(enemy->script.death_sub, -1);
    CHECK_EQ_INT(enemy->script.pending_interrupt, -1);
    CHECK_EQ_INT(enemy->callbacks.life, -1);
    CHECK_EQ_INT(enemy->callbacks.time, -1);
    CHECK_EQ_INT(enemy->turns.left, -1);
    CHECK(enemy->shots.rank_speed_low == -0.5f);
    CHECK(enemy->shots.rank_speed_high == 0.5f);
    CHECK_EQ_INT(enemy->script.state.frame_func, ECL_FUNC_NONE);
    CHECK_EQ_INT(enemy->vms[3].script_index, -1);
    CHECK_EQ_INT(enemy->callbacks.timer.previous, -999);
    CHECK_EQ_INT(enemy->script.state.timer.current, 1);
    M_End();
}

TEST(a_full_pool_returns_a_spare_that_never_runs)
{
    M_Begin("spawn_full");
    M_INSTR(1, M_OP_SET_LIFE, 300);
    M_Entry(0, ECL_TIMELINE_SPAWN_MIRRORED, 0, m_Pos, -1, -1, -1);
    M_Load();
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        CHECK(M_Spawn() == &g_Enemies.enemies[i]);
    }
    ENEMY *const spare = M_Spawn();
    CHECK(
        spare < g_Enemies.enemies || spare >= g_Enemies.enemies + ENEMY_COUNT);
    CHECK(!spare->flags.active);
    M_Frame();
    CHECK(spare->flags.mirrored);
    CHECK(!spare->flags.active);
    CHECK_EQ_INT(spare->life, 0);
    CHECK_EQ_INT(g_Enemies.enemies[0].life, 300);
    M_End();
}

// Updating

TEST(a_color_given_before_the_step_is_lost)
{
    M_Begin("color");
    M_INSTR(1, M_OP_START_SCRIPT, M_SCRIPT_LARGE);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    CHECK(enemy->color == 0xffffffffu);
    M_Frame();
    CHECK(enemy->vm.sprite == 0x101);
    CHECK(enemy->vm.color == 0xffffffffu);
    CHECK(enemy->color == 0xffffffffu);
    M_End();
}

TEST(a_color_given_on_the_first_frame_is_kept)
{
    M_Begin("color_first");
    m_Builder.subs[0].length = 0;
    M_INSTR(0, M_OP_START_SCRIPT, M_SCRIPT_LARGE);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    CHECK(enemy->color == 0xff00ff00u);
    M_Frame();
    CHECK(enemy->vm.color == 0xff00ff00u);
    M_End();
}

TEST(an_extra_sprite_is_freed_when_its_script_ends)
{
    M_Begin("extra_sprite");
    M_INSTR(0, M_OP_START_SPRITE_SCRIPT, 5, M_SCRIPT_SHORT);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    CHECK(enemy->vms[5].script_index >= 0);
    M_Frame();
    CHECK(enemy->vms[5].script_index >= 0);
    M_Frame();
    CHECK_EQ_INT(enemy->vms[5].script_index, -1);
    M_End();
}

TEST(an_enemy_is_on_screen_once_its_sprite_touches_the_play_area)
{
    M_Begin("on_screen");
    M_Load();
    const VEC3 pos = { -20.0f, 100.0f, 0.0f };
    ENEMY *const small = Enemy_Spawn(0, pos, -1, -2, -1);
    M_Frame();
    CHECK(small->flags.active && !small->flags.on_screen);
    Anm_StartScript(&small->vm, 0x100 + M_SCRIPT_LARGE);
    M_Frame();
    CHECK(small->flags.on_screen);
    M_End();
}

TEST(leaving_the_play_area_frees_an_enemy_that_was_on_screen)
{
    M_Begin("leave");
    M_INSTR(1, M_OP_SET_VELOCITY, M_F(-50.0f), M_F(0.0f), M_F(0.0f));
    M_INSTR(1, M_OP_SET_DEATH, ENEMY_DEATH_KEEP);
    M_INSTR(1, M_OP_SET_BOSS, 0);
    M_Load();
    ENEMY *const enemy = Enemy_Spawn(0, m_Pos, -1, -2, -1);
    M_Frame();
    CHECK(enemy->flags.on_screen);
    CHECK(g_Gui.boss.present);
    M_Frames(2);
    CHECK(enemy->flags.active);
    M_Frame();
    CHECK(!enemy->flags.active);
    CHECK(!enemy->flags.shootable);
    CHECK(!g_Gui.boss.present);
    M_End();
}

TEST(an_enemy_off_screen_from_the_start_stays)
{
    M_Begin("never_on_screen");
    M_Load();
    ENEMY *const enemy =
        Enemy_Spawn(0, (VEC3) { -100.0f, 100.0f, 0.0f }, -1, -2, -1);
    m_Damage = 5;
    M_Frames(3);
    CHECK(enemy->flags.active);
    CHECK_EQ_INT(m_DamageCalls, 0);
    M_End();
}

TEST(a_deleted_script_frees_the_enemy_and_its_orbits)
{
    M_Begin("delete");
    M_INSTR(0, M_OP_ADD_ORBIT, 0, M_F(0.0f), M_F(0.0f), M_F(1.0f), M_F(8.0f));
    EclBuilder_Instr(&m_Builder, m_Sub, 1, M_OP_DELETE, M_ALL, nullptr, 0);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    CHECK_EQ_INT(enemy->orbits.count, 1);
    M_Frame();
    CHECK(!enemy->flags.active);
    CHECK(m_Effects[0].fade_out);
    CHECK_EQ_INT(enemy->orbits.count, 0);
    M_End();
}

TEST(the_enemy_moves_by_its_velocity_before_its_script)
{
    M_Begin("move");
    M_INSTR(0, M_OP_SET_VELOCITY, M_F(1.5f), M_F(-2.0f), M_F(0.25f));
    M_Load();
    ENEMY *const enemy = M_Spawn();
    M_Frames(2);
    CHECK(enemy->move.pos.x == 103.0f);
    CHECK(enemy->move.pos.y == 96.0f);
    CHECK(enemy->move.pos.z == 0.5f);
    M_End();
}

TEST(the_position_is_clamped_to_the_bounds)
{
    M_Begin("clamp");
    M_INSTR(
        0, M_OP_SET_BOUNDS, M_F(50.0f), M_F(60.0f), M_F(150.0f), M_F(160.0f));
    M_Load();
    ENEMY *const enemy = M_Spawn();
    enemy->move.pos = (VEC3) { 10.0f, 200.0f, 0.0f };
    Enemy_ClampPos(enemy);
    CHECK(enemy->move.pos.x == 50.0f && enemy->move.pos.y == 160.0f);
    enemy->move.pos = (VEC3) { 170.0f, 20.0f, 0.0f };
    Enemy_ClampPos(enemy);
    CHECK(enemy->move.pos.x == 150.0f && enemy->move.pos.y == 60.0f);
    enemy->flags.clamp_pos = false;
    enemy->move.pos = (VEC3) { 170.0f, 20.0f, 0.0f };
    Enemy_ClampPos(enemy);
    CHECK(enemy->move.pos.x == 170.0f);
    M_End();
}

TEST(orbits_follow_the_enemy_and_widen_to_the_radius)
{
    M_Begin("orbits");
    M_INSTR(0, M_OP_ADD_ORBIT, 0, M_F(0.0f), M_F(0.0f), M_F(1.0f), M_F(0.8f));
    M_INSTR(0, M_OP_SET_VELOCITY, M_F(1.0f), M_F(0.0f), M_F(0.0f));
    M_Load();
    ENEMY *const enemy = M_Spawn();
    EFFECT *const orbit = enemy->orbits.effects[0];
    M_Frame();
    CHECK(orbit->origin.x == 101.0f);
    CHECK(orbit->radius == 0.3f);
    CHECK(orbit->angle == Angle_AddNormalize(0.0f, 0.031415928f));
    M_Frames(3);
    CHECK(orbit->radius == 0.3f + 0.3f + 0.3f);
    CHECK(orbit->origin.x == 104.0f);
    M_End();
}

// Damage

TEST(shots_deal_their_damage_and_score)
{
    M_Begin("damage");
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Damage = 23;
    M_Frame();
    CHECK_EQ_INT(enemy->life, 77);
    CHECK_EQ_INT(g_GameManager.score.value, 40);
    M_End();
}

TEST(the_damage_of_a_frame_is_capped_at_70)
{
    M_Begin("damage_cap");
    M_INSTR(0, M_OP_SET_LIFE, 1000);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Damage = 100;
    M_Frame();
    CHECK_EQ_INT(enemy->life, 930);
    CHECK_EQ_INT(g_GameManager.score.value, 140);
    M_End();
}

// Returns the damage that m_Damage deals to a fresh enemy with 1000 life
// in a frame of a running spell card, and the score it gives.
static int32_t M_SpellCardHurt(
    const int32_t damage, const bool by_bomb, const bool bombed,
    uint32_t *const score)
{
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        g_Enemies.enemies[i].flags.active = false;
    }
    ENEMY *const enemy = M_Spawn();
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.bombed = bombed;
    g_GameManager.score.value = 0;
    m_Damage = damage;
    m_ByBomb = by_bomb;
    M_Frame();
    *score = g_GameManager.score.value;
    return 1000 - enemy->life;
}

TEST(during_a_spell_card_shots_deal_a_seventh)
{
    M_Begin("spell_shots");
    M_INSTR(0, M_OP_SET_LIFE, 1000);
    M_Load();
    uint32_t score;
    CHECK_EQ_INT(M_SpellCardHurt(50, false, false, &score), 7);
    CHECK_EQ_INT(score, 100);
    CHECK_EQ_INT(M_SpellCardHurt(100, false, false, &score), 10);
    CHECK_EQ_INT(score, 140);
    CHECK_EQ_INT(M_SpellCardHurt(7, false, false, &score), 1);
    CHECK_EQ_INT(M_SpellCardHurt(8, false, false, &score), 1);
    CHECK_EQ_INT(M_SpellCardHurt(14, false, false, &score), 2);
    CHECK_EQ_INT(M_SpellCardHurt(0, false, false, &score), 0);
    CHECK_EQ_INT(M_SpellCardHurt(50, false, true, &score), 7);
    M_End();
}

TEST(during_a_spell_card_the_bomb_deals_a_third_once_bombed)
{
    M_Begin("spell_bomb");
    M_INSTR(0, M_OP_SET_LIFE, 1000);
    M_Load();
    uint32_t score;
    CHECK_EQ_INT(M_SpellCardHurt(50, true, false, &score), 0);
    CHECK_EQ_INT(score, 100);
    CHECK_EQ_INT(M_SpellCardHurt(50, true, true, &score), 16);
    CHECK_EQ_INT(M_SpellCardHurt(3, true, true, &score), 1);
    CHECK_EQ_INT(M_SpellCardHurt(4, true, true, &score), 1);
    CHECK_EQ_INT(M_SpellCardHurt(6, true, true, &score), 2);
    CHECK_EQ_INT(M_SpellCardHurt(0, true, true, &score), 0);
    M_End();
}

TEST(an_enemy_that_takes_no_damage_still_scores)
{
    M_Begin("no_damage");
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_INSTR(0, M_OP_SET_TAKES_DAMAGE, 0);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Damage = 30;
    M_Frame();
    CHECK_EQ_INT(enemy->life, 100);
    CHECK_EQ_INT(g_GameManager.score.value, 60);
    M_End();
}

TEST(contact_costs_10_life_even_without_damage)
{
    M_Begin("contact");
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_INSTR(0, M_OP_SET_TAKES_DAMAGE, 0);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Contact = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK_EQ_INT(enemy->life, 90);
    CHECK(m_ContactSize.x == 12.0f * (1.0f / 1.5f));
    m_Contact = PLAYER_CONTACT_CLEARED;
    M_Frame();
    CHECK_EQ_INT(enemy->life, 90);
    M_End();
}

TEST(contact_costs_a_boss_nothing)
{
    M_Begin("contact_boss");
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Contact = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK_EQ_INT(enemy->life, 100);
    M_End();
}

TEST(contact_damage_can_be_turned_off)
{
    M_Begin("contact_off");
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_INSTR(0, M_OP_SET_CONTACT_DAMAGE, 0);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Contact = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK_EQ_INT(enemy->life, 100);
    M_End();
}

TEST(an_enemy_that_cannot_be_shot_is_not_damaged)
{
    M_Begin("unshootable");
    M_INSTR(0, M_OP_SET_SHOOTABLE, 0);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Damage = 30;
    m_Contact = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK_EQ_INT(m_DamageCalls, 0);
    CHECK_EQ_INT(enemy->life, 1);
    CHECK(enemy->flags.active);
    M_End();
}

TEST(a_hidden_enemy_is_not_damaged)
{
    M_Begin("hidden");
    M_INSTR(0, M_OP_SET_HIDDEN, 1);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Damage = 30;
    M_Frame();
    CHECK_EQ_INT(m_DamageCalls, 0);
    CHECK(enemy->flags.active);
    M_End();
}

TEST(the_homing_target_is_the_lowest_enemy)
{
    M_Begin("homing");
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_Load();
    Enemy_Spawn(0, (VEC3) { 10.0f, 100.0f, 0.0f }, -1, -2, -1);
    Enemy_Spawn(0, (VEC3) { 20.0f, 300.0f, 0.0f }, -1, -2, -1);
    Enemy_Spawn(0, (VEC3) { 30.0f, 200.0f, 0.0f }, -1, -2, -1);
    M_Frame();
    CHECK(g_Player.homing_target.x == 20.0f);
    CHECK(g_Player.homing_target.y == 300.0f);
    M_End();
}

TEST(a_hit_flashes_the_sprite_for_four_frames)
{
    M_Begin("flash");
    M_INSTR(0, M_OP_SET_LIFE, 1000);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Damage = 1;
    M_Frame();
    CHECK(enemy->vm.color_op);
    CHECK_EQ_INT(enemy->hit_flash_frames, 4);
    for (int32_t i = 3; i >= 0; i--) {
        M_Frame();
        CHECK(!enemy->vm.color_op);
        CHECK_EQ_INT(enemy->hit_flash_frames, i);
    }
    M_Frame();
    CHECK(enemy->vm.color_op);
    m_Damage = 0;
    M_Frames(5);
    CHECK(!enemy->vm.color_op);
    M_End();
}

// Death

TEST(death_remove_scores_frees_and_spawns_the_effects)
{
    M_Begin("death_remove");
    M_INSTR(0, M_OP_SET_DEATH_EFFECTS, 2 | 1 << 8);
    M_Load();
    ENEMY *const enemy = Enemy_Spawn(0, m_Pos, -1, -2, 500);
    m_Damage = 5;
    M_Frame();
    CHECK(!enemy->flags.active);
    CHECK_EQ_INT(enemy->life, 0);
    CHECK_EQ_INT(g_GameManager.score.value, 510);
    CHECK_EQ_INT(m_EffectCount, 2);
    CHECK_EQ_INT(m_EffectCalls[0].type, EFFECT_EXPLOSION_LARGE);
    CHECK_EQ_INT(m_EffectCalls[0].count, 1);
    CHECK(m_EffectCalls[0].color == 0xffffffffu);
    CHECK_EQ_INT(m_EffectCalls[1].type, EFFECT_PARTICLE + 1);
    CHECK_EQ_INT(m_EffectCalls[1].count, 4);
    CHECK_EQ_INT(m_ItemCount, 0);
    M_End();
}

TEST(death_keep_scores_and_keeps_the_enemy_unshootable)
{
    M_Begin("death_keep");
    M_INSTR(0, M_OP_SET_DEATH, ENEMY_DEATH_KEEP);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Damage = 5;
    M_Frame();
    CHECK(enemy->flags.active);
    CHECK(!enemy->flags.shootable);
    CHECK_EQ_INT(g_GameManager.score.value, 110);
    M_Frame();
    CHECK_EQ_INT(m_EffectCount, 2);
    M_End();
}

TEST(death_drop_keeps_the_enemy_without_score)
{
    M_Begin("death_drop");
    M_INSTR(0, M_OP_SET_DEATH, ENEMY_DEATH_DROP);
    M_Load();
    ENEMY *const enemy = Enemy_Spawn(0, m_Pos, -1, ITEM_BOMB, -1);
    m_Damage = 5;
    M_Frame();
    CHECK(enemy->flags.active && enemy->flags.shootable);
    CHECK_EQ_INT(g_GameManager.score.value, 10);
    CHECK_EQ_INT(m_ItemCount, 1);
    CHECK_EQ_INT(m_ItemCalls[0].type, ITEM_BOMB);
    M_End();
}

TEST(death_survive_leaves_one_life_and_hides_the_boss_bar)
{
    M_Begin("death_survive");
    M_INSTR(0, M_OP_SET_DEATH, ENEMY_DEATH_SURVIVE);
    M_INSTR(0, M_OP_SET_DEATH_EFFECTS, 1);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    M_Load();
    ENEMY *const enemy = Enemy_Spawn(0, m_Pos, -1, ITEM_BOMB, -1);
    m_Damage = 5;
    M_Frame();
    CHECK(enemy->flags.active && enemy->flags.shootable);
    CHECK_EQ_INT(enemy->life, 1);
    CHECK(!enemy->flags.takes_damage);
    CHECK_EQ_INT(enemy->flags.death, ENEMY_DEATH_REMOVE);
    CHECK(!g_Gui.boss.present);
    CHECK_EQ_INT(g_GameManager.score.value, 10);
    CHECK_EQ_INT(m_ItemCount, 0);
    CHECK_EQ_INT(m_EffectCount, 5);
    for (int32_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(m_EffectCalls[i].type, EFFECT_EXPLOSION_SPIN);
    }
    CHECK_EQ_INT(m_EffectCalls[4].type, EFFECT_PARTICLE);
    M_End();
}

TEST(a_death_mode_above_3_does_nothing_but_the_effects)
{
    M_Begin("death_other");
    M_INSTR(0, M_OP_SET_DEATH, 5);
    M_Load();
    ENEMY *const enemy = Enemy_Spawn(0, m_Pos, -1, ITEM_BOMB, -1);
    m_Damage = 5;
    M_Frame();
    CHECK(enemy->flags.active && enemy->flags.shootable);
    CHECK_EQ_INT(enemy->life, -4);
    CHECK_EQ_INT(m_ItemCount, 0);
    CHECK_EQ_INT(m_EffectCount, 2);
    M_End();
}

TEST(death_stops_the_callbacks_and_starts_the_death_sub)
{
    M_Begin("death_sub");
    M_INSTR(0, M_OP_SET_DEATH, ENEMY_DEATH_KEEP);
    M_INSTR(0, M_OP_SET_DEATH_SUB, 1);
    M_INSTR(0, M_OP_SET_TIME_CALLBACK, 100);
    M_INSTR(0, M_OP_SET_LIFE_CALLBACK, 0);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    enemy->shots.rank_speed_low = 3.0f;
    enemy->shots.rank_rows_high = 4;
    enemy->script.depth = 2;
    m_Damage = 5;
    M_Frame();
    CHECK_EQ_INT(enemy->callbacks.life, -1);
    CHECK_EQ_INT(enemy->callbacks.time, -1);
    CHECK_EQ_INT(enemy->script.state.sub, 1);
    CHECK_EQ_INT(enemy->script.state.timer.current, 0);
    CHECK_EQ_INT(enemy->script.death_sub, -1);
    CHECK_EQ_INT(enemy->script.depth, 0);
    CHECK(enemy->shots.rank_speed_low == -0.5f);
    CHECK_EQ_INT(enemy->shots.rank_rows_high, 0);
    M_End();
}

TEST(an_own_item_drops_with_particles)
{
    M_Begin("item");
    M_INSTR(0, M_OP_SET_DEATH_EFFECTS, 2 << 8);
    M_Load();
    Enemy_Spawn(0, m_Pos, -1, ITEM_POINT, -1);
    m_Damage = 5;
    M_Frame();
    CHECK_EQ_INT(m_ItemCount, 1);
    CHECK_EQ_INT(m_ItemCalls[0].type, ITEM_POINT);
    CHECK_EQ_INT(m_ItemCalls[0].state, ITEM_FALLING);
    CHECK(m_ItemCalls[0].pos.x == 100.0f);
    CHECK_EQ_INT(m_EffectCount, 3);
    CHECK_EQ_INT(m_EffectCalls[0].type, EFFECT_PARTICLE + 2);
    CHECK_EQ_INT(m_EffectCalls[0].count, 3);
    M_End();
}

TEST(an_item_dropped_by_the_bomb_homes)
{
    M_Begin("item_bomb");
    M_Load();
    Enemy_Spawn(0, m_Pos, -1, ITEM_POWER, -1);
    m_Damage = 5;
    m_ByBomb = true;
    M_Frame();
    CHECK_EQ_INT(m_ItemCount, 1);
    CHECK_EQ_INT(m_ItemCalls[0].state, ITEM_HOMING);
    M_End();
}

TEST(every_third_enemy_drops_from_the_table)
{
    M_Begin("drops");
    M_Load();
    g_Enemies.drops.counter = 0;
    g_Enemies.drops.index = 31;
    m_Damage = 5;
    const ITEM_TYPE expected[] = { ITEM_BIG_POWER, ITEM_POWER, ITEM_POWER };
    int32_t drops = 0;
    for (int32_t i = 0; i < 7; i++) {
        m_EffectCount = 0;
        Enemy_Spawn(0, m_Pos, -1, -1, -1);
        M_Frame();
        if (i % 3 == 0) {
            CHECK_EQ_INT(m_ItemCount, drops + 1);
            CHECK_EQ_INT(m_ItemCalls[drops].type, expected[drops]);
            CHECK_EQ_INT(m_EffectCalls[0].count, 6);
            drops++;
        } else {
            CHECK_EQ_INT(m_ItemCount, drops);
        }
    }
    CHECK_EQ_INT(g_Enemies.drops.counter, 7);
    CHECK_EQ_INT(g_Enemies.drops.index, 2);
    M_End();
}

TEST(an_item_below_minus_1_drops_nothing_and_counts_nothing)
{
    M_Begin("drops_none");
    M_Load();
    g_Enemies.drops.counter = 0;
    Enemy_Spawn(0, m_Pos, -1, -2, -1);
    m_Damage = 5;
    M_Frame();
    CHECK_EQ_INT(m_ItemCount, 0);
    CHECK_EQ_INT(g_Enemies.drops.counter, 0);
    M_End();
}

TEST(a_boss_death_clears_the_bullets_and_its_orbits)
{
    M_Begin("boss_death");
    M_INSTR(0, M_OP_SET_BOSS, 3);
    M_INSTR(0, M_OP_ADD_ORBIT, 0, M_F(0.0f), M_F(0.0f), M_F(1.0f), M_F(8.0f));
    M_Load();
    ENEMY *const enemy = M_Spawn();
    m_Damage = 5;
    M_Frame();
    CHECK(!g_Gui.boss.present);
    CHECK_EQ_INT(m_ClearWithBonus, 1);
    CHECK_EQ_INT(m_ClearMaxBonus, 0x3200);
    CHECK(!m_ClearItems);
    CHECK(m_Effects[0].fade_out);
    CHECK_EQ_INT(enemy->orbits.count, 0);
    M_End();
}

TEST(a_boss_death_during_a_spell_card_keeps_the_bullets)
{
    M_Begin("boss_death_spell");
    M_INSTR(0, M_OP_SET_BOSS, 0);
    M_Load();
    M_Spawn();
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    m_Damage = 50;
    M_Frame();
    CHECK_EQ_INT(m_ClearWithBonus, 0);
    M_End();
}

TEST(the_boss_bar_shows_the_life_outside_dialogue)
{
    M_Begin("boss_health");
    M_INSTR(0, M_OP_SET_LIFE, 300);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    M_Load();
    M_Spawn();
    m_Damage = 30;
    M_Frame();
    CHECK(g_Gui.boss.health == 270.0f / 300.0f);
    m_MsgRunning = true;
    M_Frame();
    CHECK(g_Gui.boss.health == 270.0f / 300.0f);
    M_End();
}

// Callbacks

TEST(the_life_callback_restores_the_life_and_kills_the_others)
{
    M_Begin("life_callback");
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_INSTR(0, M_OP_SET_LIFE_CALLBACK, 50);
    M_INSTR(0, M_OP_SET_LIFE_CALLBACK_SUB, 1);
    M_INSTR(0, M_OP_SET_DEATH_SUB, 2);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, M_OP_START_SCRIPT, M_SCRIPT_SMALL);
    M_INSTR(0, M_OP_SET_SHOOTABLE, 0);
    M_INSTR(0, M_OP_SET_DEATH_SUB, 1);
    M_Load();
    ENEMY *const boss = M_Spawn();
    ENEMY *const other = Enemy_Spawn(2, m_Pos, -1, -2, -1);
    m_Damage = 60;
    M_Frame();
    CHECK_EQ_INT(boss->life, 40);
    m_Damage = 0;
    M_Frame();
    CHECK_EQ_INT(boss->life, 50);
    CHECK_EQ_INT(boss->callbacks.life, -1);
    CHECK_EQ_INT(boss->callbacks.time_sub, 2);
    CHECK_EQ_INT(boss->script.state.sub, 1);
    CHECK_EQ_INT(other->life, 0);
    CHECK_EQ_INT(other->script.state.sub, 1);
    CHECK_EQ_INT(other->script.death_sub, -1);
    M_End();
}

TEST(the_timer_callback_ends_the_spell_card_and_clears_the_bullets)
{
    M_Begin("time_callback");
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_INSTR(0, M_OP_SET_LIFE_CALLBACK, 80);
    M_INSTR(0, M_OP_SET_TIME_CALLBACK, 3);
    M_INSTR(0, M_OP_SET_TIME_CALLBACK_SUB, 1);
    M_INSTR(0, M_OP_SET_DEATH_SUB, 2);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_INSTR(0, M_OP_START_SCRIPT, M_SCRIPT_SMALL);
    M_INSTR(0, M_OP_SET_LIFE, 1000);
    M_Load();
    ENEMY *const boss = M_Spawn();
    ENEMY *const other = Enemy_Spawn(2, m_Pos, -1, -2, -1);
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.capturable = true;
    m_Damage = 5;
    M_Frames(3);
    CHECK_EQ_INT(boss->script.state.sub, 0);
    CHECK_EQ_INT(g_Gui.boss.seconds, 0);
    m_Damage = 0;
    M_Frame();
    CHECK_EQ_INT(boss->life, 80);
    CHECK_EQ_INT(boss->callbacks.life, -1);
    CHECK_EQ_INT(boss->script.state.sub, 1);
    CHECK_EQ_INT(boss->callbacks.time, -1);
    CHECK_EQ_INT(boss->callbacks.time_sub, 2);
    CHECK_EQ_INT(boss->callbacks.timer.current, 1);
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_TIMED_OUT);
    CHECK(!g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(m_ClearAll, 1);
    CHECK(!other->flags.active);
    M_End();
}

TEST(a_second_timeout_counts_past_timed_out)
{
    M_Begin("time_callback_twice");
    M_INSTR(0, M_OP_SET_TIME_CALLBACK, 0);
    M_INSTR(0, M_OP_SET_TIME_CALLBACK_SUB, 0);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    M_Load();
    M_Spawn();
    g_Enemies.spell_card.state = ENEMY_SPELL_TIMED_OUT;
    M_Frame();
    CHECK_EQ_INT(g_Enemies.spell_card.state, 3);
    M_End();
}

TEST(the_timer_callback_outside_a_spell_card_keeps_its_state)
{
    M_Begin("time_callback_none");
    M_INSTR(0, M_OP_SET_TIME_CALLBACK, 0);
    M_INSTR(0, M_OP_SET_TIME_CALLBACK_SUB, 0);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    M_Load();
    M_Spawn();
    M_Frame();
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_NONE);
    CHECK_EQ_INT(m_ClearAll, 1);
    M_End();
}

TEST(keep_spell_card_leaves_the_spell_card_and_the_bullets)
{
    M_Begin("keep_spell_card");
    M_INSTR(0, M_OP_SET_TIME_CALLBACK, 0);
    M_INSTR(0, M_OP_SET_TIME_CALLBACK_SUB, 0);
    M_INSTR(0, M_OP_SET_KEEP_SPELL_CARD, 1);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    M_Load();
    M_Spawn();
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.capturable = true;
    M_Frame();
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_RUNNING);
    CHECK(g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(m_ClearAll, 0);
    M_End();
}

TEST(the_boss_timer_shows_the_seconds_left)
{
    M_Begin("boss_seconds");
    M_INSTR(0, M_OP_SET_TIME_CALLBACK, 661);
    M_INSTR(0, M_OP_SET_BOSS, 0);
    M_Load();
    M_Spawn();
    M_Frame();
    CHECK_EQ_INT(g_Gui.boss.seconds, 11);
    M_Frames(2);
    CHECK_EQ_INT(g_Gui.boss.seconds, 10);
    M_End();
}

TEST(a_frozen_game_stops_only_the_callback_timer)
{
    M_Begin("frozen");
    M_INSTR(0, M_OP_SET_VELOCITY, M_F(1.0f), M_F(0.0f), M_F(0.0f));
    M_INSTR(0, M_OP_SET_TIME_CALLBACK, 2);
    M_INSTR(0, M_OP_SET_TIME_CALLBACK_SUB, 0);
    M_INSTR(0, M_OP_SET_LIFE, 100);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    g_GameManager.mode.frozen = true;
    m_Damage = 1;
    M_Frames(5);
    CHECK(enemy->move.pos.x == 105.0f);
    CHECK_EQ_INT(enemy->script.state.timer.current, 6);
    CHECK_EQ_INT(enemy->life, 95);
    CHECK_EQ_INT(enemy->callbacks.timer.current, 0);
    CHECK_EQ_INT(enemy->callbacks.time, 2);
    CHECK_EQ_INT(g_Enemies.timeline.time.current, 5);
    g_GameManager.mode.frozen = false;
    M_Frame();
    CHECK_EQ_INT(enemy->callbacks.timer.current, 1);
    M_End();
}

TEST(kill_all_spares_bosses_and_starts_the_death_subs_of_the_unshootable)
{
    M_Begin("kill_all");
    m_Sub = EclBuilder_BeginSub(&m_Builder);
    M_Load();
    ENEMY *const boss = M_Spawn();
    boss->flags.boss = true;
    ENEMY *const shootable = M_Spawn();
    shootable->script.death_sub = 1;
    ENEMY *const unshootable = M_Spawn();
    unshootable->flags.shootable = false;
    unshootable->script.death_sub = 1;
    ENEMY *const inactive = M_Spawn();
    inactive->flags.active = false;
    Enemy_KillAll();
    CHECK_EQ_INT(boss->life, 1);
    CHECK_EQ_INT(shootable->life, 0);
    CHECK_EQ_INT(shootable->script.death_sub, 1);
    CHECK_EQ_INT(shootable->script.state.sub, 0);
    CHECK_EQ_INT(unshootable->life, 0);
    CHECK_EQ_INT(unshootable->script.death_sub, -1);
    CHECK_EQ_INT(unshootable->script.state.sub, 1);
    CHECK_EQ_INT(unshootable->script.state.timer.current, 0);
    CHECK_EQ_INT(inactive->life, 1);
    M_End();
}

// Drawing

TEST(drawing_places_and_turns_the_sprites)
{
    M_Begin("draw");
    M_INSTR(0, M_OP_SET_ROTATE_SPRITE, 1);
    M_INSTR(0, M_OP_START_SPRITE_SCRIPT, 6, M_SCRIPT_SMALL);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    enemy->move.angle = 0.5f;
    enemy->vm.pos_offset = (VEC3) { 1.0f, 2.0f, 3.0f };
    enemy->vms[6].auto_rotate = 1;
    Chain_RunDraw(&g_Chain);
    CHECK(enemy->vm.rotation.z == 0.5f);
    CHECK(enemy->vm.pos.x == 101.0f && enemy->vm.pos.y == 102.0f);
    CHECK(enemy->vm.pos.z == 0.494f);
    CHECK(enemy->vms[6].rotation.z == 0.5f);
    CHECK(enemy->vms[6].pos.z == 0.495f);
    CHECK(enemy->vms[2].pos.x == 0.0f);
    M_End();
}

TEST(drawing_skips_a_hidden_enemy)
{
    M_Begin("draw_hidden");
    M_INSTR(0, M_OP_SET_HIDDEN, 1);
    M_Load();
    ENEMY *const enemy = M_Spawn();
    Chain_RunDraw(&g_Chain);
    CHECK(enemy->vm.pos.x == 0.0f);
    M_End();
}

TEST(enemy_draws_do_not_share_copied_template_identities)
{
    M_Begin("enemy_presentation");
    M_Load();
    CHECK(IS_OK(Anm_CreateTexture(100, 256, 256, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = { .slot = 100, .width = 16, .height = 16 };
    ENEMY *const first = M_Spawn();
    ENEMY *const second = M_Spawn();
    first->vm.sprite_data = &sprite;
    second->vm.sprite_data = &sprite;
    first->vm.shown = second->vm.shown = true;
    first->vm.visible = second->vm.visible = true;
    first->vm.color = second->vm.color = 0xffffffff;
    first->vm.scale_x = second->vm.scale_x = 1;
    first->vm.scale_y = second->vm.scale_y = 1;
    first->vm.uv_scale_x = second->vm.uv_scale_x = 0.0625f;
    first->vm.uv_scale_y = second->vm.uv_scale_y = 0.0625f;
    g_GameManager.camera.viewport = (VIEWPORT) { 0, 0, 640, 480, 0, 1 };
    g_GameManager.camera.view = Mat4_Identity();
    g_GameManager.camera.projection = Mat4_Identity();
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    CHECK(GfxRecorder_QuadId(0) != 0);
    CHECK(GfxRecorder_QuadId(0) != GfxRecorder_QuadId(1));
    const uint32_t first_generation = first->vm.presentation_generation;
    Enemy_RenewPresentation(first);
    CHECK(first->vm.presentation_generation != first_generation);
    GfxRecorder_Stop();
    M_End();
}
