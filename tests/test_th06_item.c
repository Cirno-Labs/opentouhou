#include "th06/item.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/bullet.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/player.h"
#include "th06/sound.h"

#include "core/base/memory.h"
#include "core/gfx/private.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#include <SDL3/SDL.h>
#include <math.h>

#define M_FIRST_SCRIPT 0x215
#define M_TOUCH_X 50.0f
#define M_NO_TOUCH -1.0f

// The test links these instead of the modules they stand for. The
// graze box touches every item at x m_TouchX, and records where each item
// was tested.

PLAYER g_Player;

static VEC3 m_Probes[ITEM_COUNT];
static int32_t m_ProbeCount;
static float m_TouchX;
static float m_Angle;
static int32_t m_AngleCalls;
static VEC3 m_AngleFrom;
static int32_t m_ClearCalls;
static int32_t m_PopupCalls;
static char *m_Directory;

bool Player_TouchesGrazeBox(const VEC3 pos, const VEC3 size)
{
    CHECK(size.x == 16.0f && size.y == 16.0f && size.z == 16.0f);
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

void Bullet_ClearAllToItems(void)
{
    m_ClearCalls++;
}

void Gui_ShowFullPowerPopup(void)
{
    m_PopupCalls++;
}

static void M_Setup(void)
{
    Rng_SetSeed(&g_Rng, 7);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    AsciiText_Initialize();
    GameManager_Create(&g_GameManager);
    g_GameManager.rank.value = 10;
    g_GameManager.rank.max = 99;
    g_Player = (PLAYER) {};
    g_Player.pos.y = 400.0f;
    m_ProbeCount = 0;
    m_TouchX = M_NO_TOUCH;
    m_Angle = 1.0f;
    m_AngleCalls = 0;
    m_ClearCalls = 0;
    m_PopupCalls = 0;
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
    M_Teardown();
}

TEST(an_item_falls_out_at_sixteen_below_the_play_area)
{
    M_Setup();
    const float y = 100.0f + -2.2f;
    g_GameManager.play_area.height = y - 16.0f;
    CHECK(g_GameManager.play_area.height + 16.0f == y);
    Item_Spawn((VEC3) { 10.0f, 100.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 0);
    CHECK_EQ_INT(g_GameManager.rank.value, 9);
    CHECK_EQ_INT(g_GameManager.rank.sub, 97);
    M_Frame();
    CHECK_EQ_INT(g_GameManager.rank.sub, 97);
    M_Teardown();
}

TEST(an_item_above_the_fall_out_line_stays)
{
    M_Setup();
    const float y = 100.0f + -2.2f;
    g_GameManager.play_area.height = nextafterf(y - 16.0f, 1000.0f);
    CHECK(g_GameManager.play_area.height + 16.0f > y);
    Item_Spawn((VEC3) { 10.0f, 100.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 1);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
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
    M_Frame();
    CHECK(m_Probes[0].x == x && m_Probes[0].y == y + 0.03f);
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

TEST(a_thrown_item_never_homes_at_full_power)
{
    M_Setup();
    g_GameManager.player.power = 128;
    g_Player.pos.y = 0.0f;
    Item_Spawn((VEC3) { 200.0f, 300.0f, 0.0f }, ITEM_POWER, ITEM_THROWN);
    for (int32_t frame = 0; frame < 80; frame++) {
        M_Frame();
    }
    CHECK_EQ_INT(m_ProbeCount, 1);
    CHECK_EQ_INT(m_AngleCalls, 0);
    M_Teardown();
}

TEST(full_power_above_the_line_makes_items_home_for_good)
{
    M_Setup();
    g_GameManager.player.power = 128;
    g_Player.pos.y = 127.5f;
    m_Angle = 2.0f;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    float vx;
    float vy;
    X87_SinCosScale(2.0f, 8.0f, &vx, &vy);
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 1);
    CHECK(m_AngleFrom.x == 10.0f && m_AngleFrom.y == 300.0f);
    CHECK(m_Probes[0].x == 10.0f + vx && m_Probes[0].y == 300.0f + vy);
    const VEC3 last = m_Probes[0];
    g_Player.pos.y = 300.0f;
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 2);
    CHECK(m_Probes[0].x == last.x + vx && m_Probes[0].y == last.y + vy);
    M_Teardown();
}

TEST(items_do_not_home_at_the_line)
{
    M_Setup();
    g_GameManager.player.power = 128;
    g_Player.pos.y = 128.0f;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 0);
    M_Teardown();
}

TEST(items_do_not_home_below_full_power)
{
    M_Setup();
    g_GameManager.player.power = 127;
    g_Player.pos.y = 0.0f;
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 0);
    M_Teardown();
}

