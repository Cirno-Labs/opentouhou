#include "th07/enemy.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/item.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/sound.h"
#include "th07/stubs.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include "stg/chain.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_SPRITE 0x900
#define M_SPRITE_SIZE 32.0f
#define M_SEED 77
#define M_CHERRY_OFFSET 1000
#define M_MAX_CALLS 64
// The fake damage test answers with the extra damage for a box this wide.
#define M_EXTRA_SIZE 30.0f
#define M_END { .time = -1 }
#define M_ENTRY(time_, opcode_, arg_)                                          \
    { .time = (time_), .opcode = (opcode_), .arg = (arg_) }
#define M_SPAWN(opcode_, x_, y_, z_, life_, item_, score_)                     \
    {                                                                          \
        .opcode = (opcode_), .arg = 1, .args.spawn = {                         \
            { (x_), (y_), (z_) },                                              \
            (life_),                                                           \
            (item_),                                                           \
            (score_)                                                           \
        }                                                                      \
    }

// The test links these instead of the modules they stand for. The script
// of an enemy is a per-test callback; the others record their calls.

typedef ECL_RUN (*M_RUN)(ENEMY *enemy);

typedef struct {
    VEC3 pos;
    ITEM_TYPE type;
    ITEM_STATE state;
} M_ITEM_CALL;

typedef struct {
    VEC3 pos;
    int32_t value;
    uint32_t color;
} M_POPUP_CALL;

typedef struct {
    EFFECT_TYPE type;
    int32_t count;
} M_EFFECT_CALL;

typedef struct {
    VEC3 pos;
    VEC3 size;
} M_CONTACT_CALL;

RNG g_Rng;
CHAIN g_Chain;
float g_GameSpeed;
PLAYER g_Player;
GUI g_Gui;
ASCII_TEXT g_AsciiText;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;
static M_RUN m_Run;
static const ECL_TIMELINE_ENTRY *m_Timelines[ECL_TIMELINE_COUNT];
static int32_t m_TimelineCount;
static int16_t m_Starts[M_MAX_CALLS];
static int32_t m_StartCount;
static int32_t m_RunCount;
static int32_t m_Damage;
static bool m_DamageByBomb;
static int32_t m_ExtraDamage;
static bool m_ExtraByBomb;
static int32_t m_DamageCalls;
static PLAYER_CONTACT m_Contact;
static M_CONTACT_CALL m_Hits[M_MAX_CALLS];
static int32_t m_HitCalls;
static M_CONTACT_CALL m_Grazes[M_MAX_CALLS];
static int32_t m_GrazeCalls;
static int32_t m_LaserClears;
static int32_t m_BonusClears;
static int32_t m_BonusClearMax;
static int32_t m_BulletBonus;
static M_ITEM_CALL m_Items[M_MAX_CALLS];
static int32_t m_ItemCount;
static M_POPUP_CALL m_Popups[M_MAX_CALLS];
static int32_t m_PopupCount;
static M_EFFECT_CALL m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static uint32_t m_Events;
static int32_t m_Sounds[M_MAX_CALLS];
static int32_t m_SoundCount;
static int32_t m_MsgStarted;
static bool m_MsgHolds;
static bool m_MsgRunning;
static int32_t m_BonusPopup;
// Records flags.mirrored at the first frame of each spawn.
static bool m_MirroredAtStart[M_MAX_CALLS];

int32_t Ecl_GetTimelineCount(void)
{
    return m_TimelineCount;
}

const ECL_TIMELINE_ENTRY *Ecl_GetTimeline(const int32_t index)
{
    return m_Timelines[index];
}

void Ecl_StartSub(ECL_STATE *const state, const int16_t sub)
{
    state->sub = sub;
    state->pc = 0;
    Timer_Set(&state->timer, 0);
    state->vars.ints[0] = -1;
    if (m_StartCount < M_MAX_CALLS) {
        m_Starts[m_StartCount] = sub;
    }
    m_StartCount++;
}

ECL_RUN Ecl_RunEnemy(ENEMY *const enemy)
{
    if (enemy->vm.sprite_data == nullptr) {
        Anm_InitVm(&enemy->vm);
        Anm_SetSprite(&enemy->vm, M_SPRITE);
        if (m_RunCount < M_MAX_CALLS) {
            m_MirroredAtStart[m_RunCount] = enemy->flags.mirrored;
        }
    }
    m_RunCount++;
    const ECL_RUN run = m_Run != nullptr ? m_Run(enemy) : ECL_RUN_CONTINUE;
    Timer_Tick(&enemy->script.state.timer);
    return run;
}

int32_t Player_CalcDamage(
    const VEC3 pos, const VEC3 size, bool *const hit_by_bomb)
{
    m_DamageCalls++;
    if (size.x == M_EXTRA_SIZE) {
        *hit_by_bomb = m_ExtraByBomb;
        return m_ExtraDamage;
    }
    *hit_by_bomb = m_DamageByBomb;
    return m_Damage;
}

PLAYER_CONTACT Player_CheckHit(const VEC3 pos, const VEC3 size)
{
    if (m_HitCalls < M_MAX_CALLS) {
        m_Hits[m_HitCalls] = (M_CONTACT_CALL) { pos, size };
    }
    m_HitCalls++;
    return m_Contact;
}

PLAYER_CONTACT Player_CheckGraze(const VEC3 pos, const VEC3 size)
{
    if (m_GrazeCalls < M_MAX_CALLS) {
        m_Grazes[m_GrazeCalls] = (M_CONTACT_CALL) { pos, size };
    }
    m_GrazeCalls++;
    return PLAYER_CONTACT_NONE;
}

void Player_StartBorder(void)
{
}

void Bullet_ClearAllAndKeptLasers(void)
{
    m_LaserClears++;
}

int32_t Bullet_ClearAllWithBonus(const int32_t max_bonus, const bool to_items)
{
    m_BonusClears++;
    m_BonusClearMax = max_bonus;
    return m_BulletBonus;
}

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
    if (m_ItemCount < M_MAX_CALLS) {
        m_Items[m_ItemCount] = (M_ITEM_CALL) { pos, type, state };
    }
    m_ItemCount++;
}

EFFECT *Effect_Spawn(
    const EFFECT_TYPE type, const VEC3 pos, const int32_t count,
    const uint32_t color)
{
    if (m_EffectCount < M_MAX_CALLS) {
        m_Effects[m_EffectCount] = (M_EFFECT_CALL) { type, count };
    }
    m_EffectCount++;
    return nullptr;
}

void AsciiText_AddPopup(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
    if (m_PopupCount < M_MAX_CALLS) {
        m_Popups[m_PopupCount] = (M_POPUP_CALL) { pos, value, color };
    }
    m_PopupCount++;
}

