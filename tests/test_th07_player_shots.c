#include "th07/player/private.h"

#include "gfx_recorder.h"
#include "test.h"

#include "th07/anm.h"
#include "th07/effect.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/player.h"
#include "th07/sound.h"

#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <string.h>

#define I(x) { .i = (x) }
#define INSTR(op_, time_, ...)                                                 \
    {                                                                          \
        .opcode = (op_),                                                       \
        .time = (time_),                                                       \
        .args = { __VA_ARGS__ },                                               \
    }
#define M_SPRITE 0
#define M_SPRITE_OP 3
#define M_END_OP -1
#define M_SCRIPT_SHOT 0x440
#define M_SCRIPT_HIT 0x460
#define M_MAX_CALLS 64
#define M_EFFECT_SHOT_HIT ((EFFECT_TYPE)(EFFECT_PARTICLE + 1))

// The test links these instead of the modules they stand for, and keeps
// th07/game, which would bring in every scene, out.

typedef struct {
    EFFECT_TYPE type;
    VEC3 pos;
} M_EFFECT_CALL;

PLAYER g_Player;
RNG g_Rng;
float g_GameSpeed;

static bool m_MsgRunning;
static M_EFFECT_CALL m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static EFFECT m_SpareEffect;

static ANM_INSTR m_LongInstrs[] = {
    INSTR(M_SPRITE_OP, 0, I(M_SPRITE)),
    INSTR(M_END_OP, 1000),
};
static ANM_INSTR m_ShortInstrs[] = {
    INSTR(M_SPRITE_OP, 0, I(M_SPRITE)),
    INSTR(M_END_OP, 2),
};
static ANM_SCRIPT m_Long = { m_LongInstrs, 2, 0 };
static ANM_SCRIPT m_Short = { m_ShortInstrs, 2, 0 };

static SHOT m_Shots[4];
static SHOT m_FocusedShots[1];
static SHOT_TABLE m_Tables[2];
static SHOT_TABLE m_FocusedTables[1];

bool Gui_IsMsgRunning(void)
{
    return m_MsgRunning;
}

EFFECT *Effect_Spawn(
    const EFFECT_TYPE type, const VEC3 pos, const int32_t count,
    const uint32_t color)
{
    if (m_EffectCount < M_MAX_CALLS) {
        m_Effects[m_EffectCount] = (M_EFFECT_CALL) { type, pos };
    }
    m_EffectCount++;
    return &m_SpareEffect;
}

static SHOT M_Plain(void)
{
    return (SHOT) {
        .interval = 1,
        .size_x = 12.0f,
        .size_y = 12.0f,
        .angle = -MATH_HALF_PI,
        .speed = 10.0f,
        .damage = 30,
        .script = M_SCRIPT_SHOT,
        .sound = -1,
    };
}

// Gives the player one table of count shots past full power, and a focused
// table of one shot.
static void M_SetShots(const int32_t count)
{
    m_Tables[0] = (SHOT_TABLE) { 999, count, m_Shots };
    g_Player.shot_type = (SHOT_TYPE) { .table_count = 1, .tables = m_Tables };
    m_FocusedTables[0] = (SHOT_TABLE) { 999, 1, m_FocusedShots };
    g_Player.shot_type_focused =
        (SHOT_TYPE) { .table_count = 1, .tables = m_FocusedTables };
}

static void M_SetUp(void)
{
    static const ANM_SPRITE sprite = {
        .slot = 0,
        .texture_width = 256.0f,
        .texture_height = 256.0f,
        .u1 = 0.0625f,
        .v1 = 0.0625f,
        .width = 16.0f,
        .height = 16.0f,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
    };
    GfxRecorder_Start();
    Rng_SetSeed(&g_Rng, 1234);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    CHECK(Result_Log(
        Anm_CreateTexture(0, 256, 256, ANM_TEXTURE_RGBA8), LOG_LEVEL_ERROR));
    Anm_SetSpriteEntry(M_SPRITE, &sprite);
    for (int32_t i = 0x440; i < 0x460; i++) {
        Anm_SetScriptEntry(i, &m_Long);
    }
    for (int32_t i = 0x460; i < 0x480; i++) {
        Anm_SetScriptEntry(i, &m_Short);
    }
    memset(&g_GameManager, 0, sizeof(g_GameManager));
    g_GameManager.play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    g_GameManager.camera.viewport = (VIEWPORT) { 0, 0, 640, 480, 0, 1 };
    g_GameSpeed = 1.0f;
    memset(&g_Player, 0, sizeof(g_Player));
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        Anm_InitVm(&g_Player.shots.bullets[i].vm);
    }
    g_Player.pos = (VEC3) { 192.0f, 384.0f, 0.0f };
    g_Player.options.pos[0] = (VEC3) { 168.0f, 384.0f, 0.0f };
    g_Player.options.pos[1] = (VEC3) { 216.0f, 384.0f, 0.0f };
    g_Player.options.state = PLAYER_OPTIONS_OUT;
    g_Player.homing_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    g_Player.aim_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    Timer_Set(&g_Player.state.timer, 0);
    Timer_Set(&g_Player.shots.fire_timer, -1);
    for (int32_t i = 0; i < 4; i++) {
        m_Shots[i] = M_Plain();
    }
    m_FocusedShots[0] = M_Plain();
    m_FocusedShots[0].damage = 99;
    M_SetShots(1);
    Sound_Reset();
    m_MsgRunning = false;
    m_EffectCount = 0;
}

