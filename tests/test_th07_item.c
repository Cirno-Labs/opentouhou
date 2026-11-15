#include "th07/item.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/effect.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/sound.h"
#include "th07/stubs.h"

#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/gfx/private.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#define M_FIRST_SCRIPT 0x2c4
#define M_TOUCH_X 50.0f
#define M_NO_TOUCH -1.0f
#define M_ITEM_LINE 100.0f
#define M_PULL_SPEED 8.0f
#define M_COLLECT_SIZE 24.0f
#define M_CHERRY_OFFSET 1000
#define M_MAX_CALLS 64

// The test links these instead of the modules they stand for. The
// collect box touches every item at x m_TouchX, and records where each
// item was tested; the others record their calls.

typedef struct {
    VEC3 pos;
    int32_t value;
    uint32_t color;
    bool reserved; // AsciiText_AddPopup2
} M_POPUP;

typedef struct {
    EFFECT_TYPE type;
    VEC3 pos;
    int32_t count;
    uint32_t color;
} M_EFFECT_CALL;

RNG g_Rng;
float g_GameSpeed;
PLAYER g_Player;
ENEMY_MANAGER g_Enemies;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static VEC3 m_Probes[ITEM_COUNT];
static int32_t m_ProbeCount;
static float m_TouchX;
static float m_Angle;
static int32_t m_AngleCalls;
static VEC3 m_AngleFrom;
static int32_t m_ClearCalls;
static int32_t m_PowerPopups;
static int32_t m_CherryPopups;
static int32_t m_BorderStarts;
static uint32_t m_Events;
static int32_t m_EventMarks;
static M_POPUP m_Popups[M_MAX_CALLS];
static int32_t m_PopupCount;
static M_EFFECT_CALL m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static EFFECT m_SpareEffect;
static char *m_Directory;

bool Player_TouchesCollectBox(const VEC3 pos, const VEC3 size)
{
    CHECK(size.x == M_COLLECT_SIZE && size.y == M_COLLECT_SIZE);
    CHECK(size.z == 16.0f);
    if (m_ProbeCount < ITEM_COUNT) {
        m_Probes[m_ProbeCount] = pos;
    }
    m_ProbeCount++;
    return pos.x == m_TouchX;
}

float Player_AngleFrom(const VEC3 pos)
{
    m_AngleCalls++;
    m_AngleFrom = pos;
    return m_Angle;
}

void Player_StartBorder(void)
{
    m_BorderStarts++;
}

void Bullet_ClearAllToItems(void)
{
    m_ClearCalls++;
}

void Gui_ShowFullPowerPopup(void)
{
    m_PowerPopups++;
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
    m_CherryPopups++;
}

void Replay_MarkEvent(const REPLAY_EVENT event)
{
    m_Events |= event;
    m_EventMarks++;
}

static void M_AddPopup(
    const VEC3 pos, const int32_t value, const uint32_t color,
    const bool reserved)
{
    if (m_PopupCount < M_MAX_CALLS) {
        m_Popups[m_PopupCount] = (M_POPUP) { pos, value, color, reserved };
    }
    m_PopupCount++;
}

void AsciiText_AddPopup(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
    M_AddPopup(pos, value, color, false);
}

void AsciiText_AddPopup2(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
    M_AddPopup(pos, value, color, true);
}

EFFECT *Effect_Spawn(
    const EFFECT_TYPE type, const VEC3 pos, const int32_t count,
    const uint32_t color)
{
    if (m_EffectCount < M_MAX_CALLS) {
        m_Effects[m_EffectCount] = (M_EFFECT_CALL) { type, pos, count, color };
    }
    m_EffectCount++;
    return &m_SpareEffect;
}

static void M_Setup(void)
{
    Rng_SetSeed(&g_Rng, 7);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    GameManager_Create(&g_GameManager);
    g_GameManager.rank.value = 10;
    g_GameManager.rank.max = 99;
    g_GameManager.run.difficulty = 1;
    g_GameManager.cherry.offset = M_CHERRY_OFFSET;
    g_GameManager.cherry.value = M_CHERRY_OFFSET;
    g_GameManager.cherry.max = M_CHERRY_OFFSET + 200000;
    g_GameManager.cherry.plus = M_CHERRY_OFFSET;
    g_GameManager.player.lives = 2.0f;
    g_GameManager.player.bombs = 3.0f;
    g_GameSpeed = 1.0f;
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.pos.y = 400.0f;
    g_Player.shot_type.item_line = M_ITEM_LINE;
    g_Player.shot_type.item_pull_speed = M_PULL_SPEED;
    g_Player.shot_type.item_collect_size = M_COLLECT_SIZE;
    memset(&g_Enemies, 0, sizeof(g_Enemies));
    m_ProbeCount = 0;
    m_TouchX = M_NO_TOUCH;
    m_Angle = 1.0f;
    m_AngleCalls = 0;
    m_ClearCalls = 0;
    m_PowerPopups = 0;
    m_CherryPopups = 0;
    m_BorderStarts = 0;
    m_Events = 0;
    m_EventMarks = 0;
    m_PopupCount = 0;
    m_EffectCount = 0;
    Sound_Reset();
    Item_Reset();
}

static void M_Teardown(void)
{
    Anm_Shutdown();
}

static void M_Frame(void)
{
    m_ProbeCount = 0;
    Item_Update();
}