void Replay_MarkEvent(const REPLAY_EVENT event)
{
    m_Events |= event;
}

void Sound_Queue(const int32_t sound)
{
    if (m_SoundCount < M_MAX_CALLS) {
        m_Sounds[m_SoundCount] = sound;
    }
    m_SoundCount++;
}

void Gui_StartMsg(const int32_t index)
{
    m_MsgStarted = index;
}

bool Gui_MsgHoldsTimeline(void)
{
    return m_MsgHolds;
}

bool Gui_IsMsgRunning(void)
{
    return m_MsgRunning;
}

void Gui_ShowBonusPopup(const int32_t bonus)
{
    m_BonusPopup = bonus;
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
}

static void M_WriteAnm(void)
{
    TH07_ANM_BUILDER builder;
    Th07AnmBuilder_Init(&builder);
    TH07_ANM_BUILDER_ANM *const anm = Th07AnmBuilder_Add(&builder, 64, 64, "@");
    Th07AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, M_SPRITE_SIZE, M_SPRITE_SIZE);
    Th07AnmBuilder_WriteFile(&builder, m_Directory, "data/stg1enm.anm");
}

static void M_ResetCalls(void)
{
    m_Run = nullptr;
    m_TimelineCount = 0;
    m_StartCount = 0;
    m_RunCount = 0;
    m_Damage = 0;
    m_DamageByBomb = false;
    m_ExtraDamage = 0;
    m_ExtraByBomb = false;
    m_DamageCalls = 0;
    m_Contact = PLAYER_CONTACT_NONE;
    m_HitCalls = 0;
    m_GrazeCalls = 0;
    m_LaserClears = 0;
    m_BonusClears = 0;
    m_BonusClearMax = 0;
    m_BulletBonus = 0;
    m_ItemCount = 0;
    m_PopupCount = 0;
    m_EffectCount = 0;
    m_Events = 0;
    m_SoundCount = 0;
    m_MsgStarted = -1;
    m_MsgHolds = false;
    m_MsgRunning = false;
    m_BonusPopup = 0;
}

static void M_SetUp(const char *const case_name)
{
    GfxRecorder_Start();
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    CHECK(SDL_CreateDirectory(data));
    Mem_Free(data);
    M_WriteAnm();
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, M_SEED);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.player_area = (PLAY_RECT) { 8.0f, 16.0f, 368.0f, 416.0f };
    g_GameManager.run.stage = 1;
    g_GameManager.run.shot_index = 1;
    g_GameManager.cherry.offset = M_CHERRY_OFFSET;
    g_GameManager.cherry.value = M_CHERRY_OFFSET;
    g_GameManager.cherry.max = M_CHERRY_OFFSET + 200000;
    g_GameManager.player.lives = 2.0f;
    g_GameManager.rank.max = 99;
    g_GameManager.camera.viewport =
        (VIEWPORT) { 32.0f, 16.0f, 384.0f, 448.0f, 0.0f, 1.0f };
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.pos = (VEC3) { 192.0f, 400.0f, 0.0f };
    g_Player.homing_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    g_Player.aim_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    memset(&g_Gui, 0, sizeof(g_Gui));
    memset(&g_AsciiText, 0, sizeof(g_AsciiText));
    g_GameSpeed = 1.0f;
    M_ResetCalls();
    CHECK(Result_Log(Enemy_Register(), LOG_LEVEL_ERROR));
}

static void M_TearDown(void)
{
    Enemy_Remove();
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Th07Data_Unmount();
    GfxRecorder_Stop();
    Mem_Free(m_Directory);
}

static ENEMY *M_SpawnAt(const float x, const float y)
{
    return Enemy_Spawn(0, (VEC3) { x, y, 0.0f }, -1, -2, -1, false);
}

static void M_Frame(void)
{
    Chain_RunCalc(&g_Chain);
}

static ECL_RUN M_SetLife50(ENEMY *const enemy)
{
    if (enemy->script.state.timer.current == 0) {
        enemy->life = 50;
    }
    return ECL_RUN_CONTINUE;
}

static ECL_RUN M_End(ENEMY *const enemy)
{
    return ECL_RUN_DELETE;
}

TEST(register_loads_the_stage_sprites_and_draws_the_drops)
{
    M_SetUp("register");
    CHECK(Anm_GetSpriteEntry(M_SPRITE) != nullptr);
    RNG rng;
    Rng_SetSeed(&rng, M_SEED);
    const uint16_t counter = Rng_U16InRange(&rng, 3);
    const uint16_t index = Rng_U16InRange(&rng, 8);
    CHECK_EQ_INT(g_Enemies.drops.counter, counter);
    CHECK_EQ_INT(g_Enemies.drops.index, index);
    CHECK(g_AsciiText.boss_markers[3].pos.x == -999.0f);
    CHECK(g_AsciiText.boss_markers[0].pos.y == -999.0f);
    M_TearDown();
}

TEST(spawn_sets_life_before_the_first_frame_only)
{
    M_SetUp("spawn_life");
    m_Run = M_SetLife50;
    ENEMY *const enemy =
        Enemy_Spawn(3, (VEC3) { 10.0f, 20.0f, 0.0f }, 7, 0x105, 900, true);
    CHECK(enemy == &g_Enemies.enemies[0]);
    CHECK_EQ_INT(enemy->life, 50);
    CHECK_EQ_INT(enemy->max_life, 50);
    CHECK_EQ_INT(enemy->score, 900);
    CHECK_EQ_INT(enemy->death.item, 5);
    CHECK(enemy->flags.mirrored);
    CHECK_EQ_INT(m_Starts[0], 3);
    CHECK(enemy->move.pos.x == 10.0f);
    ENEMY *const other = Enemy_Spawn(3, (VEC3) {}, -1, -1, -1, false);
    CHECK(other == &g_Enemies.enemies[1]);
    CHECK_EQ_INT(other->score, 100);
    M_TearDown();
}

static ECL_RUN M_CheckVars(ENEMY *const enemy)
{
    CHECK_EQ_INT(enemy->script.state.vars.ints[0], 42);
    return M_SetLife50(enemy);
}

TEST(spawn_with_vars_sets_life_twice_and_the_vars_after_the_start)
{
    M_SetUp("spawn_vars");
    m_Run = M_CheckVars;
    ECL_VARS vars = {};
    vars.ints[0] = 42;
    vars.floats[13] = 2.5f;
    ENEMY *const enemy = Enemy_SpawnWithVars(4, (VEC3) {}, 7, -1, -1, &vars);
    CHECK_EQ_INT(enemy->life, 7);
    CHECK_EQ_INT(enemy->max_life, 7);
    CHECK(enemy->script.state.vars.floats[13] == 2.5f);
    CHECK(!enemy->flags.mirrored);
    M_TearDown();
}