TEST(collect_all_makes_every_item_home)
{
    M_Setup();
    Item_Spawn((VEC3) { 10.0f, 300.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    Item_Spawn((VEC3) { 20.0f, 300.0f, 0.0f }, ITEM_POWER, ITEM_THROWN);
    Item_CollectAll();
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 2);
    M_Teardown();
}

TEST(a_full_pool_drops_nothing)
{
    M_Setup();
    M_FillPool();
    const int32_t generated = g_Rng.generated;
    Item_Spawn((VEC3) { 1000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_THROWN);
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
    Item_Spawn((VEC3) { 1000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, ITEM_COUNT);
    CHECK(m_Probes[5].x == 1000.0f);
    M_Teardown();
}

TEST(a_spawn_searches_past_the_end_of_the_pool)
{
    M_Setup();
    M_FillPool();
    M_Free(3.0f);
    Item_Spawn((VEC3) { 1000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Free(0.0f);
    Item_Spawn((VEC3) { 2000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, ITEM_COUNT);
    CHECK(m_Probes[0].x == 2000.0f);
    CHECK(m_Probes[3].x == 1000.0f);
    M_Teardown();
}

TEST(a_spawn_finds_a_free_slot_just_before_the_start)
{
    M_Setup();
    M_FillPool();
    M_Free((float)(ITEM_COUNT - 1));
    Item_Spawn((VEC3) { 1000.0f, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, ITEM_COUNT);
    CHECK(m_Probes[ITEM_COUNT - 1].x == 1000.0f);
    M_Teardown();
}

TEST(reset_removes_every_item)
{
    M_Setup();
    for (int32_t i = 0; i < 3; i++) {
        Item_Spawn((VEC3) { (float)i, 0.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    }
    Item_Reset();
    M_Frame();
    CHECK_EQ_INT(m_ProbeCount, 0);
    M_Teardown();
}

TEST(a_power_item_below_full_power_adds_one_power)
{
    M_Setup();
    g_GameManager.player.power = 10;
    g_GameManager.player.power_item_combo = 5;
    M_Collect(ITEM_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power, 11);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 0);
    CHECK_EQ_INT(g_GameManager.score.value, 10);
    CHECK_EQ_INT(g_GameManager.rank.sub, 1);
    CHECK_EQ_INT(m_ClearCalls, 0);
    CHECK_EQ_INT(g_AsciiText.next_popup, 1);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[0], 0);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[1], 1);
    M_Teardown();
}

TEST(reaching_full_power_clears_the_bullets)
{
    M_Setup();
    g_GameManager.player.power = 127;
    M_Collect(ITEM_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power, 128);
    CHECK_EQ_INT(m_ClearCalls, 1);
    CHECK_EQ_INT(m_PopupCalls, 1);
    CHECK_EQ_INT(g_GameManager.score.value, 10);
    CHECK_EQ_INT(g_AsciiText.popups[0].digit_count, 1);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[0], 10);
    CHECK_EQ_INT(g_AsciiText.popups[0].color, 0xff80c0ffu);
    M_Teardown();
}

TEST(a_power_item_at_full_power_scores_by_the_combo)
{
    M_Setup();
    g_GameManager.player.power = 128;
    g_GameManager.player.power_item_combo = 0;
    M_Collect(ITEM_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 1);
    CHECK_EQ_INT(g_GameManager.score.value, 20);
    CHECK_EQ_INT(g_GameManager.rank.sub, 1);
    CHECK_EQ_INT(m_ClearCalls, 0);
    M_Teardown();
}

TEST(the_power_item_combo_stops_at_thirty)
{
    M_Setup();
    g_GameManager.player.power = 128;
    g_GameManager.player.power_item_combo = 30;
    M_Collect(ITEM_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 30);
    CHECK_EQ_INT(g_GameManager.score.value, 51200);
    M_Teardown();
}

TEST(a_negative_power_item_combo_takes_the_last_bonus)
{
    M_Setup();
    g_GameManager.player.power = 128;
    g_GameManager.player.power_item_combo = -5;
    M_Collect(ITEM_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 30);
    CHECK_EQ_INT(g_GameManager.score.value, 51200);
    M_Teardown();
}

TEST(a_big_power_item_keeps_the_combo_and_the_rank)
{
    M_Setup();
    g_GameManager.player.power = 10;
    g_GameManager.player.power_item_combo = 5;
    M_Collect(ITEM_BIG_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power, 18);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 5);
    CHECK_EQ_INT(g_GameManager.score.value, 10);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    CHECK_EQ_INT(m_ClearCalls, 0);
    M_Teardown();
}

TEST(a_big_power_item_stops_at_full_power)
{
    M_Setup();
    g_GameManager.player.power = 125;
    M_Collect(ITEM_BIG_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power, 128);
    CHECK_EQ_INT(m_ClearCalls, 1);
    CHECK_EQ_INT(m_PopupCalls, 1);
    M_Teardown();
}

TEST(a_big_power_item_at_full_power_adds_eight_to_the_combo)
{
    M_Setup();
    g_GameManager.player.power = 128;
    g_GameManager.player.power_item_combo = 5;
    M_Collect(ITEM_BIG_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 13);
    CHECK_EQ_INT(g_GameManager.score.value, 500);
    g_GameManager.player.power_item_combo = 25;
    M_Collect(ITEM_BIG_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 30);
    CHECK_EQ_INT(g_GameManager.score.value, 500 + 51200);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    M_Teardown();
}

TEST(a_point_item_scores_less_the_lower_it_is_collected)
{
    static const uint32_t scores[] = { 52800, 52800, 87040, 130560, 171200 };
    for (int32_t difficulty = 0; difficulty < 5; difficulty++) {
        M_Setup();
        g_GameManager.run.difficulty = difficulty;
        const VEC3 pos = M_Collect(ITEM_POINT, 202.5f);
        CHECK(pos.y > 200.0f && pos.y < 201.0f);
        CHECK_EQ_INT(g_GameManager.score.value, scores[difficulty]);
        CHECK_EQ_INT(g_GameManager.player.point_items, 1);
        CHECK_EQ_INT(g_GameManager.player.point_items_total, 1);
        CHECK_EQ_INT(g_GameManager.rank.sub, 3);
        M_Teardown();
    }
}

TEST(a_point_item_scores_the_most_above_pixel_128)
{
    M_Setup();
    g_GameManager.run.difficulty = 1;
    const VEC3 pos = M_Collect(ITEM_POINT, 130.0f);
    CHECK(pos.y > 127.0f && pos.y < 128.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 100000);
    CHECK_EQ_INT(g_GameManager.rank.sub, 30);
    M_Teardown();
}

TEST(a_point_item_scores_by_the_whole_pixel)
{
    M_Setup();
    g_GameManager.run.difficulty = 1;
    const VEC3 pos = M_Collect(ITEM_POINT, 130.9f);
    CHECK(pos.y > 128.0f && pos.y < 129.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 60000);
    CHECK_EQ_INT(g_GameManager.rank.sub, 3);
    M_Teardown();
}

TEST(a_bomb_item_adds_a_bomb_up_to_eight)
{
    M_Setup();
    g_GameManager.player.bombs = 7;
    M_Collect(ITEM_BOMB, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.bombs, 8);
    M_Collect(ITEM_BOMB, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.bombs, 8);
    CHECK_EQ_INT(g_GameManager.rank.sub, 10);
    M_Teardown();
}

TEST(a_life_item_adds_a_life_up_to_eight)
{
    M_Setup();
    g_GameManager.player.lives = 7;
    M_Collect(ITEM_LIFE, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.lives, 8);
    CHECK_EQ_INT(g_GameManager.rank.value, 12);
    M_Collect(ITEM_LIFE, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.lives, 8);
    CHECK_EQ_INT(g_GameManager.rank.value, 14);
    M_Teardown();
}

TEST(a_full_power_item_fills_the_power)
{
    M_Setup();
    g_GameManager.player.power = 20;
    M_Collect(ITEM_FULL_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.player.power, 128);
    CHECK_EQ_INT(g_GameManager.score.value, 1000);
    CHECK_EQ_INT(m_ClearCalls, 1);
    CHECK_EQ_INT(m_PopupCalls, 1);
    CHECK_EQ_INT(g_AsciiText.next_popup, 2);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[0], 10);
    CHECK_EQ_INT(g_AsciiText.popups[1].digit_count, 4);
    M_Collect(ITEM_FULL_POWER, 300.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 2000);
    CHECK_EQ_INT(m_ClearCalls, 1);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    M_Teardown();
}

TEST(a_star_item_scores_by_the_graze)
{
    M_Setup();
    g_GameManager.player.graze = 100;
    M_Collect(ITEM_STAR, 300.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 830);
    CHECK_EQ_INT(g_AsciiText.next_popup, 0);
    CHECK_EQ_INT(g_AsciiText.next_popup2, 1);
    CHECK_EQ_INT(g_AsciiText.popups[512].digits[1], 3);
    M_Teardown();
}

TEST(a_star_item_scores_100_in_a_bomb)
{
    M_Setup();
    g_GameManager.player.graze = 100;
    g_Player.bomb.running = true;
    M_Collect(ITEM_STAR, 300.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 100);
    M_Teardown();
}

static void M_CheckQueue(const int32_t first, const int32_t second)
{
    CHECK_EQ_INT(g_SoundEvents.queue[0], first);
    CHECK_EQ_INT(g_SoundEvents.queue[1], second);
    Sound_Reset();
}

TEST(a_collected_item_queues_the_item_sound)
{
    M_Setup();
    g_GameManager.player.power = 10;
    Sound_Reset();
    M_Collect(ITEM_POWER, 300.0f);
    M_CheckQueue(SOUND_ITEM, -1);
    M_Frame();
    M_CheckQueue(-1, -1);
    M_Teardown();
}

TEST(a_new_power_level_queues_the_power_up_sound)
{
    M_Setup();
    g_GameManager.player.power = 7;
    Sound_Reset();
    M_Collect(ITEM_POWER, 300.0f);
    M_CheckQueue(SOUND_POWER_UP, SOUND_ITEM);
    M_Teardown();
}

TEST(a_life_item_queues_the_extend_sound_also_at_eight_lives)
{
    M_Setup();
    g_GameManager.player.lives = 7;
    Sound_Reset();
    M_Collect(ITEM_LIFE, 300.0f);
    M_CheckQueue(SOUND_EXTEND, SOUND_ITEM);
    M_Collect(ITEM_LIFE, 300.0f);
    M_CheckQueue(SOUND_EXTEND, SOUND_ITEM);
    M_Teardown();
}

TEST(a_full_power_item_queues_the_power_up_sound_below_full_power)
{
    M_Setup();
    g_GameManager.player.power = 20;
    Sound_Reset();
    M_Collect(ITEM_FULL_POWER, 300.0f);
    M_CheckQueue(SOUND_POWER_UP, SOUND_ITEM);
    M_Collect(ITEM_FULL_POWER, 300.0f);
    M_CheckQueue(SOUND_ITEM, -1);
    M_Teardown();
}

TEST(an_unknown_type_is_collected_without_effect)
{
    M_Setup();
    M_Collect((ITEM_TYPE)7, 300.0f);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(g_GameManager.rank.value, 10);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
    M_Teardown();
}

// Writes an ANM file whose first script draws a random sprite on each of
// its first 20 frames, which shows each step of the script as a draw from
// g_Rng.
static void M_WriteAnm(void)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    AnmBuilder_AddSprite(&builder, 0, 0, 0, 16, 16);
    AnmBuilder_AddSprite(&builder, 1, 16, 0, 32, 16);
    const int32_t script = AnmBuilder_BeginScript(&builder, 0);
    const uint32_t args[] = { 0, 2 };
    for (int16_t time = 0; time < 20; time++) {
        AnmBuilder_Instr(&builder, script, time, 16, args, 2);
    }
    AnmBuilder_Instr(&builder, script, 1000, 0, nullptr, 0);
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, "item.anm", bytes, size);
}

TEST(an_item_steps_its_script_until_it_is_collected)
{
    M_Setup();
    m_Directory = TestDir_Prepare("script");
    Th06Data_WriteArchives(m_Directory, -1);
    M_WriteAnm();
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Anm_Load(0, "item.anm", M_FIRST_SCRIPT), LOG_LEVEL_ERROR));
    Item_Spawn((VEC3) { M_TOUCH_X, 300.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    CHECK_EQ_INT(g_Rng.generated, 1);
    M_Frame();
    CHECK_EQ_INT(g_Rng.generated, 2);
    M_Frame();
    CHECK_EQ_INT(g_Rng.generated, 3);
    m_TouchX = M_TOUCH_X;
    M_Frame();
    CHECK_EQ_INT(g_Rng.generated, 3);
    M_Teardown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

TEST(item_drawing_keeps_positions_and_indicator_state_headlessly)
{
    M_Setup();
    g_GameManager.play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    Item_Spawn((VEC3) { 50.0f, -80.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { 70.0f, 120.0f, 0.0f }, ITEM_POINT, ITEM_FALLING);
    const ITEM *const items = Item_GetSlots();
    const RNG before = g_Rng;
    Item_Draw();
    CHECK(items[0].pos.y == -80.0f);
    CHECK(items[0].vm.pos.x == 82.0f);
    CHECK(items[0].vm.pos.y == 24.0f);
    CHECK(items[0].vm.pos.z == 0.01f);
    CHECK(!items[0].on_screen);
    CHECK_EQ_INT(items[0].vm.color >> 24, 80);
    CHECK(items[1].on_screen);
    CHECK(items[1].vm.pos.x == 102.0f);
    CHECK(items[1].vm.pos.y == 136.0f);
    CHECK_EQ_INT(g_Rng.seed, before.seed);
    CHECK_EQ_INT(g_Rng.generated, before.generated);
    M_Teardown();
}

TEST(item_draws_identify_each_spawn)
{
    M_Setup();
    m_Directory = TestDir_Prepare("item_presentation");
    Th06Data_WriteArchives(m_Directory, -1);
    M_WriteAnm();
    CHECK(IS_OK(Files_Init(m_Directory)));
    CHECK(IS_OK(Anm_Load(0, "item.anm", M_FIRST_SCRIPT)));
    CHECK(IS_OK(Anm_CreateTexture(0, 64, 64, ANM_TEXTURE_RGBA8)));
    Item_Spawn((VEC3) { 50, 100, 0 }, ITEM_POWER, ITEM_FALLING);
    Item_Spawn((VEC3) { 70, 100, 0 }, ITEM_POWER, ITEM_FALLING);
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
    Files_Shutdown();
    Mem_Free(m_Directory);
}