static void M_FillPool(void)
{
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        Item_Spawn((VEC3) { (float)i, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    }
}

static void M_Free(const float x)
{
    m_TouchX = x;
    M_Frame();
    m_TouchX = M_NO_TOUCH;
}

// Drops an item at the touch line and collects it in one frame. Returns
// where it was collected.
static VEC3 M_Collect(const ITEM_TYPE type, const float y)
{
    Item_Spawn((VEC3) { M_TOUCH_X, y, 0.0f }, type, ITEM_FALLING);
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 1);
    m_TouchX = M_NO_TOUCH;
    const VEC3 pos = m_Probes[0];
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 0);
    return pos;
}

static int32_t M_Cherry(void)
{
    return g_GameManager.cherry.value - M_CHERRY_OFFSET;
}

static int32_t M_Plus(void)
{
    return g_GameManager.cherry.plus - M_CHERRY_OFFSET;
}

// Checks that g_Rng moved from before by updates checksum updates of two
// draws each.
static void M_CheckDraws(RNG before, const int32_t updates)
{
    for (int32_t i = 0; i < updates * 2; i++) {
        Rng_U32InRange(&before, 100000);
    }
    CHECK_EQ_INT(g_Rng.seed, before.seed);
    CHECK_EQ_INT(g_Rng.generated, before.generated);
}

static bool M_IsQueued(const int32_t sound)
{
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        if (g_SoundEvents.queue[i] == sound) {
            return true;
        }
    }
    return false;
}

TEST(a_falling_item_accelerates_to_three_and_falls_out)
{
    M_Setup();
    Item_Spawn((VEC3) { 10.0f, 100.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    float y = 100.0f;
    float speed = -2.2f;
    int32_t frames = 0;
    for (;;) {
        y += speed;
        M_Frame();
        if (y >= 448.0f + 16.0f) {
            break;
        }
        CHECK_EQ_INT(m_ProbeCount, 1);
        CHECK(m_Probes[0].x == 10.0f && m_Probes[0].y == y);
        if (speed < 3.0f) {
            speed += 0.03f;
        } else {
            speed = 3.0f;
        }
        frames++;
    }
    CHECK_EQ_INT(m_ProbeCount, 0);
    CHECK(speed == 3.0f);
    CHECK(frames > 200);
    CHECK_EQ_INT(g_GameManager.rank.value, 9);
    CHECK_EQ_INT(g_GameManager.rank.sub, 97);
    M_Teardown();
}

TEST(an_item_falls_out_at_sixteen_below_the_play_area)
{
    M_Setup();
    const float y = 100.0f + -2.2f;
    g_GameManager.play_area.height = y - 16.0f;
    Item_Spawn((VEC3) { 10.0f, 100.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 0);
    CHECK(!Item_GetSlots()[0].active);
    g_GameManager.play_area.height = nextafterf(y - 16.0f, 1000.0f);
    Item_Spawn((VEC3) { 10.0f, 100.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 1);
    M_Teardown();
}

TEST(items_move_and_fall_at_the_game_speed)
{
    M_Setup();
    g_GameSpeed = 0.5f;
    Item_Spawn((VEC3) { 10.0f, 100.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    const float y = 100.0f + -2.2f * 0.5f;
    CHECK(m_Probes[0].y == y);
    const ITEM *const item = &Item_GetSlots()[0];
    CHECK(item->velocity.y == -2.2f + 0.03f * 0.5f);
    CHECK_EQ_INT(item->timer.current, 0);
    CHECK(item->timer.sub_frame == 0.5f);
    M_Frame();
    CHECK(m_Probes[0].y == y + (-2.2f + 0.03f * 0.5f) * 0.5f);
    CHECK_EQ_INT(item->timer.current, 1);
    M_Teardown();
}

TEST(a_thrown_item_flies_to_a_random_target_then_falls)
{
    M_Setup();
    RNG expected = g_Rng;
    Item_Spawn((VEC3) { 200.0f, 300.0f, 0.0f }, ITEM_POWER, ITEM_THROWN);
    const float tx = Rng_F32(&expected) * 288.0f + 48.0f;
    const float ty = Rng_F32(&expected) * 192.0f - 64.0f;
    CHECK(g_Rng.seed == expected.seed);
    CHECK_EQ_INT(g_Rng.generated, expected.generated);
    float x = 0.0f;
    float y = 0.0f;
    for (int32_t frame = 0; frame < 60; frame++) {
        M_Frame();
        const float t = (float)frame / 60.0f;
        x = t * tx + 200.0f * (1.0f - t);
        y = t * ty + 300.0f * (1.0f - t);
        CHECK_EQ_INT(m_ProbeCount, 1);
        CHECK(m_Probes[0].x == x && m_Probes[0].y == y);
    }
    M_Frame();
    CHECK(m_Probes[0].x == x && m_Probes[0].y == y);
    CHECK_EQ_INT(Item_GetSlots()[0].state, ITEM_FALLING);
    M_Frame();
    CHECK(m_Probes[0].x == x && m_Probes[0].y == y + 0.03f);
    M_Teardown();
}

TEST(a_thrown_item_flies_by_the_fraction_of_its_timer)
{
    M_Setup();
    g_GameSpeed = 0.5f;
    Item_Spawn((VEC3) { 200.0f, 300.0f, 0.0f }, ITEM_POWER, ITEM_THROWN);
    const ITEM *const item = &Item_GetSlots()[0];
    const VEC3 target = item->target;
    M_Frame();
    M_Frame();
    const float t = 0.5f / 60.0f;
    CHECK(m_Probes[0].x == t * target.x + 200.0f * (1.0f - t));
    CHECK(m_Probes[0].y == t * target.y + 300.0f * (1.0f - t));
    M_Teardown();
}

TEST(a_landed_thrown_item_is_pulled_in_at_full_power)
{
    M_Setup();
    g_GameManager.player.power = 128.0f;
    g_Player.pos.y = 50.0f;
    Item_Spawn((VEC3) { 200.0f, 300.0f, 0.0f }, ITEM_BIG_POWER, ITEM_THROWN);
    for (int32_t frame = 0; frame < 61; frame++) {
        M_Frame();
    }
    CHECK_EQ_INT(m_AngleCalls, 0);
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 1);
    CHECK_EQ_INT(Item_GetSlots()[0].state, ITEM_HOMING);
    M_Teardown();
}

TEST(a_thrown_item_does_not_fall_out_in_flight)
{
    M_Setup();
    Item_Spawn((VEC3) { 200.0f, 1000.0f, 0.0f }, ITEM_POWER, ITEM_THROWN);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 1);
    CHECK(m_Probes[0].y == 1000.0f);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    M_Teardown();
}

TEST(full_power_above_the_item_line_pulls_items_in_for_good)
{
    M_Setup();
    g_GameManager.player.power = 128.0f;
    g_Player.pos.y = nextafterf(M_ITEM_LINE, 0.0f);
    m_Angle = 2.0f;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    float vx;
    float vy;
    X87_SinCosScale(2.0f, M_PULL_SPEED, &vx, &vy);
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 1);
    CHECK(m_AngleFrom.x == 10.0f && m_AngleFrom.y == 300.0f);
    CHECK(m_Probes[0].x == 10.0f + vx && m_Probes[0].y == 300.0f + vy);
    CHECK(!Item_GetSlots()[0].full_value);
    g_Player.pos.y = 300.0f;
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 2);
    M_Teardown();
}

TEST(items_are_not_pulled_in_at_the_item_line)
{
    M_Setup();
    g_GameManager.player.power = 128.0f;
    g_Player.pos.y = M_ITEM_LINE;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 0);
    M_Teardown();
}

TEST(items_are_not_pulled_in_below_full_power)
{
    M_Setup();
    g_GameManager.player.power = 127.9f;
    g_Player.pos.y = 0.0f;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 0);
    M_Teardown();
}