TEST(spawn_whose_first_frame_ends_the_script_frees_the_slot)
{
    M_SetUp("spawn_end");
    m_Run = M_End;
    ENEMY *const enemy = Enemy_Spawn(0, (VEC3) {}, -1, 3, 500, false);
    CHECK(enemy == &g_Enemies.enemies[0]);
    CHECK(!enemy->flags.active);
    CHECK_EQ_INT(enemy->score, 100);
    CHECK_EQ_INT(enemy->max_life, 0);
    M_TearDown();
}

TEST(spawn_without_a_free_slot_returns_the_spare)
{
    M_SetUp("spawn_spare");
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        g_Enemies.enemies[i].flags.active = true;
    }
    ENEMY *const enemy = M_SpawnAt(0.0f, 0.0f);
    CHECK(
        enemy < g_Enemies.enemies || enemy >= g_Enemies.enemies + ENEMY_COUNT);
    CHECK_EQ_INT(m_RunCount, 0);
    M_TearDown();
}

TEST(timeline_mirrors_ops_2_and_3_before_the_first_frame_and_6_and_7_after)
{
    M_SetUp("timeline_mirror");
    static const ECL_TIMELINE_ENTRY timeline[] = {
        M_SPAWN(ECL_TIMELINE_SPAWN_MIRRORED, 10, 20, 0, -1, -1, -1),
        M_SPAWN(ECL_TIMELINE_SPAWN_MIRRORED_DEFAULT, 10, 20, 0, -1, -1, -1),
        M_SPAWN(ECL_TIMELINE_SPAWN_RANDOM_MIRRORED, 10, 20, 0, -1, -1, -1),
        M_SPAWN(
            ECL_TIMELINE_SPAWN_RANDOM_MIRRORED_DEFAULT, 10, 20, 0, -1, -1, -1),
        M_END,
    };
    m_Timelines[0] = timeline;
    m_TimelineCount = 1;
    M_Frame();
    CHECK(m_MirroredAtStart[0]);
    CHECK(m_MirroredAtStart[1]);
    CHECK(!m_MirroredAtStart[2]);
    CHECK(!m_MirroredAtStart[3]);
    for (int32_t i = 0; i < 4; i++) {
        CHECK(g_Enemies.enemies[i].flags.mirrored);
    }
    M_TearDown();
}

TEST(timeline_random_spawns_draw_each_axis_at_or_below_minus_990)
{
    M_SetUp("timeline_random");
    static const ECL_TIMELINE_ENTRY timeline[] = {
        M_SPAWN(ECL_TIMELINE_SPAWN_RANDOM, -990.0f, 20.0f, -991.0f, 5, -1, -1),
        M_END,
    };
    m_Timelines[0] = timeline;
    m_TimelineCount = 1;
    RNG rng = g_Rng;
    const float x = Rng_F32InRange(&rng, 368.0f);
    const float z = Rng_F32InRange(&rng, 800.0f);
    M_Frame();
    const ENEMY *const enemy = &g_Enemies.enemies[0];
    CHECK(enemy->move.prev_pos.x == x);
    CHECK(enemy->move.prev_pos.y == 20.0f);
    CHECK(enemy->move.prev_pos.z == z);
    M_TearDown();
}

TEST(timelines_hold_while_the_dialogue_or_a_boss_waits)
{
    M_SetUp("timeline_wait");
    static const ECL_TIMELINE_ENTRY first[] = {
        M_ENTRY(0, ECL_TIMELINE_WAIT_MSG, 0),
        M_ENTRY(1, ECL_TIMELINE_SPAWN_DEFAULT, 1),
        M_END,
    };
    static const ECL_TIMELINE_ENTRY second[] = {
        M_ENTRY(0, ECL_TIMELINE_WAIT_BOSS, 2),
        M_ENTRY(1, ECL_TIMELINE_SPAWN_DEFAULT, 1),
        M_END,
    };
    m_Timelines[0] = first;
    m_Timelines[1] = second;
    m_TimelineCount = 2;
    m_MsgHolds = true;
    ENEMY *const boss = M_SpawnAt(100.0f, 100.0f);
    boss->flags.hidden = true;
    m_StartCount = 0;
    g_Enemies.bosses[2] = boss;
    M_Frame();
    M_Frame();
    CHECK_EQ_INT(g_Enemies.timelines[0].time.current, 0);
    CHECK_EQ_INT(g_Enemies.timelines[1].time.current, 0);
    m_MsgHolds = false;
    M_Frame();
    CHECK_EQ_INT(g_Enemies.timelines[0].time.current, 1);
    M_Frame();
    CHECK_EQ_INT(m_StartCount, 1);
    g_Enemies.bosses[2] = nullptr;
    M_Frame();
    M_Frame();
    CHECK_EQ_INT(m_StartCount, 2);
    M_TearDown();
}

TEST(timeline_starts_dialogue_sets_power_and_interrupts_a_boss)
{
    M_SetUp("timeline_ops");
    ECL_TIMELINE_ENTRY timeline[] = {
        M_ENTRY(0, ECL_TIMELINE_START_MSG, 2),
        M_ENTRY(0, ECL_TIMELINE_SET_POWER, 64),
        { .opcode = ECL_TIMELINE_INTERRUPT_BOSS,
          .args.interrupt = { .boss = 1, .interrupt = 5 } },
        M_ENTRY(0, ECL_TIMELINE_SPAWN_DEFAULT, 1),
        M_END,
    };
    m_Timelines[0] = timeline;
    m_TimelineCount = 1;
    g_GameManager.run.character = 2;
    ENEMY *const boss = &g_Enemies.enemies[ENEMY_COUNT - 1];
    g_Enemies.bosses[1] = boss;
    g_Gui.boss.present = true;
    RNG rng = g_Rng;
    GameManager_DrawChecksumKeys();
    const uint16_t seed = g_Rng.seed;
    g_Rng = rng;
    M_Frame();
    CHECK_EQ_INT(m_MsgStarted, 22);
    CHECK(g_GameManager.player.power == 64.0f);
    CHECK_EQ_INT(g_Rng.seed, seed);
    CHECK_EQ_INT(boss->script.pending_interrupt, 5);
    CHECK_EQ_INT(m_StartCount, 0);
    M_TearDown();
}

static ECL_RUN M_SetVelocity(ENEMY *const enemy)
{
    enemy->move.velocity = (VEC3) { 2.0f, 1.0f, 4.0f };
    return ECL_RUN_CONTINUE;
}