static void M_TearDown(void)
{
    Anm_Shutdown();
    GfxRecorder_Stop();
}

static PLAYER_BULLET *M_Bullet(const int32_t index)
{
    return &g_Player.shots.bullets[index];
}

static int32_t M_Flying(void)
{
    int32_t count = 0;
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        count += M_Bullet(i)->state == PLAYER_BULLET_FLYING;
    }
    return count;
}

// Fires one plain shot into bullet 0 and returns it.
static PLAYER_BULLET *M_FireOne(const SHOT shot)
{
    m_Shots[0] = shot;
    M_SetShots(1);
    Player_FireShots(0);
    CHECK(M_Bullet(0)->state == PLAYER_BULLET_FLYING);
    return M_Bullet(0);
}

static int32_t M_Damage(const VEC3 pos, const VEC3 size)
{
    bool hit_by_bomb = false;
    return Player_CalcDamage(pos, size, &hit_by_bomb);
}

TEST(a_shot_starts_from_its_option_with_its_offset_and_velocity)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.option = 2;
    shot.offset_x = 3.0f;
    shot.offset_y = -5.0f;
    shot.size_y = 24.0f;
    shot.angle = -1.2f;
    shot.kind = SHOT_KIND_PIERCING;
    shot.sound = SOUND_PLAYER_SHOT;
    const PLAYER_BULLET *const bullet = M_FireOne(shot);
    CHECK(bullet->pos.x == 219.0f && bullet->pos.y == 379.0f);
    CHECK(bullet->pos.z == 0.495f);
    CHECK(bullet->size.x == 12.0f && bullet->size.y == 24.0f);
    CHECK(bullet->size.z == 1.0f);
    CHECK(bullet->angle == -1.2f && bullet->speed == 10.0f);
    float x;
    float y;
    X87_SinCosScale(-1.2f, 10.0f, &x, &y);
    CHECK(bullet->velocity.x == x && bullet->velocity.y == y);
    CHECK_EQ_INT(bullet->kind, SHOT_KIND_PIERCING);
    CHECK_EQ_INT(bullet->damage, 30);
    CHECK_EQ_INT(bullet->vm.script_index, M_SCRIPT_SHOT);
    CHECK(bullet->vm.z_write_off);
    CHECK(bullet->shot == &m_Shots[0]);
    CHECK_EQ_INT(bullet->timer.current, 0);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_PLAYER_SHOT);
    M_TearDown();
}

TEST(a_shot_velocity_rounds_its_trig_product_once)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.angle = -1.57079637f;
    shot.speed = 12.0f;
    const PLAYER_BULLET *const bullet = M_FireOne(shot);
    // Rounding the cosine to float first gives -5.2453663e-07.
    CHECK(bullet->velocity.x == -5.245367e-07f);
    CHECK(bullet->velocity.y == -12.0f);
    M_TearDown();
}

TEST(a_plain_shot_fires_where_the_frame_modulo_its_interval_is_its_phase)
{
    M_SetUp();
    m_Shots[0].interval = 5;
    m_Shots[0].phase = 2;
    for (int32_t frame = 0; frame < 15; frame++) {
        Player_FireShots(frame);
    }
    CHECK_EQ_INT(M_Flying(), 3);
    M_TearDown();
}

TEST(a_shot_that_does_not_fire_leaves_its_bullet_for_the_next_shot)
{
    M_SetUp();
    m_Shots[0].interval = 2;
    m_Shots[0].phase = 1;
    m_Shots[0].damage = 1;
    m_Shots[1].damage = 2;
    m_Shots[2].interval = 2;
    m_Shots[2].phase = 1;
    m_Shots[2].damage = 3;
    M_SetShots(3);
    Player_FireShots(0);
    CHECK_EQ_INT(M_Flying(), 1);
    CHECK_EQ_INT(M_Bullet(0)->damage, 2);
    Player_FireShots(1);
    CHECK_EQ_INT(M_Flying(), 4);
    CHECK_EQ_INT(M_Bullet(1)->damage, 1);
    CHECK_EQ_INT(M_Bullet(2)->damage, 2);
    CHECK_EQ_INT(M_Bullet(3)->damage, 3);
    M_TearDown();
}