TEST(extra_and_phantasm_pull_items_in_at_any_power)
{
    for (int32_t difficulty = 4; difficulty < 6; difficulty++) {
        M_Setup();
        g_GameManager.run.difficulty = difficulty;
        g_Player.pos.y = 0.0f;
        Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
        M_Frame();
        CHECK_EQ_INT(m_AngleCalls, 1);
        M_Teardown();
    }
}

TEST(the_border_pulls_items_in_at_full_value)
{
    M_Setup();
    g_Player.border_state = PLAYER_BORDER_RUNNING;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 1);
    CHECK(Item_GetSlots()[0].full_value);
    M_Teardown();
}

TEST(a_respawn_drops_a_pulled_item_and_keeps_its_sideways_speed)
{
    M_Setup();
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_CollectAll();
    M_Frame();
    const ITEM *const item = &Item_GetSlots()[0];
    const float vx = item->velocity.x;
    CHECK(vx != 0.0f);
    const VEC3 last = m_Probes[0];
    g_Player.state.value = PLAYER_RESPAWNING;
    M_Frame();
    CHECK_EQ_INT(item->state, ITEM_FALLING);
    CHECK(m_Probes[0].x == last.x + vx);
    CHECK(m_Probes[0].y == last.y + -0.5f);
    CHECK(item->velocity.y == -0.5f + 0.03f);
    CHECK_EQ_INT(m_AngleCalls, 1);
    M_Teardown();
}

TEST(collect_all_pulls_every_item_in_from_a_slow_rise)
{
    M_Setup();
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_Spawn((VEC3) { 20.0f, 300.0f, 0.0f }, ITEM_POWER, ITEM_THROWN);
    Item_CollectAll();
    const ITEM *const items = Item_GetSlots();
    for (int32_t i = 0; i < 2; i++) {
        CHECK_EQ_INT(items[i].state, ITEM_HOMING);
        CHECK(items[i].velocity.x == 0.0f && items[i].velocity.y == -0.5f);
    }
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 2);
    M_Teardown();
}