TEST(enemies_move_at_the_game_speed_and_keep_the_last_step)
{
    M_SetUp("move");
    m_Run = M_SetVelocity;
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    ENEMY *const mirrored =
        Enemy_Spawn(0, (VEC3) { 100.0f, 100.0f, 0.0f }, -1, -2, -1, true);
    g_GameSpeed = 0.5f;
    M_Frame();
    M_Frame();
    CHECK(enemy->move.pos.x == 102.0f);
    CHECK(enemy->move.pos.y == 101.0f);
    CHECK(enemy->move.prev_pos.x == 101.0f);
    CHECK(enemy->move.step.x == 1.0f);
    CHECK(mirrored->move.pos.x == 98.0f);
    M_TearDown();
}

TEST(leaving_the_play_area_despawns_a_boss_and_marks_the_replay)
{
    M_SetUp("leave");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    enemy->flags.boss = true;
    enemy->boss_index = 1;
    enemy->flags.death = ENEMY_DEATH_KEEP;
    g_Enemies.bosses[1] = enemy;
    g_Gui.boss.present = true;
    M_Frame();
    CHECK(enemy->flags.on_screen);
    enemy->move.pos.x = 400.0f + M_SPRITE_SIZE;
    M_Frame();
    CHECK(!enemy->flags.active);
    CHECK(g_Enemies.bosses[1] == nullptr);
    CHECK(!g_Gui.boss.present);
    CHECK((m_Events & REPLAY_EVENT_ENEMY_GONE) != 0);
    M_TearDown();
}

TEST(a_trail_keeps_the_enemy_until_its_end_leaves_too)
{
    M_SetUp("trail_leave");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    enemy->trail.flags = ENEMY_TRAIL_ON;
    enemy->trail.count = 3;
    enemy->trail.step = 1;
    for (int32_t i = 0; i < 3; i++) {
        M_Frame();
    }
    enemy->move.pos.x = 500.0f;
    M_Frame();
    M_Frame();
    CHECK(enemy->flags.active);
    CHECK(enemy->trail.boxes[2].pos.x == 100.0f);
    M_Frame();
    CHECK(!enemy->flags.active);
    M_TearDown();
}

TEST(a_life_callback_restarts_the_script_and_kills_the_other_enemies)
{
    M_SetUp("life_callback");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    ENEMY *const other = M_SpawnAt(150.0f, 100.0f);
    other->flags.shootable = false;
    other->script.death_sub = 9;
    // The callback kills every enemy but the bosses, and this one too
    // otherwise.
    enemy->flags.boss = true;
    enemy->life = 100;
    enemy->callbacks.life[2] = 120;
    enemy->callbacks.life_sub[2] = 6;
    enemy->callbacks.time = 500;
    enemy->script.periodic.sub = 3;
    enemy->script.depth = 4;
    enemy->shots.interval = 9;
    m_StartCount = 0;
    const int32_t runs = m_RunCount;
    M_Frame();
    CHECK_EQ_INT(enemy->life, 120);
    CHECK_EQ_INT(enemy->callbacks.life[2], -1);
    CHECK_EQ_INT(enemy->callbacks.time, -1);
    CHECK_EQ_INT(enemy->script.periodic.sub, -1);
    CHECK_EQ_INT(enemy->script.depth, 0);
    CHECK_EQ_INT(enemy->shots.interval, 0);
    CHECK(enemy->shots.rank_speed_high == 0.5f);
    CHECK_EQ_INT(m_Starts[0], 6);
    CHECK_EQ_INT(m_Starts[1], 9);
    CHECK_EQ_INT(other->life, 0);
    // The script runs again in the frame of the callback.
    CHECK_EQ_INT(m_RunCount - runs, 3);
    M_TearDown();
}

TEST(the_timer_callback_takes_a_quarter_of_the_cherry_less_its_last_digit)
{
    M_SetUp("timer_callback");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    enemy->flags.boss = true;
    enemy->life = 1000;
    enemy->callbacks.time = 120;
    enemy->callbacks.time_sub = 4;
    enemy->callbacks.life[0] = 300;
    enemy->callbacks.life[1] = 200;
    enemy->script.death_sub = 8;
    Timer_Set(&enemy->callbacks.timer, 0);
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.bonus = 5000;
    g_GameManager.cherry.value = M_CHERRY_OFFSET + 12345;
    M_Frame();
    CHECK_EQ_INT(g_Gui.boss.seconds, 2);
    enemy->callbacks.timer.current = 120;
    m_StartCount = 0;
    M_Frame();
    CHECK_EQ_INT(m_Starts[0], 4);
    CHECK_EQ_INT(enemy->life, 300);
    CHECK_EQ_INT(enemy->callbacks.life[0], -1);
    CHECK_EQ_INT(enemy->callbacks.life[1], 200);
    CHECK_EQ_INT(enemy->callbacks.time_sub, 8);
    CHECK_EQ_INT(g_Enemies.spell_card.state, ENEMY_SPELL_TIMED_OUT);
    CHECK(!g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(g_Enemies.spell_card.bonus, 0);
    CHECK_EQ_INT(m_LaserClears, 1);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET + 12345 - 3080);
    M_TearDown();
}

TEST(halted_enemies_hold_their_timer_and_flash_by_the_hit_before_them)
{
    M_SetUp("halted");
    ENEMY *const hit = M_SpawnAt(100.0f, 100.0f);
    ENEMY *const halted = M_SpawnAt(150.0f, 100.0f);
    halted->flags.halted_by_bomb = true;
    M_Frame();
    const int32_t timer = halted->callbacks.timer.current;
    hit->life = 1000;
    g_Player.state.value = PLAYER_DYING;
    m_Damage = 5;
    const int32_t runs = m_RunCount;
    M_Frame();
    CHECK_EQ_INT(m_RunCount - runs, 1);
    CHECK_EQ_INT(halted->callbacks.timer.current, timer);
    CHECK(hit->vm.use_color2);
    CHECK(halted->vm.use_color2);
    CHECK_EQ_INT(hit->vm.color2 & 0xffffffu, 0xff80c0u);
    M_TearDown();
}

TEST(enemies_go_to_the_draw_list_they_name_last_first)
{
    M_SetUp("draw_lists");
    ENEMY *const low = M_SpawnAt(100.0f, 100.0f);
    ENEMY *const high = M_SpawnAt(150.0f, 100.0f);
    ENEMY *const low2 = M_SpawnAt(200.0f, 100.0f);
    ENEMY *const hidden = M_SpawnAt(200.0f, 100.0f);
    high->draw_list = ENEMY_DRAW_HIGH;
    hidden->flags.hidden = true;
    M_Frame();
    CHECK(g_Enemies.draw_lists[ENEMY_DRAW_LOW] == low2);
    CHECK(low2->next_drawn == low);
    CHECK(low->next_drawn == nullptr);
    CHECK(g_Enemies.draw_lists[ENEMY_DRAW_HIGH] == high);
    CHECK(g_Enemies.draw_lists[1] == nullptr);
    M_TearDown();
}