TEST(shots_past_the_last_free_bullet_do_not_fire)
{
    M_SetUp();
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT - 1; i++) {
        M_Bullet(i)->state = PLAYER_BULLET_HIT;
    }
    M_SetShots(3);
    Player_FireShots(0);
    CHECK_EQ_INT(M_Flying(), 1);
    CHECK(M_Bullet(PLAYER_BULLET_COUNT - 1)->shot == &m_Shots[0]);
    M_TearDown();
}

TEST(the_table_past_the_power_and_the_focused_type_fire)
{
    M_SetUp();
    m_Shots[0].damage = 1;
    m_Shots[1].damage = 2;
    m_Tables[0] = (SHOT_TABLE) { 8, 1, &m_Shots[0] };
    m_Tables[1] = (SHOT_TABLE) { 999, 1, &m_Shots[1] };
    g_Player.shot_type.table_count = 2;
    g_GameManager.player.power = 7.9f;
    Player_FireShots(0);
    CHECK_EQ_INT(M_Bullet(0)->damage, 1);
    g_GameManager.player.power = 8.0f;
    Player_FireShots(0);
    CHECK_EQ_INT(M_Bullet(1)->damage, 2);
    g_Player.move.focused = true;
    Player_FireShots(0);
    CHECK_EQ_INT(M_Bullet(2)->damage, 99);
    M_TearDown();
}

TEST(a_homing_fire_turns_to_the_aim_target_at_one_and_a_half_speed)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.fire = SHOT_FIRE_HOMING;
    shot.angle = -1.4f;
    const PLAYER_BULLET *bullet = M_FireOne(shot);
    CHECK(bullet->angle == -1.4f);
    CHECK(bullet->velocity.x == X87_Cos(-1.4f) * 10.0f);
    M_Bullet(0)->state = PLAYER_BULLET_FREE;
    g_Player.aim_target = (VEC3) { 100.0f, 200.0f, 0.0f };
    bullet = M_FireOne(shot);
    const float angle = Angle_AddNormalize(
        X87_Atan2(200.0f - 384.0f, 100.0f - 192.0f), -1.4f + MATH_HALF_PI);
    float x;
    float y;
    X87_SinCosScale(angle, 10.0f * 1.5f, &x, &y);
    CHECK(bullet->angle == angle);
    CHECK(bullet->velocity.x == x && bullet->velocity.y == y);
    M_TearDown();
}

TEST(an_aimed_fire_turns_by_the_aim_angle)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.fire = SHOT_FIRE_AIMED;
    shot.angle = -1.3f;
    g_Player.shots.aim_angle = -2.0f;
    const PLAYER_BULLET *const bullet = M_FireOne(shot);
    const float angle = Angle_AddNormalize(-2.0f, -1.3f + MATH_HALF_PI);
    float x;
    float y;
    X87_SinCosScale(angle, 10.0f, &x, &y);
    CHECK(bullet->angle == angle);
    CHECK(bullet->velocity.x == x && bullet->velocity.y == y);
    M_TearDown();
}

TEST(a_laser_fires_once_while_the_options_are_out)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.fire = SHOT_FIRE_LASER;
    shot.kind = SHOT_KIND_LASER;
    shot.interval = 120;
    shot.phase = 1;
    shot.option = 2;
    shot.offset_x = 4.0f;
    m_Shots[0] = shot;
    g_Player.options.state = PLAYER_OPTIONS_IN;
    Player_FireShots(0);
    CHECK_EQ_INT(M_Flying(), 0);
    g_Player.options.state = PLAYER_OPTIONS_OUT;
    Player_FireShots(0);
    Player_FireShots(1);
    CHECK_EQ_INT(M_Flying(), 1);
    const PLAYER_LASER *const laser = &g_Player.shots.lasers[1];
    CHECK(laser->bullet == M_Bullet(0));
    CHECK(laser->shot == &m_Shots[0]);
    CHECK_EQ_INT(laser->timer.current, 120);
    CHECK_EQ_INT(M_Bullet(0)->laser, 1);
    CHECK_EQ_INT(M_Bullet(0)->option, 2);
    CHECK(M_Bullet(0)->laser_offset_x == 4.0f);
    M_TearDown();
}

TEST(a_laser_of_another_shot_is_interrupted_and_its_entry_freed)
{
    M_SetUp();
    m_Shots[0].fire = SHOT_FIRE_LASER;
    m_Shots[0].interval = 120;
    m_Shots[1] = m_Shots[0];
    M_SetShots(1);
    Player_FireShots(0);
    m_Tables[0].shots = &m_Shots[1];
    Player_FireShots(0);
    CHECK_EQ_INT(M_Bullet(0)->vm.pending_interrupt, 1);
    CHECK(g_Player.shots.lasers[0].bullet == nullptr);
    CHECK_EQ_INT(M_Flying(), 1);
    Player_FireShots(0);
    CHECK(g_Player.shots.lasers[0].bullet == M_Bullet(1));
    M_TearDown();
}