TEST(drop_pulled_lets_homing_items_fall_from_a_slow_rise)
{
    M_Setup();
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_Spawn((VEC3) { 20.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_CollectAll();
    M_Frame();
    Item_Spawn((VEC3) { 30.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_THROWN);
    Item_DropPulled();
    const ITEM *const items = Item_GetSlots();
    for (int32_t i = 0; i < 2; i++) {
        CHECK_EQ_INT(items[i].state, ITEM_FALLING);
        CHECK(items[i].velocity.x == 0.0f && items[i].velocity.y == -0.9f);
    }
    CHECK_EQ_INT(items[2].state, ITEM_THROWN);
    M_Teardown();
}

TEST(a_full_pool_drops_nothing)
{
    M_Setup();
    M_FillPool();
    const int32_t generated = g_Rng.generated;
    Item_Spawn((VEC3) { 10000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_THROWN);
    CHECK_EQ_INT(g_Rng.generated, generated);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, ITEM_COUNT);
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        CHECK(m_Probes[i].x == (float)i);
    }
    M_Teardown();
}

TEST(a_spawn_takes_the_slot_after_the_last_used)
{
    M_Setup();
    for (int32_t i = 0; i < 3; i++) {
        Item_Spawn((VEC3) { (float)i, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    }
    M_Free(0.0f);
    Item_Spawn((VEC3) { 3.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 3);
    CHECK(m_Probes[0].x == 1.0f);
    CHECK(m_Probes[2].x == 3.0f);
    M_Teardown();
}

TEST(a_spawn_wraps_to_the_first_free_slot)
{
    M_Setup();
    M_FillPool();
    M_Free(5.0f);
    Item_Spawn((VEC3) { 10000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, ITEM_COUNT);
    CHECK(m_Probes[5].x == 10000.0f);
    M_Teardown();
}

TEST(a_spawn_searches_past_the_end_of_the_pool)
{
    M_Setup();
    M_FillPool();
    M_Free(3.0f);
    Item_Spawn((VEC3) { 10000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Free(0.0f);
    Item_Spawn((VEC3) { 20000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, ITEM_COUNT);
    CHECK(m_Probes[0].x == 20000.0f);
    CHECK(m_Probes[3].x == 10000.0f);
    M_Teardown();
}

TEST(a_spawn_finds_a_free_slot_just_before_the_start)
{
    M_Setup();
    M_FillPool();
    M_Free((float)(ITEM_COUNT - 1));
    Item_Spawn((VEC3) { 10000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, ITEM_COUNT);
    CHECK(m_Probes[ITEM_COUNT - 1].x == 10000.0f);
    M_Teardown();
}

TEST(reset_removes_every_item)
{
    M_Setup();
    for (int32_t i = 0; i < 3; i++) {
        Item_Spawn((VEC3) { (float)i, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    }
    M_Frame();
    Item_Reset();
    CHECK(!Item_GetSlots()[0].drawn);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 0);
    Item_Spawn((VEC3) { 7.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    CHECK(Item_GetSlots()[0].active);
    M_Teardown();
}

TEST(a_power_item_spawns_as_a_cherry_item_at_full_power)
{
    M_Setup();
    g_GameManager.player.power = 128.0f;
    Item_Spawn((VEC3) { 10.0f, 0.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { 20.0f, 0.0f, 0.0f }, ITEM_BIG_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { 30.0f, 0.0f, 0.0f }, ITEM_FULL_POWER, ITEM_FALLING);
    g_GameManager.player.power = 127.9f;
    Item_Spawn((VEC3) { 40.0f, 0.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    const ITEM *const items = Item_GetSlots();
    CHECK_EQ_INT(items[0].type, ITEM_CHERRY);
    CHECK_EQ_INT(items[1].type, ITEM_CHERRY);
    CHECK_EQ_INT(items[2].type, ITEM_FULL_POWER);
    CHECK_EQ_INT(items[3].type, ITEM_POWER);
    CHECK(items[0].vm.z_write_off);
    CHECK(items[0].on_screen && !items[0].full_value);
    M_Teardown();
}

TEST(a_power_item_below_full_power_adds_one_power)
{
    M_Setup();
    g_GameManager.player.power = 10.0f;
    g_GameManager.player.power_item_combo = 5;
    const RNG before = g_Rng;
    M_Collect(ITEM_POWER, 300.0f);
    CHECK(g_GameManager.player.power == 11.0f);
    M_CheckDraws(before, 1);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 0);
    CHECK_EQ_INT(g_GameManager.score.value, 1);
    CHECK_EQ_INT(g_GameManager.rank.sub, 1);
    CHECK_EQ_INT(m_ClearCalls, 0);
    CHECK_EQ_INT(m_PopupCount, 1);
    CHECK_EQ_INT(m_Popups[0].value, 10);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_WHITE);
    M_Teardown();
}

TEST(a_new_power_level_shows_the_level_up)
{
    M_Setup();
    g_GameManager.player.power = 7.5f;
    M_Collect(ITEM_POWER, 300.0f);
    CHECK_EQ_INT(m_Popups[0].value, -1);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_ARGB(0xff, 0xff, 0xc0, 0xa0));
    CHECK(M_IsQueued(SOUND_POWER_UP));
    CHECK(M_IsQueued(SOUND_ITEM));
    M_Teardown();
}

TEST(reaching_full_power_clears_the_bullets_and_converts_the_others)
{
    M_Setup();
    g_GameManager.player.power = 127.0f;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { 20.0f, 300.0f, 0.0f }, ITEM_BIG_POWER, ITEM_THROWN);
    Item_Spawn((VEC3) { 30.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_Spawn((VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { 40.0f, 300.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    const RNG before = g_Rng;
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK(g_GameManager.player.power == 128.0f);
    M_CheckDraws(before, 2);
    CHECK_EQ_INT(m_ClearCalls, 1);
    CHECK_EQ_INT(m_PowerPopups, 1);
    CHECK_EQ_INT(g_GameManager.score.value, 1);
    CHECK_EQ_INT(m_Popups[0].value, -1);
    const ITEM *const items = Item_GetSlots();
    CHECK_EQ_INT(items[0].type, ITEM_CHERRY);
    CHECK_EQ_INT(items[1].type, ITEM_CHERRY);
    CHECK_EQ_INT(items[2].type, ITEM_POINT);
    CHECK_EQ_INT(items[3].type, ITEM_POWER);
    CHECK_EQ_INT(items[4].type, ITEM_CHERRY);
    CHECK_EQ_INT(m_EffectCount, 3);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_EXPLOSION);
    CHECK_EQ_INT(m_Effects[0].count, 1);
    CHECK_EQ_INT(m_Effects[0].color, COLOR_WHITE);
    CHECK(m_Effects[2].pos.x == 40.0f && m_Effects[2].pos.y == 300.0f);
    // The thrown item loses where it dropped; the faster ones keep their
    // speed.
    CHECK(items[0].velocity.y == -2.2f + 0.03f);
    CHECK(items[1].velocity.x == 0.0f && items[1].velocity.y == -0.5f);
    CHECK(items[4].velocity.y == -2.2f + 0.03f);
    M_Teardown();
}

TEST(reaching_full_power_in_a_spell_card_keeps_the_bullets)
{
    M_Setup();
    g_GameManager.player.power = 125.0f;
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    M_Collect(ITEM_BIG_POWER, 300.0f);
    CHECK(g_GameManager.player.power == 128.0f);
    CHECK_EQ_INT(m_ClearCalls, 0);
    CHECK_EQ_INT(m_PowerPopups, 1);
    M_Teardown();
}

TEST(a_power_item_at_full_power_scores_by_the_combo)
{
    M_Setup();
    g_GameManager.player.power = 128.0f;
    g_GameManager.player.power_item_combo = 0;
    Item_Reset();
    g_GameManager.player.power = 127.9f;
    Item_Spawn((VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    g_GameManager.player.power = 128.0f;
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 1);
    CHECK_EQ_INT(g_GameManager.score.value, 2);
    CHECK_EQ_INT(g_GameManager.rank.sub, 1);
    CHECK_EQ_INT(m_Popups[0].value, 20);
    M_Teardown();
}

static void M_CollectComboPower(const int8_t combo)
{
    g_GameManager.player.power = 127.9f;
    Item_Spawn((VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    g_GameManager.player.power = 128.0f;
    g_GameManager.player.power_item_combo = combo;
    m_TouchX = M_TOUCH_X;
    M_Frame();
    m_TouchX = M_NO_TOUCH;
}

TEST(the_power_item_combo_stops_at_thirty)
{
    M_Setup();
    M_CollectComboPower(30);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 30);
    CHECK_EQ_INT(g_GameManager.score.value, 5120);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_ARGB(0xff, 0xff, 0xff, 0x00));
    M_Teardown();
}

TEST(a_negative_power_item_combo_takes_the_last_bonus)
{
    M_Setup();
    M_CollectComboPower(-5);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 30);
    CHECK_EQ_INT(g_GameManager.score.value, 5120);
    M_Teardown();
}

TEST(a_big_power_item_adds_eight_power_and_keeps_the_combo)
{
    M_Setup();
    g_GameManager.player.power = 10.0f;
    g_GameManager.player.power_item_combo = 5;
    const RNG before = g_Rng;
    M_Collect(ITEM_BIG_POWER, 300.0f);
    CHECK(g_GameManager.player.power == 18.0f);
    M_CheckDraws(before, 1);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 5);
    CHECK_EQ_INT(g_GameManager.score.value, 1);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    CHECK_EQ_INT(m_Popups[0].value, -1);
    M_Teardown();
}

TEST(a_big_power_item_at_full_power_shows_the_last_amount_only)
{
    M_Setup();
    g_GameManager.player.power = 127.9f;
    Item_Spawn(
        (VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_BIG_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_STAR, ITEM_FALLING);
    Item_Spawn(
        (VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_BIG_POWER, ITEM_FALLING);
    g_GameManager.player.power = 128.0f;
    g_GameManager.player.graze_total = 400;
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK_EQ_INT(m_PopupCount, 3);
    CHECK_EQ_INT(m_Popups[0].value, 0);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_WHITE);
    CHECK_EQ_INT(m_Popups[1].value, 400);
    CHECK_EQ_INT(m_Popups[2].value, 400);
    CHECK_EQ_INT(g_GameManager.score.value, 40);
    CHECK(g_GameManager.player.power == 128.0f);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 0);
    M_Teardown();
}

TEST(the_last_amount_starts_at_zero_in_each_update)
{
    M_Setup();
    g_GameManager.player.graze_total = 400;
    M_Collect(ITEM_STAR, 300.0f);
    g_GameManager.player.power = 127.9f;
    Item_Spawn(
        (VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_BIG_POWER, ITEM_FALLING);
    g_GameManager.player.power = 128.0f;
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK_EQ_INT(m_Popups[1].value, 0);
    M_Teardown();
}

static void M_SetCherry(const int32_t cherry)
{
    g_GameManager.cherry.value = M_CHERRY_OFFSET + cherry;
}

TEST(a_point_item_scores_less_per_whole_pixel_below_the_item_line)
{
    M_Setup();
    const VEC3 pos = M_Collect(ITEM_POINT, M_ITEM_LINE + 2.2f + 3.9f);
    CHECK(pos.y > M_ITEM_LINE + 3.0f && pos.y < M_ITEM_LINE + 4.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 4970);
    CHECK_EQ_INT(m_Popups[0].value, 49700);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_WHITE);
    CHECK_EQ_INT(g_GameManager.player.point_items, 1);
    CHECK_EQ_INT(g_GameManager.player.point_items_total, 1);
    M_Teardown();
}

TEST(a_point_item_scores_in_full_above_the_item_line)
{
    M_Setup();
    const VEC3 pos = M_Collect(ITEM_POINT, M_ITEM_LINE + 2.1f);
    CHECK(pos.y < M_ITEM_LINE);
    CHECK_EQ_INT(g_GameManager.score.value, 5000);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_ARGB(0xff, 0xff, 0xff, 0x00));
    M_Teardown();
}

TEST(a_point_item_pulled_in_by_the_border_scores_in_full)
{
    M_Setup();
    g_Player.border_state = PLAYER_BORDER_RUNNING;
    m_Angle = 0.0f;
    Item_Spawn(
        (VEC3) { M_TOUCH_X - 8.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK_EQ_INT(g_GameManager.score.value, 5000);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_ARGB(0xff, 0xff, 0xff, 0x00));
    M_Teardown();
}

TEST(a_point_item_adds_rank_by_the_fixed_line)
{
    M_Setup();
    g_Player.shot_type.item_line = 200.0f;
    M_Collect(ITEM_POINT, 131.0f);
    CHECK_EQ_INT(g_GameManager.rank.sub, 3);
    CHECK_EQ_INT(g_GameManager.score.value, 5000);
    M_Collect(ITEM_POINT, 130.0f);
    CHECK_EQ_INT(g_GameManager.rank.sub, 13);
    M_Teardown();
}

TEST(a_full_point_item_scores_the_cherry_above_50000)
{
    M_Setup();
    M_SetCherry(50000);
    M_Collect(ITEM_POINT, 0.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 5000);
    M_SetCherry(123456);
    M_Collect(ITEM_POINT, 0.0f);
    CHECK_EQ_INT(m_Popups[1].value, 123450);
    CHECK_EQ_INT(g_GameManager.score.value, 5000 + 12345);
    M_Teardown();
}

TEST(a_low_point_item_adds_a_fifth_of_the_cherry_above_50000)
{
    M_Setup();
    M_SetCherry(50000 + 1234);
    M_Collect(ITEM_POINT, M_ITEM_LINE + 2.2f + 10.5f);
    CHECK_EQ_INT(m_Popups[0].value, 49000 + 240);
    M_Teardown();
}

TEST(point_items_win_every_extend_they_reach)
{
    M_Setup();
    g_GameManager.player.point_items_total = 199;
    g_GameManager.player.lives = 7.0f;
    const RNG before = g_Rng;
    M_Collect(ITEM_POINT, 0.0f);
    CHECK_EQ_INT(g_GameManager.player.extends, 3);
    CHECK_EQ_INT(g_GameManager.player.next_extend_items, 300);
    CHECK(g_GameManager.player.lives == 8.0f);
    CHECK(g_GameManager.player.bombs == 5.0f);
    M_CheckDraws(before, 3);
    CHECK_EQ_INT(g_GameManager.rank.value, 16);
    CHECK(M_IsQueued(SOUND_EXTEND));
    M_Teardown();
}

TEST(the_extends_follow_the_difficulty)
{
    static const int32_t normal[] = { 50, 125, 200, 300, 450, 800, 1000 };
    static const int32_t extra[] = { 200, 500, 800, 1300, 1800 };
    M_Setup();
    for (int32_t i = 0; i < (int32_t)ARRAY_SIZE(normal); i++) {
        g_GameManager.player.extends = i;
        g_GameManager.player.point_items_total = -1;
        M_Collect(ITEM_POINT, 0.0f);
        CHECK_EQ_INT(g_GameManager.player.next_extend_items, normal[i]);
    }
    g_GameManager.run.difficulty = 4;
    for (int32_t i = 0; i < (int32_t)ARRAY_SIZE(extra); i++) {
        g_GameManager.player.extends = i;
        g_GameManager.player.point_items_total = -1;
        M_Collect(ITEM_POINT, 0.0f);
        CHECK_EQ_INT(g_GameManager.player.next_extend_items, extra[i]);
    }
    M_Teardown();
}

TEST(no_extend_is_won_once_none_are_left)
{
    M_Setup();
    g_GameManager.player.extends = -1;
    g_GameManager.player.point_items_total = 5000;
    M_Collect(ITEM_POINT, 0.0f);
    CHECK_EQ_INT(g_GameManager.player.extends, -1);
    CHECK(g_GameManager.player.lives == 2.0f);
    M_Teardown();
}

TEST(a_life_item_adds_a_life_then_a_bomb_up_to_eight)
{
    M_Setup();
    g_GameManager.player.lives = 7.0f;
    g_GameManager.player.bombs = 7.0f;
    const RNG before = g_Rng;
    M_Collect(ITEM_LIFE, 300.0f);
    CHECK(g_GameManager.player.lives == 8.0f);
    CHECK(M_IsQueued(SOUND_EXTEND));
    CHECK_EQ_INT(g_GameManager.rank.value, 12);
    M_Collect(ITEM_LIFE, 300.0f);
    CHECK(g_GameManager.player.lives == 8.0f);
    CHECK(g_GameManager.player.bombs == 8.0f);
    CHECK_EQ_INT(g_GameManager.rank.value, 14);
    M_CheckDraws(before, 2);
    Sound_Reset();
    M_Collect(ITEM_LIFE, 300.0f);
    CHECK(!M_IsQueued(SOUND_EXTEND));
    CHECK_EQ_INT(g_GameManager.rank.value, 14);
    M_CheckDraws(before, 2);
    M_Teardown();
}

TEST(a_bomb_item_adds_a_bomb_up_to_eight)
{
    M_Setup();
    g_GameManager.player.bombs = 7.5f;
    const RNG before = g_Rng;
    M_Collect(ITEM_BOMB, 300.0f);
    CHECK(g_GameManager.player.bombs == 8.5f);
    M_CheckDraws(before, 1);
    M_Collect(ITEM_BOMB, 300.0f);
    CHECK(g_GameManager.player.bombs == 8.5f);
    M_CheckDraws(before, 1);
    CHECK_EQ_INT(g_GameManager.rank.sub, 10);
    M_Teardown();
}

// Collects every item at the touch line in one frame.
static void M_CollectAtTouch(void)
{
    m_TouchX = M_TOUCH_X;
    M_Frame();
    m_TouchX = M_NO_TOUCH;
}

TEST(a_full_power_item_fills_the_power)
{
    M_Setup();
    g_GameManager.player.power = 20.0f;
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    Item_Spawn(
        (VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_FULL_POWER, ITEM_FALLING);
    const RNG before = g_Rng;
    M_CollectAtTouch();
    CHECK(g_GameManager.player.power == 128.0f);
    M_CheckDraws(before, 1);
    CHECK_EQ_INT(g_GameManager.score.value, 100);
    CHECK_EQ_INT(m_ClearCalls, 1);
    CHECK_EQ_INT(m_PowerPopups, 1);
    CHECK(M_IsQueued(SOUND_POWER_UP));
    CHECK_EQ_INT(m_PopupCount, 2);
    CHECK_EQ_INT(m_Popups[0].value, -1);
    CHECK_EQ_INT(m_Popups[1].value, 1000);
    CHECK_EQ_INT(Item_GetSlots()[0].type, ITEM_CHERRY);
    CHECK_EQ_INT(Item_GetSlots()[1].type, ITEM_FULL_POWER);
    Sound_Reset();
    Item_Spawn(
        (VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_FULL_POWER, ITEM_FALLING);
    M_CollectAtTouch();
    CHECK_EQ_INT(g_GameManager.score.value, 200);
    CHECK_EQ_INT(m_ClearCalls, 1);
    CHECK(!M_IsQueued(SOUND_POWER_UP));
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    M_Teardown();
}

TEST(a_star_item_scores_by_the_graze_and_adds_cherry)
{
    M_Setup();
    g_GameManager.player.graze_total = 439;
    M_Collect(ITEM_STAR, 300.0f);
    CHECK_EQ_INT(m_Popups[0].value, 400);
    CHECK(m_Popups[0].reserved);
    CHECK_EQ_INT(g_GameManager.score.value, 40);
    CHECK_EQ_INT(M_Cherry(), 20);
    CHECK_EQ_INT(M_Plus(), 20);
    M_Teardown();
}

TEST(a_star_item_scores_100_as_a_bomb_starts)
{
    M_Setup();
    g_GameManager.player.graze_total = 439;
    g_Player.state.bomb_started = true;
    M_Collect(ITEM_STAR, 300.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 10);
    CHECK_EQ_INT(M_Cherry(), 20);
    M_Teardown();
}

TEST(a_star_item_in_a_bomb_adds_cherry_by_its_slot)
{
    M_Setup();
    g_Player.bomb.running = true;
    M_Collect(ITEM_STAR, 300.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 30);
    CHECK_EQ_INT(M_Cherry(), 10);
    CHECK_EQ_INT(M_Plus(), 10);
    M_Collect(ITEM_STAR, 300.0f);
    CHECK_EQ_INT(M_Cherry(), 20);
    CHECK_EQ_INT(M_Plus(), 10);
    M_Teardown();
}

TEST(a_border_star_adds_cherry_partly_without_the_plus)
{
    M_Setup();
    M_Collect(ITEM_BORDER_STAR, 300.0f);
    CHECK_EQ_INT(M_Cherry(), 100);
    CHECK_EQ_INT(M_Plus(), 30);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(m_PopupCount, 0);
    M_Teardown();
}

TEST(a_cherry_item_adds_cherry_by_the_captures)
{
    M_Setup();
    g_GameManager.stats.spell_cards_captured = 3;
    M_Collect(ITEM_CHERRY, 300.0f);
    CHECK_EQ_INT(M_Cherry(), 1300);
    CHECK_EQ_INT(M_Plus(), 1300);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(m_PopupCount, 1);
    CHECK_EQ_INT(m_Popups[0].value, 1300);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_ARGB(0xff, 0xff, 0x40, 0x40));
    M_Teardown();
}

TEST(a_cherry_item_scores_as_a_point_item_once_the_cherry_is_full)
{
    M_Setup();
    g_GameManager.cherry.max = g_GameManager.cherry.value;
    const VEC3 pos = M_Collect(ITEM_CHERRY, M_ITEM_LINE + 2.2f + 3.9f);
    CHECK(pos.y > M_ITEM_LINE + 3.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 4970);
    CHECK_EQ_INT(m_PopupCount, 1);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_WHITE);
    CHECK_EQ_INT(M_Cherry(), 0);
    CHECK_EQ_INT(M_Plus(), 1000);
    M_Teardown();
}

TEST(a_boss_star_scores_by_the_graze_and_adds_100_cherry)
{
    M_Setup();
    g_GameManager.player.graze_total = 80;
    M_Collect(ITEM_BOSS_STAR, 300.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 32);
    CHECK_EQ_INT(M_Cherry(), 100);
    CHECK_EQ_INT(m_PopupCount, 1);
    CHECK_EQ_INT(m_Popups[0].value, 100);
    CHECK_EQ_INT(m_Popups[0].color, COLOR_ARGB(0xff, 0xff, 0x40, 0x40));
    g_GameManager.cherry.max = g_GameManager.cherry.value;
    M_Collect(ITEM_BOSS_STAR, 300.0f);
    CHECK_EQ_INT(m_PopupCount, 2);
    CHECK_EQ_INT(m_Popups[1].value, 320);
    CHECK_EQ_INT(m_Popups[1].color, COLOR_WHITE);
    M_Teardown();
}

TEST(every_collect_marks_the_item_event)
{
    M_Setup();
    Item_Spawn(
        (VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_BORDER_STAR, ITEM_FALLING);
    Item_Spawn(
        (VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_BORDER_STAR, ITEM_FALLING);
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_BORDER_STAR, ITEM_FALLING);
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK_EQ_INT(m_Events, REPLAY_EVENT_ITEM);
    CHECK_EQ_INT(m_EventMarks, 2);
    CHECK(M_IsQueued(SOUND_ITEM));
    Sound_Reset();
    M_Frame();
    CHECK_EQ_INT(m_EventMarks, 2);
    CHECK(!M_IsQueued(SOUND_ITEM));
    M_Teardown();
}

TEST(an_unknown_type_is_collected_without_effect)
{
    M_Setup();
    M_Collect((ITEM_TYPE)10, 300.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    CHECK_EQ_INT(m_EventMarks, 1);
    M_Teardown();
}

// Writes an ANM file whose script for ITEM_POWER waits at a stop, which
// shows each step of the script in its time.
static void M_WriteAnm(void)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    const int32_t script = AnmBuilder_BeginScript(anm, 0);
    const uint32_t sprite = 0;
    AnmBuilder_Instr(anm, script, 3, 0, 0, &sprite, 1);
    AnmBuilder_Instr(anm, script, 2, 1000, 0, nullptr, 0);
    AnmBuilder_End(anm, script);
    AnmBuilder_WriteFile(&builder, m_Directory, "item.anm");
}

static void M_LoadAnm(const char *const name)
{
    m_Directory = TestDir_Prepare(name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    M_WriteAnm();
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Anm_Load(0, "item.anm", M_FIRST_SCRIPT), LOG_LEVEL_ERROR));
}

static void M_UnloadAnm(void)
{
    Files_Shutdown();
    Mem_Free(m_Directory);
}

TEST(an_item_steps_its_script_until_it_is_collected)
{
    M_Setup();
    M_LoadAnm("script");
    Item_Spawn((VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    const ITEM *const item = &Item_GetSlots()[0];
    CHECK(item->vm.script != nullptr);
    const int32_t start = item->vm.script_time.current;
    M_Frame();
    CHECK_EQ_INT(item->vm.script_time.current, start + 1);
    CHECK_EQ_INT(item->timer.current, 1);
    M_Frame();
    CHECK_EQ_INT(item->vm.script_time.current, start + 2);
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK_EQ_INT(item->vm.script_time.current, start + 2);
    CHECK_EQ_INT(item->timer.current, 2);
    M_Teardown();
    M_UnloadAnm();
}

TEST(the_update_leaves_the_items_to_draw)
{
    M_Setup();
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_Spawn((VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_Spawn((VEC3) { 30.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    const ITEM *const items = Item_GetSlots();
    CHECK(!items[0].drawn);
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK(items[0].drawn && !items[1].drawn && items[2].drawn);
    g_GameManager.play_area.height = 0.0f;
    M_Frame();
    CHECK(!items[0].drawn && !items[2].drawn);
    M_Teardown();
}

TEST(item_drawing_keeps_positions_and_indicator_state_headlessly)
{
    M_Setup();
    g_GameManager.play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    Item_Spawn((VEC3) { 50.0f, -80.0f + 2.2f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { 70.0f, 120.0f + 2.2f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_Spawn((VEC3) { 90.0f, 120.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    ITEM *const items = (ITEM *)Item_GetSlots();
    items[0].pos.y = -80.0f;
    items[0].vm.z_write_off = false;
    items[1].pos.y = 120.0f;
    items[2].active = false;
    const RNG before = g_Rng;
    Item_Draw();
    CHECK(items[0].vm.pos.x == 82.0f);
    CHECK(items[0].vm.pos.y == 24.0f);
    CHECK(items[0].vm.pos.z == 0.01f);
    CHECK(!items[0].on_screen);
    CHECK(items[0].vm.z_write_off);
    CHECK_EQ_INT(items[0].vm.color >> 24, 80);
    CHECK(items[1].on_screen);
    CHECK(items[1].vm.pos.x == 102.0f);
    CHECK(items[1].vm.pos.y == 136.0f);
    CHECK_EQ_INT(g_Rng.seed, before.seed);
    CHECK_EQ_INT(g_Rng.generated, before.generated);
    M_Teardown();
}

TEST(item_draws_identify_each_item_left_by_the_update)
{
    M_Setup();
    M_LoadAnm("item_presentation");
    CHECK(IS_OK(Anm_CreateTexture(0, 64, 64, ANM_TEXTURE_RGBA8)));
    Item_Spawn((VEC3) { 50, 100, 0 }, ITEM_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { 70, 100, 0 }, ITEM_POWER, ITEM_FALLING);
    M_Frame();
    Item_Spawn((VEC3) { 90, 100, 0 }, ITEM_POWER, ITEM_FALLING);
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    const uint32_t generated = g_Rng.generated;
    Item_Draw();
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    CHECK(GfxRecorder_QuadId(0) != 0);
    CHECK(GfxRecorder_QuadId(0) != GfxRecorder_QuadId(1));
    CHECK_EQ_INT(g_Rng.generated, generated);
    GfxRecorder_Stop();
    M_Teardown();
    M_UnloadAnm();
}