TEST(removed_enemies_score_and_drop_their_item)
{
    M_SetUp("death_remove");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    enemy->score = 5000;
    enemy->death.item = ITEM_BOMB;
    enemy->death.effect = 1;
    enemy->death.particle_color = 2;
    m_Damage = 1;
    m_DamageByBomb = true;
    M_Frame();
    CHECK(!enemy->flags.active);
    CHECK_EQ_INT(g_GameManager.score.value, 500);
    CHECK_EQ_INT(m_ItemCount, 1);
    CHECK_EQ_INT(m_Items[0].type, ITEM_BOMB);
    CHECK_EQ_INT(m_Items[0].state, ITEM_HOMING);
    CHECK_EQ_INT(m_EffectCount, 3);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_PARTICLE + 2);
    CHECK_EQ_INT(m_Effects[1].type, 1);
    CHECK_EQ_INT(m_Effects[2].count, 4);
    CHECK_EQ_INT(m_Sounds[0], SOUND_ENEMY_DEATH_0);
    M_TearDown();
}

TEST(surviving_enemies_spawn_their_effect_four_times_and_still_sound)
{
    M_SetUp("death_survive");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    ENEMY *const second = M_SpawnAt(100.0f, 100.0f);
    second->flags.death = ENEMY_DEATH_SURVIVE;
    second->death.effect = 2;
    g_Gui.boss.present = true;
    enemy->life = 100;
    m_Damage = 1;
    M_Frame();
    CHECK(second->flags.active);
    CHECK_EQ_INT(second->life, 1);
    CHECK(!second->flags.takes_damage);
    CHECK_EQ_INT(second->flags.death, ENEMY_DEATH_REMOVE);
    CHECK(!g_Gui.boss.present);
    CHECK_EQ_INT(m_EffectCount, 5);
    CHECK_EQ_INT(m_Effects[3].type, 2);
    CHECK_EQ_INT(m_Effects[4].type, EFFECT_PARTICLE);
    CHECK_EQ_INT(m_Sounds[1], SOUND_ENEMY_DEATH_1);
    CHECK((m_Events & REPLAY_EVENT_ENEMY_GONE) != 0);
    M_TearDown();
}

TEST(the_drop_table_drops_for_every_third_enemy)
{
    M_SetUp("drop_table");
    g_Enemies.drops.counter = 0;
    g_Enemies.drops.index = 7;
    for (int32_t i = 0; i < 4; i++) {
        ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
        enemy->death.item = -1;
        enemy->death.effect = -1;
    }
    m_Damage = 1;
    M_Frame();
    CHECK_EQ_INT(m_ItemCount, 2);
    CHECK_EQ_INT(m_Items[0].type, ITEM_CHERRY);
    CHECK_EQ_INT(m_Items[1].type, ITEM_POINT);
    CHECK_EQ_INT(m_Items[0].state, ITEM_FALLING);
    CHECK_EQ_INT(g_Enemies.drops.counter, 4);
    CHECK_EQ_INT(g_Enemies.drops.index, 9);
    CHECK_EQ_INT(m_Effects[0].count, 6);
    M_TearDown();
}

TEST(bonus_clears_start_at_2000_even_above_the_maximum)
{
    M_SetUp("clear_bonus");
    ENEMY *const boss = M_SpawnAt(100.0f, 100.0f);
    boss->flags.boss = true;
    ENEMY *const grazeable = M_SpawnAt(50.0f, 60.0f);
    grazeable->flags.grazeable = true;
    grazeable->trail.flags = ENEMY_TRAIL_ON;
    grazeable->trail.count = 12;
    ENEMY *const plain = M_SpawnAt(50.0f, 60.0f);
    plain->flags.shootable = false;
    plain->script.death_sub = 12;
    m_StartCount = 0;
    CHECK_EQ_INT(Enemy_ClearForBonus(0, 5), 5 + 2000);
    CHECK_EQ_INT(m_PopupCount, 3);
    CHECK_EQ_INT(m_Popups[0].value, 2000);
    CHECK_EQ_INT(m_Popups[0].color, 0xffffff00u);
    CHECK_EQ_INT(m_Popups[1].value, 0);
    CHECK_EQ_INT(m_ItemCount, 3);
    CHECK_EQ_INT(m_Items[2].type, ITEM_STAR);
    CHECK_EQ_INT(m_Items[2].state, ITEM_HOMING);
    CHECK_EQ_INT(boss->life, 1);
    CHECK_EQ_INT(grazeable->life, 0);
    CHECK_EQ_INT(m_Starts[0], 12);
    CHECK_EQ_INT(Enemy_ClearForBonus(2050, 0), 2000 + 2030 + 2050);
    CHECK_EQ_INT(m_Popups[4].color, 0xffffffffu);
    CHECK_EQ_INT(m_Popups[5].color, 0xffffff00u);
    M_TearDown();
}

TEST(a_boss_slot_makes_a_boss_active)
{
    M_SetUp("boss_active");
    CHECK(!Enemy_IsBossActive());
    g_Enemies.bosses[7] = &g_Enemies.enemies[3];
    CHECK(Enemy_IsBossActive());
    M_TearDown();
}

// Updates and draws a frame. Returns the quads drawn.
static int32_t M_DrawFrame(void)
{
    M_Frame();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    return GfxRecorder_QuadCount();
}

TEST(trails_draw_a_copy_on_every_step_th_box)
{
    M_SetUp("trail_copies");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    enemy->trail.flags = ENEMY_TRAIL_ON | ENEMY_TRAIL_FADE;
    enemy->trail.count = 5;
    enemy->trail.step = 2;
    M_DrawFrame();
    M_DrawFrame();
    // The head and the boxes at 2 and 4, while those are on the trail.
    CHECK_EQ_INT(M_DrawFrame(), 2);
    M_DrawFrame();
    CHECK_EQ_INT(M_DrawFrame(), 3);
    const GFX_RECORDED_QUAD *const last = GfxRecorder_Quad(2);
    CHECK_EQ_INT(last->vertices[0].color.alpha, 0xff - 0xff * 4 / 5);
    CHECK_EQ_INT(enemy->vm.color >> 24, 0xff);
    enemy->trail.flags |= ENEMY_TRAIL_HIDE_HEAD;
    CHECK_EQ_INT(M_DrawFrame(), 2);
    M_TearDown();
}