TEST(a_trail_laser_fires_while_the_options_are_in_with_an_empty_trail)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.fire = SHOT_FIRE_TRAIL_LASER;
    shot.interval = 12;
    shot.phase = 2;
    m_Shots[0] = shot;
    Player_FireShots(0);
    CHECK_EQ_INT(M_Flying(), 0);
    g_Player.options.state = PLAYER_OPTIONS_IN;
    Player_FireShots(0);
    const PLAYER_BULLET *const bullet = M_Bullet(0);
    CHECK_EQ_INT(g_Player.shots.lasers[2].timer.current, 999);
    CHECK_EQ_INT(bullet->trail_length, 12);
    CHECK(bullet->pos.x == -999.0f);
    for (int32_t i = 0; i < PLAYER_TRAIL_COUNT; i++) {
        CHECK(bullet->trail[i].x == -999.0f);
    }
    M_TearDown();
}

TEST(lasers_go_with_their_options_and_all_go_at_a_death)
{
    M_SetUp();
    M_SetShots(3);
    Player_FireShots(0);
    for (int32_t i = 0; i < PLAYER_LASER_COUNT; i++) {
        M_Bullet(i)->vm.stopped = true;
        g_Player.shots.lasers[i].bullet = M_Bullet(i);
        Timer_Set(&g_Player.shots.lasers[i].timer, 100);
    }
    g_Player.options.state = PLAYER_OPTIONS_SLIDING_IN;
    Player_UpdateShots();
    CHECK_EQ_INT(M_Bullet(2)->state, PLAYER_BULLET_FREE);
    // The interrupt reached the script, which has no label for it.
    CHECK(!M_Bullet(0)->vm.stopped && !M_Bullet(1)->vm.stopped);
    CHECK_EQ_INT(M_Bullet(0)->state, PLAYER_BULLET_FLYING);
    for (int32_t i = 0; i < PLAYER_LASER_COUNT; i++) {
        CHECK(g_Player.shots.lasers[i].bullet == nullptr);
    }
    g_Player.options.state = PLAYER_OPTIONS_OUT;
    g_Player.state.value = PLAYER_DYING;
    g_Player.shots.lasers[0].bullet = M_Bullet(0);
    Player_UpdateShots();
    CHECK(g_Player.shots.lasers[0].bullet == nullptr);
    CHECK_EQ_INT(M_Bullet(0)->state, PLAYER_BULLET_FREE);
    M_TearDown();
}

TEST(laser_timers_run_down_at_the_game_speed_and_cut_to_50_when_idle)
{
    M_SetUp();
    PLAYER_LASER *const lasers = g_Player.shots.lasers;
    PLAYER_BULLET bullets[2] = {};
    lasers[0].bullet = &bullets[0];
    lasers[1].bullet = &bullets[1];
    Timer_Set(&lasers[0].timer, 100);
    Timer_Set(&lasers[1].timer, 1);
    Timer_Set(&g_Player.shots.fire_timer, 0);
    g_GameSpeed = 0.5f;
    Player_UpdateShots();
    CHECK_EQ_INT(lasers[0].timer.current, 99);
    CHECK(lasers[1].bullet == nullptr);
    Player_UpdateShots();
    CHECK_EQ_INT(lasers[0].timer.current, 99);
    g_GameSpeed = 1.0f;
    Player_UpdateShots();
    CHECK_EQ_INT(lasers[0].timer.current, 98);
    Timer_Set(&g_Player.shots.fire_timer, -1);
    Player_UpdateShots();
    CHECK_EQ_INT(lasers[0].timer.current, 50);
    M_TearDown();
}

TEST(a_full_speed_trail_laser_timer_never_runs_down)
{
    M_SetUp();
    g_Player.options.state = PLAYER_OPTIONS_IN;
    PLAYER_BULLET bullet = {};
    PLAYER_LASER *const laser = &g_Player.shots.lasers[2];
    laser->bullet = &bullet;
    Timer_Set(&laser->timer, 999);
    Timer_Set(&g_Player.shots.fire_timer, 0);
    Player_UpdateShots();
    CHECK_EQ_INT(laser->timer.current, 999);
    M_TearDown();
}

TEST(a_shot_moves_by_its_velocity_at_the_game_speed)
{
    M_SetUp();
    PLAYER_BULLET *const bullet = M_FireOne(M_Plain());
    bullet->velocity = (VEC3) { 0.3f, -0.7f, 0.0f };
    const VEC3 start = bullet->pos;
    g_GameSpeed = 0.5f;
    Player_UpdateShots();
    CHECK(bullet->pos.x == start.x + 0.5f * 0.3f);
    CHECK(bullet->pos.y == start.y + 0.5f * -0.7f);
    CHECK(bullet->timer.sub_frame == 0.5f);
    M_TearDown();
}

TEST(a_shot_outside_the_play_area_is_freed_but_not_a_laser)
{
    M_SetUp();
    PLAYER_BULLET *const bullet = M_FireOne(M_Plain());
    bullet->pos.y = -20.0f;
    Player_UpdateShots();
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_FREE);
    CHECK_EQ_INT(bullet->timer.current, 1);
    SHOT shot = M_Plain();
    shot.kind = SHOT_KIND_LASER;
    M_FireOne(shot)->pos.y = -20.0f;
    Player_UpdateShots();
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_FLYING);
    M_TearDown();
}

TEST(a_homing_shot_steers_to_the_homing_target_for_40_frames)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.move = SHOT_MOVE_HOMING;
    shot.speed = 6.0f;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    g_Player.homing_target = (VEC3) { 100.0f, 100.0f, 0.0f };
    const VEC3 pos = bullet->pos;
    const VEC3 v = bullet->velocity;
    Player_UpdateShots();
    float x = 100.0f - pos.x;
    float y = 100.0f - pos.y;
    float len = X87_Sqrt(x * x + y * y) / (6.0f / 4.0f);
    x = x / len + v.x;
    y = y / len + v.y;
    len = X87_Sqrt(x * x + y * y);
    const float speed = len > 10.0f ? 10.0f : len;
    CHECK(bullet->speed == speed);
    CHECK(bullet->velocity.x == x * speed / len);
    CHECK(bullet->velocity.y == y * speed / len);
    M_TearDown();
}

TEST(homing_shots_speed_up_to_their_limit_without_a_target)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.move = SHOT_MOVE_HOMING;
    shot.speed = 9.9f;
    PLAYER_BULLET *bullet = M_FireOne(shot);
    Player_UpdateShots();
    CHECK(bullet->speed == 9.9f + 0.33333334f);
    Player_UpdateShots();
    CHECK(bullet->speed == 9.9f + 0.33333334f);
    bullet->state = PLAYER_BULLET_FREE;
    shot.move = SHOT_MOVE_HOMING_FAST;
    shot.speed = 17.9f;
    bullet = M_FireOne(shot);
    Player_UpdateShots();
    CHECK(bullet->speed == 17.9f + 0.6f);
    Player_UpdateShots();
    CHECK(bullet->speed == 17.9f + 0.6f);
    M_TearDown();
}

TEST(a_rising_shot_speeds_up_upwards_by_a_random_step)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.move = SHOT_MOVE_RISING;
    shot.angle = 0.0f;
    shot.speed = 0.0f;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    RNG rng = g_Rng;
    const float expected = 0.0f - (Rng_F32(&rng) * 0.1f + 0.27f);
    Player_UpdateShots();
    CHECK(bullet->velocity.y == expected);
    CHECK_EQ_INT(g_Rng.generated, rng.generated);
    bullet->state = PLAYER_BULLET_HIT;
    Player_UpdateShots();
    CHECK_EQ_INT(g_Rng.generated, rng.generated);
    M_TearDown();
}

static PLAYER_BULLET *M_FireLaser(void)
{
    SHOT shot = M_Plain();
    shot.fire = SHOT_FIRE_LASER;
    shot.move = SHOT_MOVE_LASER;
    shot.kind = SHOT_KIND_LASER;
    shot.interval = 100;
    shot.option = 1;
    shot.offset_x = 2.0f;
    shot.angle = 0.0f;
    shot.speed = 1.0f;
    return M_FireOne(shot);
}

TEST(a_laser_holds_its_option_and_reaches_the_top)
{
    M_SetUp();
    PLAYER_BULLET *const bullet = M_FireLaser();
    Timer_Set(&g_Player.shots.fire_timer, 0);
    Player_UpdateShots();
    CHECK(bullet->pos.x == 168.0f + 2.0f + 1.0f);
    CHECK(bullet->pos.y == 384.0f / 2.0f);
    CHECK(bullet->pos.z == 0.44f);
    CHECK(bullet->vm.scale_y == 384.0f / 14.0f);
    CHECK(bullet->size.y == 384.0f);
    CHECK_EQ_INT(g_Player.shots.lasers[0].timer.current, 99);
    m_MsgRunning = true;
    Player_UpdateShots();
    CHECK_EQ_INT(g_Player.shots.lasers[0].timer.current, 20);
    M_TearDown();
}

TEST(a_laser_whose_time_ran_out_frees_its_bullet_and_entry)
{
    M_SetUp();
    PLAYER_BULLET *const bullet = M_FireLaser();
    PLAYER_LASER *const laser = &g_Player.shots.lasers[0];
    laser->bullet = nullptr;
    Timer_Set(&laser->timer, 0);
    Player_UpdateShots();
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_FREE);
    CHECK(laser->bullet == nullptr);
    M_TearDown();
}

TEST(a_laser_whose_time_ran_out_frees_its_entry_from_another_bullet)
{
    M_SetUp();
    M_FireLaser();
    PLAYER_LASER *const laser = &g_Player.shots.lasers[0];
    PLAYER_BULLET other = {};
    laser->bullet = &other;
    Timer_Set(&laser->timer, -1);
    Player_UpdateShots();
    CHECK(laser->bullet == nullptr);
    M_TearDown();
}