TEST(a_strip_runs_through_the_boxes_up_to_the_first_gone_one)
{
    M_SetUp("trail_strip");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    enemy->trail.flags = ENEMY_TRAIL_ON | ENEMY_TRAIL_STRIP;
    enemy->trail.count = 4;
    enemy->trail.step = 1;
    for (int32_t i = 0; i < 3; i++) {
        M_DrawFrame();
    }
    // The head and three quads through the four boxes.
    CHECK_EQ_INT(M_DrawFrame(), 4);
    const GFX_RECORDED_QUAD *const first = GfxRecorder_Quad(1);
    CHECK(first->vertices[0].position.y == 100.0f + 16.0f + 16.0f + 0.5f);
    CHECK(first->vertices[1].position.y == 100.0f + 16.0f - 16.0f + 0.5f);
    CHECK(GfxRecorder_Quad(3)->vertices[2].position.x == 100.0f + 32.0f + 0.5f);
    enemy->trail.boxes[1].pos.x = -999.0f;
    // The update shifts the gone box to 2; the strip stops there.
    CHECK_EQ_INT(M_DrawFrame(), 2);
    M_TearDown();
}

// Spawns an enemy at 100, 100 with life to spare.
static ENEMY *M_SpawnTarget(void)
{
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    enemy->life = 100000;
    return enemy;
}

// Runs a frame in which the shots deal damage, with the callback timer of
// enemy at frame. Returns the life it took.
static int32_t M_HurtAt(ENEMY *const enemy, const int32_t damage, int32_t frame)
{
    Timer_Set(&enemy->callbacks.timer, frame);
    m_Damage = damage;
    const int32_t life = enemy->life;
    M_Frame();
    return life - enemy->life;
}

static int32_t M_Hurt(ENEMY *const enemy, const int32_t damage)
{
    return M_HurtAt(enemy, damage, 0);
}

// Runs M_HurtAt and returns the cherry it gained.
static int32_t M_CherryAt(
    ENEMY *const enemy, const int32_t damage, const int32_t frame)
{
    const int32_t cherry = g_GameManager.cherry.value;
    M_HurtAt(enemy, damage, frame);
    return g_GameManager.cherry.value - cherry;
}

TEST(shots_score_cherry_by_the_stage_and_more_on_unfocused_bosses)
{
    M_SetUp("damage_cherry");
    ENEMY *const enemy = M_SpawnTarget();
    // Stage 1: 60 / (30 - 2) and 60 / (10 - 0) tens.
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 0), 20);
    enemy->flags.boss = true;
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 0), 60);
    // Stage 3: 49 / (10 - 2).
    g_GameManager.run.stage = 3;
    CHECK_EQ_INT(M_CherryAt(enemy, 49, 0), 60);
    // Stage 5: 60 / (10 - 3) tens up to 70, and 60 / (30 - 10).
    g_GameManager.run.stage = 5;
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 0), 70);
    g_Player.move.focused = true;
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 0), 30);
    // Damage too small for points scores 10 on odd frames while focused.
    CHECK_EQ_INT(M_CherryAt(enemy, 5, 0), 0);
    CHECK_EQ_INT(M_CherryAt(enemy, 5, 1), 10);
    enemy->flags.boss = false;
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 0), 0);
    g_Player.move.focused = false;
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 0), 30);
    CHECK_EQ_INT(M_CherryAt(enemy, 5, 0), 10);
    // The stage counts up to 5.
    g_GameManager.run.stage = 6;
    CHECK_EQ_INT(M_CherryAt(enemy, 57, 0), 20);
    g_Player.bomb.running = true;
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 0), 0);
    M_TearDown();
}

TEST(shots_deal_at_most_70_and_score_a_fifth_of_it)
{
    M_SetUp("damage_cap");
    ENEMY *const enemy = M_SpawnTarget();
    const uint32_t score = g_GameManager.score.value;
    CHECK_EQ_INT(M_Hurt(enemy, 100), 70);
    // GameManager_AddScore takes tens.
    CHECK_EQ_INT(g_GameManager.score.value - score, 14);
    CHECK_EQ_INT(enemy->last_damage, 70);
    // Damage still scores where it takes no life.
    enemy->flags.takes_damage = false;
    CHECK_EQ_INT(M_Hurt(enemy, 34), 0);
    CHECK_EQ_INT(g_GameManager.score.value - score, 14 + 6);
    CHECK_EQ_INT(enemy->last_damage, 0);
    M_TearDown();
}

TEST(shot_0_cuts_the_damage_to_enemies_but_bosses_on_stages_4_to_6)
{
    M_SetUp("damage_shot_0");
    g_GameManager.run.shot_index = 0;
    ENEMY *const enemy = M_SpawnTarget();
    g_GameManager.run.stage = 3;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 64);
    g_GameManager.run.stage = 4;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 64 - 16 - 4);
    g_GameManager.run.stage = 5;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 32);
    g_GameManager.run.stage = 6;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 32);
    g_GameManager.run.stage = 7;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 64);
    g_GameManager.run.stage = 5;
    // The cut goes with the cherry, which focus and the bomb stop.
    g_Player.move.focused = true;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 64);
    g_Player.move.focused = false;
    g_Player.bomb.running = true;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 64);
    g_Player.bomb.running = false;
    enemy->flags.boss = true;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 64);
    g_GameManager.run.shot_index = 1;
    enemy->flags.boss = false;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 64);
    M_TearDown();
}

TEST(shot_0_takes_20_and_30_cherry_down_by_10_on_odd_frames)
{
    M_SetUp("damage_shot_0_odd");
    g_GameManager.run.shot_index = 0;
    ENEMY *const enemy = M_SpawnTarget();
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 0), 20);
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 1), 10);
    CHECK_EQ_INT(M_CherryAt(enemy, 90, 3), 20);
    CHECK_EQ_INT(M_CherryAt(enemy, 120, 1), 40);
    CHECK_EQ_INT(M_CherryAt(enemy, 5, 1), 10);
    g_GameManager.run.shot_index = 1;
    CHECK_EQ_INT(M_CherryAt(enemy, 60, 1), 20);
    M_TearDown();
}

TEST(spell_cards_cut_shots_to_a_seventh_and_bombs_once_bombed)
{
    M_SetUp("damage_spell");
    ENEMY *const enemy = M_SpawnTarget();
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    CHECK_EQ_INT(M_Hurt(enemy, 50), 7);
    CHECK_EQ_INT(M_Hurt(enemy, 5), 1);
    m_DamageByBomb = true;
    CHECK_EQ_INT(M_Hurt(enemy, 50), 0);
    g_Enemies.spell_card.bombed = true;
    CHECK_EQ_INT(M_Hurt(enemy, 50), 20);
    CHECK_EQ_INT(M_Hurt(enemy, 7), 2);
    CHECK_EQ_INT(M_Hurt(enemy, 2), 1);
    CHECK_EQ_INT(enemy->last_damage, 1);
    g_Enemies.spell_card.state = ENEMY_SPELL_NONE;
    CHECK_EQ_INT(M_Hurt(enemy, 50), 50);
    M_TearDown();
}