TEST(a_stopped_laser_is_interrupted_when_it_fades_or_loses_its_entry)
{
    M_SetUp();
    PLAYER_BULLET *const bullet = M_FireLaser();
    PLAYER_LASER *const laser = &g_Player.shots.lasers[0];
    Timer_Set(&g_Player.shots.fire_timer, 0);
    bullet->vm.stopped = true;
    Timer_Set(&laser->timer, 72);
    Player_UpdateShots();
    CHECK(bullet->vm.stopped);
    // The interrupt reaches the script, which has no label for it.
    Player_UpdateShots();
    CHECK(!bullet->vm.stopped);
    bullet->vm.stopped = true;
    Timer_Set(&laser->timer, 90);
    PLAYER_BULLET other = {};
    laser->bullet = &other;
    Player_UpdateShots();
    CHECK(!bullet->vm.stopped);
    M_TearDown();
}

TEST(a_dying_player_frees_a_laser_in_its_move)
{
    M_SetUp();
    PLAYER_BULLET *const bullet = M_FireLaser();
    g_Player.shots.lasers[0].bullet = nullptr;
    g_Player.state.value = PLAYER_DYING;
    Player_UpdateShots();
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_FREE);
    M_TearDown();
}

TEST(a_trail_laser_leaves_damage_boxes_along_its_trail)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.fire = SHOT_FIRE_TRAIL_LASER;
    shot.move = SHOT_MOVE_TRAIL_LASER;
    shot.kind = SHOT_KIND_TRAIL_LASER;
    shot.interval = 2;
    shot.phase = 2;
    shot.offset_x = -3.0f;
    shot.angle = 0.0f;
    shot.speed = 0.0f;
    shot.size_x = 20.0f;
    g_Player.options.state = PLAYER_OPTIONS_IN;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    Player_UpdateShots();
    CHECK(bullet->trail[0].x == -999.0f);
    CHECK(g_Player.damage_boxes[PLAYER_LASER_BOX_FIRST].size.x == 0.0f);
    CHECK(bullet->pos.x == 189.0f);
    CHECK(bullet->pos.y == 384.0f / 2.0f - 32.0f);
    CHECK(bullet->vm.scale_y == (384.0f + 64.0f) / 14.0f);
    CHECK(bullet->size.y == 448.0f);
    const VEC3 first = bullet->pos;
    Player_UpdateShots();
    Player_UpdateShots();
    CHECK(bullet->trail[1].x == first.x && bullet->trail[1].y == first.y);
    const PLAYER_DAMAGE_BOX *const box =
        &g_Player.damage_boxes[PLAYER_LASER_BOX_FIRST];
    CHECK(box->pos.x == first.x && box->size.x == 20.0f);
    CHECK_EQ_INT(box->damage, 1);
    CHECK(g_Player.damage_boxes[PLAYER_LASER_BOX_FIRST + 1].size.x == 0.0f);
    M_TearDown();
}

TEST(firing_runs_30_frames_on_whole_frames_but_not_in_a_marisa_b_bomb)
{
    M_SetUp();
    m_Shots[0].interval = 5;
    Player_StartFiring();
    Player_StartFiring();
    for (int32_t i = 0; i < 29; i++) {
        Player_UpdateFiring();
    }
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, 29);
    Player_UpdateFiring();
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, -1);
    CHECK_EQ_INT(M_Flying(), 6);
    M_TearDown();
    M_SetUp();
    g_GameSpeed = 0.5f;
    Player_StartFiring();
    Player_UpdateFiring();
    Player_UpdateFiring();
    Player_UpdateFiring();
    CHECK_EQ_INT(M_Flying(), 2);
    M_TearDown();
    M_SetUp();
    g_Player.bomb.running = true;
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 1;
    Player_StartFiring();
    Player_UpdateFiring();
    CHECK_EQ_INT(M_Flying(), 0);
    g_GameManager.run.shot_type = 0;
    Player_StartFiring();
    Timer_Set(&g_Player.shots.fire_timer, 0);
    Player_UpdateFiring();
    CHECK_EQ_INT(M_Flying(), 1);
    M_TearDown();
}

TEST(firing_stops_while_dying_or_respawning)
{
    M_SetUp();
    Player_StartFiring();
    g_Player.state.value = PLAYER_DYING;
    Player_UpdateFiring();
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, -1);
    g_Player.state.value = PLAYER_RESPAWNING;
    Player_StartFiring();
    Player_UpdateFiring();
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, -1);
    M_TearDown();
}