TEST(invulnerable_bosses_take_a_ninth_and_others_nothing)
{
    M_SetUp("damage_invulnerable");
    ENEMY *const enemy = M_SpawnTarget();
    ENEMY *const boss = M_SpawnAt(150.0f, 100.0f);
    boss->flags.boss = true;
    boss->life = 100000;
    Timer_Set(&enemy->invulnerable_timer, 2);
    Timer_Set(&boss->invulnerable_timer, 2);
    int32_t life = boss->life;
    CHECK_EQ_INT(M_Hurt(enemy, 64), 0);
    CHECK_EQ_INT(life - boss->life, 7);
    CHECK_EQ_INT(boss->last_damage, 7);
    CHECK_EQ_INT(enemy->invulnerable_timer.current, 1);
    // A spell card cuts first.
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    life = boss->life;
    CHECK_EQ_INT(M_Hurt(enemy, 63), 0);
    CHECK_EQ_INT(life - boss->life, 1);
    g_Enemies.spell_card.state = ENEMY_SPELL_NONE;
    CHECK_EQ_INT(M_Hurt(enemy, 45), 45);
    CHECK_EQ_INT(enemy->invulnerable_timer.current, 0);
    M_TearDown();
}

TEST(the_extra_hitbox_adds_its_damage_over_2_5_and_sets_the_bomb_hit)
{
    M_SetUp("damage_extra");
    ENEMY *const enemy = M_SpawnTarget();
    m_ExtraDamage = 9;
    CHECK_EQ_INT(M_Hurt(enemy, 10), 10);
    CHECK_EQ_INT(m_DamageCalls, 1);
    enemy->extra_hitbox_size = (VEC3) { M_EXTRA_SIZE, 10.0f, 10.0f };
    CHECK_EQ_INT(M_Hurt(enemy, 10), 13);
    CHECK_EQ_INT(m_DamageCalls, 3);
    CHECK_EQ_INT(M_Hurt(enemy, 0), 3);
    // A bomb in the extra hitbox adds nothing.
    m_ExtraByBomb = true;
    CHECK_EQ_INT(M_Hurt(enemy, 10), 10);
    // The bomb hit is that of the extra hitbox, whatever the first says.
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    CHECK_EQ_INT(M_Hurt(enemy, 49), 0);
    m_ExtraByBomb = false;
    m_DamageByBomb = true;
    CHECK_EQ_INT(M_Hurt(enemy, 46), (46 + 3) / 7);
    M_TearDown();
}

TEST(contact_tests_every_sixth_trail_box_narrowing_with_the_trail)
{
    M_SetUp("contact_trail");
    ENEMY *const enemy = M_SpawnTarget();
    enemy->trail.flags = ENEMY_TRAIL_ON | ENEMY_TRAIL_NARROW;
    enemy->trail.count = 20;
    enemy->trail.contact_count = 14;
    enemy->trail.step = 1;
    m_Contact = PLAYER_CONTACT_BOX;
    const int32_t life = enemy->life;
    M_Frame();
    // The head and boxes 1, 7 and 13 each cost 10 life.
    CHECK_EQ_INT(m_HitCalls, 4);
    CHECK_EQ_INT(life - enemy->life, 40);
    CHECK_EQ_INT(m_GrazeCalls, 0);
    CHECK(m_Hits[0].size.x == 12.0f / 1.5f);
    CHECK(m_Hits[2].pos.x == -999.0f);
    const float narrow = 12.0f - 12.0f * 7.0f * (1.0f / 14.0f);
    CHECK(m_Hits[2].size.y == narrow / 1.5f);
    enemy->trail.flags = ENEMY_TRAIL_ON;
    m_HitCalls = 0;
    M_Frame();
    CHECK(m_Hits[3].size.z == 12.0f / 1.5f);
    int32_t before = enemy->life;
    enemy->flags.boss = true;
    M_Frame();
    CHECK_EQ_INT(enemy->life, before);
    enemy->flags.contact_damage = false;
    m_HitCalls = 0;
    M_Frame();
    CHECK_EQ_INT(m_HitCalls, 0);
    M_TearDown();
}

TEST(grazeable_enemies_graze_every_sixth_frame_and_cost_no_life)
{
    M_SetUp("contact_graze");
    ENEMY *const enemy = M_SpawnTarget();
    enemy->flags.grazeable = true;
    enemy->trail.flags = ENEMY_TRAIL_ON;
    enemy->trail.count = 8;
    enemy->trail.contact_count = 8;
    enemy->trail.step = 1;
    m_Contact = PLAYER_CONTACT_BOX;
    const int32_t life = enemy->life;
    // The timer is at 0 and has ticked: the head and boxes 1 and 7.
    M_Frame();
    CHECK_EQ_INT(m_GrazeCalls, 3);
    CHECK(m_Grazes[0].size.x == 12.0f / 0.7f);
    for (int32_t i = 1; i < 6; i++) {
        M_Frame();
    }
    CHECK_EQ_INT(m_GrazeCalls, 3);
    M_Frame();
    CHECK_EQ_INT(m_GrazeCalls, 6);
    CHECK_EQ_INT(enemy->life, life);
    // A held timer has not ticked.
    enemy->callbacks.timer.previous = enemy->callbacks.timer.current = 12;
    M_Frame();
    CHECK_EQ_INT(m_GrazeCalls, 6);
    M_TearDown();
}

TEST(targets_go_to_the_lowest_enemy_unless_a_boss_is_nearer_in_x)
{
    M_SetUp("targets");
    M_SpawnAt(50.0f, 100.0f);
    M_SpawnAt(300.0f, 200.0f);
    M_SpawnAt(250.0f, 150.0f);
    M_Frame();
    CHECK(g_Player.homing_target.x == 300.0f);
    CHECK(g_Player.aim_target.x == -999.0f);
    CHECK(!g_Player.target_found);
    ENEMY *const far = M_SpawnAt(260.0f, 50.0f);
    ENEMY *const near = M_SpawnAt(180.0f, 60.0f);
    far->flags.boss = true;
    near->flags.boss = true;
    g_Player.homing_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    M_Frame();
    CHECK(g_Player.homing_target.x == 180.0f);
    CHECK(g_Player.target_found);
    // Once found, later enemies keep it.
    M_SpawnAt(190.0f, 300.0f);
    M_Frame();
    CHECK(g_Player.homing_target.x == 180.0f);
    M_TearDown();
}