TEST(nothing_deals_damage_before_the_state_timer_ticked)
{
    M_SetUp();
    M_FireOne(M_Plain());
    Timer_Set(&g_Player.state.timer, 3);
    g_Player.state.timer.previous = 3;
    bool hit_by_bomb = true;
    CHECK_EQ_INT(
        Player_CalcDamage(M_Bullet(0)->pos, (VEC3) { 8, 8, 0 }, &hit_by_bomb),
        0);
    CHECK(hit_by_bomb);
    M_TearDown();
}

TEST(a_plain_shot_hits_once_and_stops_slowed_to_an_eighth)
{
    M_SetUp();
    PLAYER_BULLET *const bullet = M_FireOne(M_Plain());
    const VEC3 at = bullet->pos;
    const VEC3 v = bullet->velocity;
    CHECK_EQ_INT(M_Damage(at, (VEC3) { 8, 8, 0 }), 30);
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_HIT);
    CHECK_EQ_INT(bullet->vm.script_index, M_SCRIPT_HIT);
    CHECK(bullet->pos.z == 0.1f);
    CHECK(bullet->velocity.x == v.x / 8.0f && bullet->velocity.y == v.y / 8.0f);
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK_EQ_INT(m_Effects[0].type, M_EFFECT_SHOT_HIT);
    CHECK(m_Effects[0].pos.z == 0.495f);
    CHECK_EQ_INT(M_Damage(at, (VEC3) { 8, 8, 0 }), 0);
    M_TearDown();
}

TEST(a_shot_misses_a_box_it_does_not_touch)
{
    M_SetUp();
    PLAYER_BULLET *const bullet = M_FireOne(M_Plain());
    VEC3 at = bullet->pos;
    at.x += 6.0f + 4.0f + 0.01f;
    CHECK_EQ_INT(M_Damage(at, (VEC3) { 8, 8, 0 }), 0);
    at.x -= 0.02f;
    CHECK_EQ_INT(M_Damage(at, (VEC3) { 8, 8, 0 }), 30);
    M_TearDown();
}

TEST(a_bomb_cuts_shot_damage_to_a_third_at_least_1)
{
    M_SetUp();
    g_Player.bomb.running = true;
    SHOT shot = M_Plain();
    shot.damage = 2;
    shot.kind = SHOT_KIND_PIERCING;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 1);
    bullet->damage = 31;
    CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 10);
    M_TearDown();
}

TEST(a_piercing_shot_keeps_hitting_at_full_speed)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.kind = SHOT_KIND_PIERCING;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    const VEC3 v = bullet->velocity;
    CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 30);
    CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 30);
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_HIT);
    CHECK(bullet->velocity.x == v.x && bullet->velocity.y == v.y);
    CHECK_EQ_INT(m_EffectCount, 1);
    M_TearDown();
}

TEST(a_laser_hits_on_even_frames_and_keeps_flying)
{
    M_SetUp();
    for (int32_t kind = SHOT_KIND_LASER; kind <= SHOT_KIND_TRAIL_LASER;
         kind++) {
        SHOT shot = M_Plain();
        shot.kind = (SHOT_KIND)kind;
        PLAYER_BULLET *const bullet = M_FireOne(shot);
        CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 30);
        CHECK_EQ_INT(bullet->state, PLAYER_BULLET_FLYING);
        bullet->timer.current = 1;
        CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 0);
        bullet->timer.current = 2;
        CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 30);
        bullet->state = PLAYER_BULLET_FREE;
    }
    CHECK_EQ_INT(m_EffectCount, 0);
    M_TearDown();
}

TEST(a_lingering_shot_grows_and_turns_up_on_its_first_hit)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.kind = SHOT_KIND_PIERCING;
    shot.hit = SHOT_HIT_LINGER;
    shot.script = 0x446;
    shot.angle = -1.0f;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    RNG rng = g_Rng;
    const float angle = Rng_F32(&rng) * 1.5707964f - 2.3561945f;
    float x;
    float y;
    X87_SinCosScale(angle, 6.0f, &x, &y);
    const VEC3 enemy = { bullet->pos.x + 1.0f, bullet->pos.y, 0.0f };
    CHECK_EQ_INT(M_Damage(enemy, (VEC3) { 8, 8, 0 }), 30);
    CHECK(bullet->size.x == 64.0f && bullet->size.y == 64.0f);
    CHECK(bullet->velocity.x == x && bullet->velocity.y == y);
    CHECK(bullet->angle == -1.0f);
    CHECK_EQ_INT(m_EffectCount, 2);
    CHECK(m_Effects[0].pos.x == enemy.x);
    CHECK_EQ_INT(g_Rng.generated, rng.generated);
    M_TearDown();
}

TEST(a_lingering_shot_draws_its_angle_with_any_script)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.hit = SHOT_HIT_LINGER;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    RNG rng = g_Rng;
    Rng_F32(&rng);
    const VEC3 v = bullet->velocity;
    M_Damage(bullet->pos, (VEC3) { 8, 8, 0 });
    CHECK_EQ_INT(g_Rng.generated, rng.generated);
    CHECK(bullet->velocity.x == v.x / 8.0f);
    M_TearDown();
}