TEST(character_2_aims_at_enemies_60_to_120_degrees_above_the_player)
{
    M_SetUp("targets_aim");
    g_GameManager.run.character = 2;
    // 45 and 135 degrees above the player, then 63 and 90.
    M_SpawnAt(292.0f, 300.0f);
    M_SpawnAt(92.0f, 300.0f);
    M_SpawnAt(242.0f, 300.0f);
    M_SpawnAt(192.0f, 100.0f);
    M_Frame();
    CHECK(g_Player.aim_target.x == 242.0f);
    CHECK(g_Player.homing_target.x == 292.0f);
    CHECK(!g_Player.target_found);
    // A boss outside the window sets only the homing target.
    ENEMY *const boss = M_SpawnAt(10.0f, 390.0f);
    boss->flags.boss = true;
    g_Player.homing_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    M_Frame();
    CHECK(!g_Player.target_found);
    CHECK(g_Player.homing_target.x == 10.0f);
    CHECK(g_Player.aim_target.x == 242.0f);
    boss->move.pos = (VEC3) { 192.0f, 50.0f, 0.0f };
    M_Frame();
    CHECK(g_Player.target_found);
    CHECK(g_Player.aim_target.y == 50.0f);
    M_TearDown();
}

// Runs four frames of a strip of four boxes with flags. Returns the quads
// of the last.
static int32_t M_DrawStripOf4(ENEMY *const enemy, const uint8_t flags)
{
    enemy->trail.flags = ENEMY_TRAIL_ON | ENEMY_TRAIL_STRIP | flags;
    enemy->trail.count = 4;
    enemy->trail.step = 1;
    for (int32_t i = 0; i < 3; i++) {
        M_DrawFrame();
    }
    return M_DrawFrame();
}

TEST(narrow_strips_drop_the_boxes_whose_angles_match_their_neighbours)
{
    M_SetUp("trail_strip_narrow");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    // Boxes 1 and 2 match; the last box is never dropped.
    CHECK_EQ_INT(M_DrawStripOf4(enemy, ENEMY_TRAIL_NARROW), 2);
    const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(1);
    CHECK(quad->vertices[2].position.y == 100.0f + 16.0f - 4.0f + 0.5f);
    CHECK(quad->vertices[3].position.y == 100.0f + 16.0f + 4.0f + 0.5f);
    // The texture steps as if no box dropped.
    float u = 0.5f;
    for (int32_t i = 0; i < 3; i++) {
        u -= 0.5f / 3.0f;
    }
    CHECK(quad->vertices[2].uv.x == u);
    enemy->move.angle = 0.5f;
    CHECK_EQ_INT(M_DrawFrame(), 4);
    M_TearDown();
}

TEST(fading_strips_fade_along_the_trail)
{
    M_SetUp("trail_strip_fade");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    CHECK_EQ_INT(M_DrawStripOf4(enemy, ENEMY_TRAIL_FADE), 4);
    CHECK_EQ_INT(GfxRecorder_Quad(1)->vertices[0].color.alpha, 0xff);
    CHECK_EQ_INT(
        GfxRecorder_Quad(3)->vertices[2].color.alpha, 0xff - 0xff * 3 / 4);
    CHECK_EQ_INT(
        GfxRecorder_Quad(3)->vertices[3].color.alpha, 0xff - 0xff * 3 / 4);
    CHECK_EQ_INT(enemy->vm.color >> 24, 0xff);
    M_TearDown();
}

TEST(strips_sit_at_the_fixed_play_area_corner)
{
    M_SetUp("trail_strip_corner");
    ENEMY *const enemy = M_SpawnAt(100.0f, 100.0f);
    g_GameManager.play_area.x = 40.0f;
    g_GameManager.play_area.y = 20.0f;
    CHECK_EQ_INT(M_DrawStripOf4(enemy, 0), 4);
    const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(1);
    CHECK(quad->vertices[0].position.x == 100.0f + 32.0f + 0.5f);
    CHECK(quad->vertices[0].position.y == 100.0f + 16.0f + 16.0f + 0.5f);
    M_TearDown();
}

TEST(the_sub_rank_grows_every_2400_less_240_per_life_frames)
{
    M_SetUp("rank_period");
    const int32_t rank = g_GameManager.rank.value;
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, rank);
    Timer_Set(&g_Enemies.frames, 2400 - 2 * 240);
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, rank + 1);
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, rank + 1);
    g_GameManager.player.lives = 3.0f;
    Timer_Set(&g_Enemies.frames, 2 * (2400 - 3 * 240));
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, rank + 2);
    // Dialogue stops the rank and the frame count.
    const int32_t frames = g_GameManager.run.progress_frames;
    m_MsgRunning = true;
    Timer_Set(&g_Enemies.frames, 2400 - 3 * 240);
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.value, rank + 2);
    CHECK_EQ_INT(g_GameManager.run.progress_frames, frames);
    m_MsgRunning = false;
    M_Frame();
    CHECK_EQ_INT(g_GameManager.run.progress_frames, frames + 1);
    M_TearDown();
}

TEST(a_boss_killed_outside_a_spell_card_clears_for_a_bonus)
{
    M_SetUp("death_boss_clear");
    ENEMY *const boss = M_SpawnAt(100.0f, 100.0f);
    boss->flags.boss = true;
    ENEMY *const grazeable = M_SpawnAt(150.0f, 100.0f);
    grazeable->flags.grazeable = true;
    grazeable->life = 100000;
    m_BulletBonus = 500;
    m_Damage = 5;
    M_Frame();
    CHECK(!boss->flags.active);
    CHECK_EQ_INT(m_BonusClears, 1);
    CHECK_EQ_INT(m_BonusClearMax, 8000);
    CHECK_EQ_INT(m_BonusPopup, 500 + 2000);
    CHECK_EQ_INT(grazeable->life, 0);
    ENEMY *const card = M_SpawnAt(100.0f, 100.0f);
    card->flags.boss = true;
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    M_Frame();
    CHECK(!card->flags.active);
    CHECK_EQ_INT(m_BonusClears, 1);
    M_TearDown();
}

TEST(the_low_layer_draws_lists_0_and_1_and_the_high_layer_2_and_3)
{
    M_SetUp("draw_layers");
    const uint8_t lists[4] = { 3, 1, 2, 0 };
    for (int32_t i = 0; i < 4; i++) {
        ENEMY *const enemy = M_SpawnAt(100.0f + 50.0f * lists[i], 100.0f);
        enemy->draw_list = lists[i];
    }
    CHECK_EQ_INT(M_DrawFrame(), 4);
    for (int32_t i = 1; i < 4; i++) {
        CHECK(
            GfxRecorder_Quad(i)->vertices[0].position.x
            == GfxRecorder_Quad(i - 1)->vertices[0].position.x + 50.0f);
    }
    M_TearDown();
}