TEST(a_lingering_shot_hits_every_other_frame_a_third_as_hard)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.kind = SHOT_KIND_PIERCING;
    shot.hit = SHOT_HIT_LINGER;
    shot.damage = 2;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    bullet->state = PLAYER_BULLET_HIT;
    bullet->velocity = (VEC3) { 1.0f, -2.0f, 0.0f };
    bullet->timer.current = 3;
    CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 0);
    bullet->timer.current = 4;
    CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 1);
    CHECK(bullet->velocity.x == 0.88f && bullet->velocity.y == -2.0f * 0.88f);
    CHECK_EQ_INT(m_EffectCount, 0);
    bullet->timer.current = 6;
    bullet->damage = 30;
    CHECK_EQ_INT(M_Damage(bullet->pos, (VEC3) { 8, 8, 0 }), 10);
    CHECK_EQ_INT(m_EffectCount, 1);
    M_TearDown();
}

TEST(a_spark_shot_bursts_an_effect_every_8_hits)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.kind = SHOT_KIND_LASER;
    shot.hit = SHOT_HIT_SPARK;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    const VEC3 enemy = { bullet->pos.x + 1.0f, bullet->pos.y + 2.0f, 0.0f };
    g_Player.shots.hit_counter = 1;
    for (int32_t i = 0; i < 6; i++) {
        M_Damage(enemy, (VEC3) { 8, 8, 0 });
    }
    CHECK_EQ_INT(m_EffectCount, 0);
    M_Damage(enemy, (VEC3) { 8, 8, 0 });
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK(m_Effects[0].pos.x == bullet->pos.x);
    CHECK(m_Effects[0].pos.y == enemy.y);
    M_TearDown();
}

TEST(damage_boxes_deal_and_count_their_damage_and_spark_every_4_hits)
{
    M_SetUp();
    const VEC3 enemy = { 50.0f, 60.0f, 0.0f };
    g_Player.damage_boxes[3] =
        (PLAYER_DAMAGE_BOX) { enemy, { 16, 16, 0 }, 7, 0 };
    g_Player.damage_boxes[PLAYER_LASER_BOX_FIRST] =
        (PLAYER_DAMAGE_BOX) { enemy, { 16, 16, 0 }, 1, 0 };
    g_Player.damage_boxes[4] =
        (PLAYER_DAMAGE_BOX) { enemy, { 0, 16, 0 }, 9, 0 };
    g_Player.shots.hit_counter = 3;
    bool hit_by_bomb = true;
    CHECK_EQ_INT(Player_CalcDamage(enemy, (VEC3) { 8, 8, 0 }, &hit_by_bomb), 8);
    CHECK(!hit_by_bomb);
    CHECK_EQ_INT(g_Player.damage_boxes[3].damage_dealt, 7);
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_BOMB_SPARK);
    g_Player.shots.hit_counter = 2;
    g_Player.bomb.running = true;
    CHECK_EQ_INT(Player_CalcDamage(enemy, (VEC3) { 8, 8, 0 }, &hit_by_bomb), 8);
    CHECK(hit_by_bomb);
    CHECK_EQ_INT(m_EffectCount, 2);
    CHECK_EQ_INT(m_Effects[1].type, M_EFFECT_SHOT_HIT);
    M_TearDown();
}

TEST(flying_shots_draw_turned_in_the_play_area_with_their_trail)
{
    M_SetUp();
    SHOT shot = M_Plain();
    shot.draw = SHOT_DRAW_TRAIL;
    PLAYER_BULLET *const bullet = M_FireOne(shot);
    bullet->vm.auto_rotate = 1;
    bullet->vm.color = COLOR_ARGB(0x80, 0xff, 0xff, 0xff);
    bullet->trail_length = 4;
    for (int32_t i = 0; i < PLAYER_TRAIL_COUNT; i++) {
        bullet->trail[i] = (VEC3) { 10.0f * (float)i, 20.0f, 0.3f };
    }
    bullet->trail[3].x = -999.0f;
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view =
        Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
    Gfx_BeginTick((GFX_COLOR) {});
    Player_DrawShots(PLAYER_BULLET_HIT, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    Gfx_BeginTick((GFX_COLOR) {});
    Player_DrawShots(PLAYER_BULLET_FLYING, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 4);
    CHECK_EQ_INT(
        GfxRecorder_Quad(3)->vertices[0].color.alpha, 0x80 - 0x80 * 2 / 4);
    CHECK(
        bullet->vm.rotation.z
        == Angle_AddNormalize(bullet->angle, MATH_HALF_PI));
    CHECK(bullet->vm.pos.x == 20.0f + 32.0f && bullet->vm.pos.y == 36.0f);
    CHECK(bullet->vm.color == COLOR_ARGB(0x80, 0xff, 0xff, 0xff));
    M_TearDown();
}
