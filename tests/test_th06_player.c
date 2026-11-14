#include "th06/player.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/anm.h"
#include "th06/bomb.h"
#include "th06/bullet.h"
#include "th06/effect.h"
#include "th06/enemy.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/scene.h"
#include "th06/sound.h"

#include "core/base/memory.h"
#include "core/gfx/private.h"
#include "core/math/x87.h"

#include "stg/chain.h"
#include "stg/rng.h"

#include <SDL3/SDL.h>
#include <math.h>

#define M_HALF_PI 1.5707964f
#define M_START_X 192.0f
#define M_START_Y 384.0f
#define M_MAX_CALLS 64
#define M_SCRIPT_SHOT 0x440
#define M_SCRIPT_HIT 0x460
#define M_SCRIPT_RISING 0x441
#define M_SCRIPT_LASER 0x445

// The test links these instead of the modules they stand for. Each
// records its calls.

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

ENEMY_MANAGER g_Enemies;

static char *m_Directory;
static bool m_MsgRunning;
static int32_t m_ClearCalls;
static int32_t m_BombCalls;
static bool m_BombInvulnerable;
static PLAYER_BOMB_HIT m_BombSeenHit;
static PLAYER_CLEAR_AREA m_BombSeenArea;
static M_EFFECT_CALL m_Effects[M_MAX_CALLS];
static int32_t m_EffectCount;
static M_ITEM_CALL m_Items[M_MAX_CALLS];
static int32_t m_ItemCount;
static EFFECT m_SpareEffect;

bool Gui_IsMsgRunning(void)
{
    return m_MsgRunning;
}

void Bullet_ClearAll(void)
{
    m_ClearCalls++;
}

void Bomb_Update(void)
{
    m_BombCalls++;
    m_BombSeenHit = g_Player.bomb.hits[0];
    m_BombSeenArea = g_Player.bomb.clear_areas[0];
    if (m_BombInvulnerable) {
        g_Player.state.value = PLAYER_INVULNERABLE;
    }
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

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
    if (m_ItemCount < M_MAX_CALLS) {
        m_Items[m_ItemCount] = (M_ITEM_CALL) { pos, type, state };
    }
    m_ItemCount++;
}

static void M_AddScript(
    ANM_BUILDER *const builder, const int32_t id, const int16_t end)
{
    const int32_t script = AnmBuilder_BeginScript(builder, id);
    const uint32_t sprite = 0;
    AnmBuilder_Instr(builder, script, 0, 1, &sprite, 1);
    AnmBuilder_Instr(builder, script, end, 0, nullptr, 0);
}

// Adds a laser script that waits at a stop and ends at interrupt 1.
static void M_AddLaserScript(ANM_BUILDER *const builder, const int32_t id)
{
    const int32_t script = AnmBuilder_BeginScript(builder, id);
    const uint32_t sprite = 0;
    const uint32_t label = 1;
    AnmBuilder_Instr(builder, script, 0, 1, &sprite, 1);
    AnmBuilder_Instr(builder, script, 0, 21, nullptr, 0);
    AnmBuilder_Instr(builder, script, 0, 22, &label, 1);
    AnmBuilder_Instr(builder, script, 0, 0, nullptr, 0);
}

// Writes a player ANM file numbered from 0x400: a 16 by 16 sprite, the
// turning scripts, the shot scripts and their hit scripts, which end at
// frame 3, and the option scripts. The laser scripts wait for their
// interrupt.
static void M_WriteAnm(const char *const name)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 64, 64);
    AnmBuilder_AddSprite(&builder, 0, 0.0f, 0.0f, 16.0f, 16.0f);
    for (int32_t i = 0; i < 5; i++) {
        M_AddScript(&builder, i, 1000);
    }
    for (int32_t i = 0x40; i < 0x45; i++) {
        M_AddScript(&builder, i, 1000);
    }
    for (int32_t i = 0x45; i < 0x48; i++) {
        M_AddLaserScript(&builder, i);
    }
    for (int32_t i = 0x60; i < 0x68; i++) {
        M_AddScript(&builder, i, 3);
    }
    M_AddScript(&builder, 0x80, 1000);
    M_AddScript(&builder, 0x81, 1000);
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, name, bytes, size);
}

static void M_Setup(
    const char *const case_name, const uint8_t character,
    const uint8_t shot_type)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    M_WriteAnm("data/player00.anm");
    M_WriteAnm("data/player01.anm");
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.player_area = (PLAY_RECT) { 8.0f, 16.0f, 368.0f, 416.0f };
    g_GameManager.run.character = character;
    g_GameManager.run.shot_type = shot_type;
    g_GameManager.rank.value = 20;
    g_GameManager.rank.max = 99;
    Scene_Request(SCENE_GAMEPLAY);
    g_Enemies = (ENEMY_MANAGER) {};
    g_Buttons = (BUTTONS) {};
    m_MsgRunning = false;
    m_ClearCalls = 0;
    m_BombCalls = 0;
    m_BombInvulnerable = false;
    m_EffectCount = 0;
    m_ItemCount = 0;
    CHECK(Result_Log(Player_Register(), LOG_LEVEL_ERROR));
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
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

// Sets the player up past its first update, alive at the start position.
static void M_SetupAlive(
    const char *const case_name, const uint8_t character,
    const uint8_t shot_type)
{
    M_Setup(case_name, character, shot_type);
    M_Frame();
    g_Player.state.value = PLAYER_ALIVE;
    m_ClearCalls = 0;
}

// Runs one frame with the shot button held, then releases it.
static void M_Shoot(void)
{
    g_Buttons.held.shoot = true;
    M_Frame();
    g_Buttons.held.shoot = false;
}

static int32_t M_CountBullets(void)
{
    int32_t count = 0;
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        if (g_Player.shots.bullets[i].state != PLAYER_BULLET_FREE) {
            count++;
        }
    }
    return count;
}

// Puts a flying straight shot of damage in bullet slot index at pos.
static PLAYER_BULLET *M_PlaceBullet(
    const int32_t index, const VEC3 pos, const int16_t damage)
{
    PLAYER_BULLET *const bullet = &g_Player.shots.bullets[index];
    Anm_StartScript(&bullet->vm, M_SCRIPT_SHOT);
    bullet->pos = pos;
    bullet->size = (VEC3) { 12.0f, 12.0f, 1.0f };
    bullet->velocity = (VEC3) { 0.0f, -8.0f, 0.0f };
    bullet->damage = damage;
    bullet->state = PLAYER_BULLET_FLYING;
    bullet->type = PLAYER_BULLET_STRAIGHT;
    Timer_Set(&bullet->timer, 0);
    return bullet;
}

static VEC3 M_At(const float x, const float y)
{
    return (VEC3) { x, y, 0.0f };
}

static const VEC3 m_Small = { 2.0f, 2.0f, 0.0f };

// Registration and respawn

TEST(register_places_the_player_respawning_at_the_bottom)
{
    M_Setup("register", 0, 0);
    CHECK(g_Player.pos.x == M_START_X && g_Player.pos.y == M_START_Y);
    CHECK(g_Player.pos.z == 0.49f);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_RESPAWNING);
    CHECK_EQ_INT(g_Player.state.timer.current, 120);
    CHECK_EQ_INT(g_Player.state.bomb_window, 8);
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, -1);
    CHECK(g_Player.box.hitbox_size.x == 1.25f);
    CHECK(g_Player.box.graze_size.x == 12.0f);
    CHECK(g_Player.move.speed_scale_x == 1.0f);
    CHECK_EQ_INT(g_Player.vm.script_index, 0x400);
    CHECK_EQ_INT(g_Player.option_vms[0].script_index, 0x480);
    CHECK_EQ_INT(g_Player.option_vms[1].script_index, 0x481);
    M_Teardown();
}

TEST(register_takes_the_speeds_of_the_shot_type)
{
    M_Setup("speeds", 1, 0);
    CHECK(g_Player.move.speed == 5.0f);
    CHECK(g_Player.move.speed_focused == 2.5f);
    CHECK(g_Player.move.speed_diagonal == 5.0f / sqrtf(2.0f));
    CHECK(g_Player.move.speed_diagonal_focused == 2.5f / sqrtf(2.0f));
    M_Teardown();
}

TEST(remove_releases_the_sprites_outside_next_stage)
{
    M_Setup("remove", 0, 0);
    CHECK(Anm_GetScript(0x400) != nullptr);
    Player_Remove();
    CHECK(Anm_GetScript(0x400) == nullptr);
    M_Teardown();
}

TEST(next_stage_keeps_the_sprites)
{
    M_Setup("next_stage", 0, 0);
    Scene_Request(SCENE_NEXT_STAGE);
    Player_Remove();
    CHECK(Anm_GetScript(0x400) != nullptr);
    Anm_Release(5);
    CHECK(Result_Log(Player_Register(), LOG_LEVEL_ERROR));
    CHECK(Anm_GetScript(0x400) == nullptr);
    M_Teardown();
}

TEST(the_first_update_makes_the_player_invulnerable)
{
    M_Setup("first_update", 0, 0);
    M_Frame();
    CHECK_EQ_INT(g_Player.state.value, PLAYER_INVULNERABLE);
    CHECK_EQ_INT(g_Player.state.timer.current, 239);
    CHECK_EQ_INT(g_Player.state.bomb_window, 6);
    CHECK_EQ_INT(g_Player.state.respawn_clear_frames, 89);
    CHECK_EQ_INT(m_ClearCalls, 1);
    M_Teardown();
}

TEST(a_respawn_clears_the_bullets_for_90_frames)
{
    M_Setup("respawn_clear", 0, 0);
    M_Frames(100);
    CHECK_EQ_INT(m_ClearCalls, 90);
    M_Teardown();
}

TEST(invulnerability_ends_after_240_frames)
{
    M_Setup("invulnerable", 0, 0);
    M_Frames(239);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_INVULNERABLE);
    M_Frame();
    CHECK_EQ_INT(g_Player.state.value, PLAYER_ALIVE);
    M_Teardown();
}

TEST(a_frozen_game_stops_the_player)
{
    M_SetupAlive("frozen", 0, 0);
    g_GameManager.mode.frozen = true;
    g_Player.bomb.hits[0].size.x = 5.0f;
    const int32_t frames = g_Player.state.timer.current;
    g_Buttons.held.right = true;
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X);
    CHECK(g_Player.bomb.hits[0].size.x == 5.0f);
    CHECK_EQ_INT(g_Player.state.timer.current, frames);
    M_Teardown();
}

// Movement

TEST(the_player_moves_by_its_speed)
{
    M_SetupAlive("move", 0, 0);
    g_Buttons.held.right = true;
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X + 4.0f);
    g_Buttons.held.focus = true;
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X + 6.0f);
    CHECK(g_Player.move.focused);
    M_Teardown();
}

TEST(a_diagonal_move_divides_the_speed_by_the_root_of_two)
{
    M_SetupAlive("diagonal", 0, 0);
    g_Buttons.held.up = true;
    g_Buttons.held.right = true;
    M_Frame();
    const float step = 4.0f / sqrtf(2.0f);
    CHECK(g_Player.pos.x == M_START_X + step);
    CHECK(g_Player.pos.y == M_START_Y - step);
    M_Teardown();
}

TEST(right_wins_over_left_and_up_over_down)
{
    M_SetupAlive("opposite", 0, 0);
    g_Buttons.held.left = true;
    g_Buttons.held.right = true;
    g_Buttons.held.up = true;
    g_Buttons.held.down = true;
    M_Frame();
    const float step = 4.0f / sqrtf(2.0f);
    CHECK(g_Player.pos.x == M_START_X + step);
    CHECK(g_Player.pos.y == M_START_Y - step);
    M_Teardown();
}

TEST(the_player_stays_in_the_player_area)
{
    M_SetupAlive("clamp", 0, 0);
    g_Player.pos = M_At(375.0f, 431.0f);
    g_Buttons.held.right = true;
    g_Buttons.held.down = true;
    M_Frame();
    CHECK(g_Player.pos.x == 376.0f && g_Player.pos.y == 432.0f);
    g_Buttons = (BUTTONS) {};
    g_Player.pos = M_At(9.0f, 17.0f);
    g_Buttons.held.left = true;
    g_Buttons.held.up = true;
    M_Frame();
    CHECK(g_Player.pos.x == 8.0f && g_Player.pos.y == 16.0f);
    M_Teardown();
}

TEST(the_boxes_follow_the_player)
{
    M_SetupAlive("boxes", 0, 0);
    g_Buttons.held.right = true;
    M_Frame();
    CHECK(g_Player.box.hitbox_min.x == 196.0f - 1.25f);
    CHECK(g_Player.box.hitbox_max.y == M_START_Y + 1.25f);
    CHECK(g_Player.box.graze_min.y == M_START_Y - 12.0f);
    CHECK(g_Player.box.graze_max.x == 196.0f + 12.0f);
    M_Teardown();
}

TEST(a_turn_starts_the_turning_scripts)
{
    M_SetupAlive("turn", 0, 0);
    g_Buttons.held.left = true;
    M_Frame();
    CHECK_EQ_INT(g_Player.vm.script_index, 0x401);
    g_Buttons.held.left = false;
    M_Frame();
    CHECK_EQ_INT(g_Player.vm.script_index, 0x402);
    g_Buttons.held.right = true;
    M_Frame();
    CHECK_EQ_INT(g_Player.vm.script_index, 0x403);
    g_Buttons.held.right = false;
    M_Frame();
    CHECK_EQ_INT(g_Player.vm.script_index, 0x404);
    M_Teardown();
}

TEST(the_player_does_not_move_while_dying)
{
    M_SetupAlive("dying_move", 0, 0);
    CHECK(Player_CheckHit(g_Player.pos, m_Small) == PLAYER_CONTACT_BOX);
    g_Buttons.held.right = true;
    M_Frame();
    CHECK(g_Player.pos.x == M_START_X);
    M_Teardown();
}

// Options

TEST(options_appear_at_power_8)
{
    M_SetupAlive("options", 0, 0);
    g_GameManager.player.power = 7;
    M_Frame();
    CHECK_EQ_INT(g_Player.options.state, PLAYER_OPTIONS_NONE);
    g_GameManager.player.power = 8;
    M_Frame();
    CHECK_EQ_INT(g_Player.options.state, PLAYER_OPTIONS_OUT);
    CHECK(g_Player.options.pos[0].x == M_START_X - 24.0f);
    CHECK(g_Player.options.pos[1].x == M_START_X + 24.0f);
    CHECK(g_Player.options.pos[0].y == M_START_Y);
    M_Teardown();
}

TEST(options_slide_in_over_8_frames_while_focused)
{
    M_SetupAlive("slide_in", 0, 0);
    g_GameManager.player.power = 8;
    M_Frame();
    g_Buttons.held.focus = true;
    M_Frame();
    CHECK_EQ_INT(g_Player.options.state, PLAYER_OPTIONS_SLIDING_IN);
    CHECK(g_Player.options.pos[0].x == M_START_X - 23.75f);
    CHECK(g_Player.options.pos[0].y == M_START_Y - 4.0f);
    M_Frames(7);
    CHECK_EQ_INT(g_Player.options.state, PLAYER_OPTIONS_IN);
    CHECK(g_Player.options.pos[1].x == M_START_X + 8.0f);
    CHECK(g_Player.options.pos[1].y == M_START_Y - 32.0f);
    M_Frame();
    CHECK(g_Player.options.pos[1].x == M_START_X + 8.0f);
    M_Teardown();
}

TEST(a_slide_that_turns_back_starts_where_the_other_was)
{
    M_SetupAlive("slide_back", 0, 0);
    g_GameManager.player.power = 8;
    M_Frame();
    g_Buttons.held.focus = true;
    M_Frames(3);
    g_Buttons.held.focus = false;
    M_Frame();
    CHECK_EQ_INT(g_Player.options.state, PLAYER_OPTIONS_SLIDING_OUT);
    CHECK_EQ_INT(g_Player.options.timer.current, 5);
    CHECK(g_Player.options.pos[0].x == M_START_X - 14.25f);
    CHECK(g_Player.options.pos[0].y == M_START_Y - 12.0f);
    M_Teardown();
}

// Shots

TEST(the_shot_button_fires_a_burst)
{
    M_SetupAlive("fire", 0, 0);
    M_Shoot();
    CHECK_EQ_INT(M_CountBullets(), 1);
    const PLAYER_BULLET *const bullet = &g_Player.shots.bullets[0];
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_FLYING);
    CHECK_EQ_INT(bullet->type, PLAYER_BULLET_STRAIGHT);
    CHECK_EQ_INT(bullet->damage, 48);
    CHECK_EQ_INT(bullet->vm.script_index, M_SCRIPT_SHOT);
    CHECK(bullet->pos.x == M_START_X && bullet->pos.y == M_START_Y);
    CHECK(bullet->pos.z == 0.495f);
    CHECK(bullet->size.x == 12.0f && bullet->size.z == 1.0f);
    CHECK(bullet->velocity.x == X87_Cos(-M_HALF_PI) * 12.0f);
    CHECK(bullet->velocity.y == X87_Sin(-M_HALF_PI) * 12.0f);
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, 1);
    M_Teardown();
}

TEST(a_shot_queues_the_sound_of_its_table_entry_when_it_fires)
{
    M_SetupAlive("shot_sound", 0, 0);
    Sound_Reset();
    M_Shoot();
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_PLAYER_SHOT);
    Sound_Reset();
    M_Frame();
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    M_Teardown();
}

TEST(a_burst_lasts_30_frames)
{
    M_SetupAlive("burst", 0, 0);
    g_Buttons.held.shoot = true;
    M_Frames(30);
    CHECK_EQ_INT(M_CountBullets(), 6);
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, -1);
    M_Frame();
    CHECK_EQ_INT(M_CountBullets(), 7);
    M_Teardown();
}

TEST(the_power_level_starts_at_its_threshold)
{
    M_SetupAlive("power_7", 0, 0);
    g_GameManager.player.power = 7;
    M_Shoot();
    CHECK_EQ_INT(M_CountBullets(), 1);
    M_Teardown();

    M_SetupAlive("power_8", 0, 0);
    g_GameManager.player.power = 8;
    M_Shoot();
    CHECK_EQ_INT(M_CountBullets(), 3);
    const PLAYER_BULLET *const option = &g_Player.shots.bullets[1];
    CHECK(option->pos.x == M_START_X - 24.0f);
    CHECK_EQ_INT(option->type, PLAYER_BULLET_HOMING);
    CHECK(option->angle == -2.0943952f);
    M_Teardown();
}

TEST(no_shots_start_during_a_dialogue)
{
    M_SetupAlive("dialogue", 0, 0);
    m_MsgRunning = true;
    M_Shoot();
    CHECK_EQ_INT(M_CountBullets(), 0);
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, -1);
    M_Teardown();
}

TEST(a_shot_that_does_not_fire_leaves_its_bullet_free)
{
    M_SetupAlive("skip", 1, 0);
    g_GameManager.player.power = 16;
    g_Buttons.held.shoot = true;
    M_Frames(16);
    CHECK(g_Player.shots.bullets[5].angle == -1.5707964f);
    CHECK(g_Player.shots.bullets[6].angle == -1.4835299f);
    CHECK(g_Player.shots.bullets[7].angle == -1.6580628f);
    CHECK_EQ_INT(g_Player.shots.bullets[8].state, PLAYER_BULLET_FREE);
    M_Teardown();
}

TEST(shots_past_the_last_free_bullet_do_not_fire)
{
    M_SetupAlive("full", 0, 0);
    g_GameManager.player.power = 8;
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT - 1; i++) {
        M_PlaceBullet(i, M_At(100.0f, 200.0f), 1)->velocity = (VEC3) {};
    }
    M_Shoot();
    CHECK_EQ_INT(M_CountBullets(), PLAYER_BULLET_COUNT);
    const PLAYER_BULLET *const last =
        &g_Player.shots.bullets[PLAYER_BULLET_COUNT - 1];
    CHECK(last->angle == -1.5707964f);
    M_Shoot();
    CHECK_EQ_INT(M_CountBullets(), PLAYER_BULLET_COUNT);
    M_Teardown();
}

TEST(no_shots_fire_during_the_bomb_of_marisa_b)
{
    M_SetupAlive("master_spark", 1, 1);
    g_Player.bomb.running = true;
    M_Shoot();
    CHECK_EQ_INT(M_CountBullets(), 0);
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, 1);
    M_Teardown();

    M_SetupAlive("other_bomb", 1, 0);
    g_Player.bomb.running = true;
    M_Shoot();
    CHECK_EQ_INT(M_CountBullets(), 1);
    M_Teardown();
}

TEST(a_death_stops_the_burst)
{
    M_SetupAlive("death_burst", 0, 0);
    M_Shoot();
    CHECK(Player_CheckHit(g_Player.pos, m_Small) == PLAYER_CONTACT_BOX);
    M_Frame();
    CHECK_EQ_INT(g_Player.shots.fire_timer.current, -1);
    M_Teardown();
}

TEST(a_shot_outside_the_play_area_is_freed)
{
    M_SetupAlive("outside", 0, 0);
    M_PlaceBullet(0, M_At(100.0f, 7.0f), 1);
    M_Frame();
    CHECK_EQ_INT(g_Player.shots.bullets[0].state, PLAYER_BULLET_FLYING);
    M_Frame();
    CHECK_EQ_INT(g_Player.shots.bullets[0].state, PLAYER_BULLET_FREE);
    M_Teardown();
}

TEST(a_homing_shot_steers_to_the_target)
{
    M_SetupAlive("homing", 0, 0);
    g_GameManager.player.power = 8;
    M_Shoot();
    PLAYER_BULLET *const bullet = &g_Player.shots.bullets[1];
    const VEC3 start = bullet->pos;
    const VEC3 velocity = bullet->velocity;
    g_Player.homing_target = M_At(100.0f, 100.0f);
    M_Frame();

    float x = 100.0f - start.x;
    float y = 100.0f - start.y;
    float len = sqrtf(x * x + y * y) / (10.0f / 4.0f);
    x = x / len + velocity.x;
    y = y / len + velocity.y;
    len = sqrtf(x * x + y * y);
    const float speed = len > 10.0f ? 10.0f : len;
    CHECK(bullet->speed == speed);
    CHECK(bullet->velocity.x == x * speed / len);
    CHECK(bullet->velocity.y == y * speed / len);
    CHECK(bullet->pos.x == start.x + bullet->velocity.x);
    CHECK(g_Player.homing_target.x == -999.0f);
    M_Teardown();
}

TEST(a_homing_shot_steers_only_in_its_first_40_frames)
{
    M_SetupAlive("homing_40", 0, 0);
    g_GameManager.player.power = 8;
    M_Shoot();
    PLAYER_BULLET *const bullet = &g_Player.shots.bullets[1];
    const VEC3 velocity = bullet->velocity;
    bullet->timer.current = 40;
    g_Player.homing_target = M_At(100.0f, 100.0f);
    M_Frame();
    CHECK(bullet->velocity.x == velocity.x && bullet->velocity.y == velocity.y);
    M_Teardown();
}

TEST(a_homing_shot_without_a_target_speeds_up)
{
    M_SetupAlive("homing_speed", 0, 0);
    g_GameManager.player.power = 8;
    M_Shoot();
    PLAYER_BULLET *const bullet = &g_Player.shots.bullets[1];
    bullet->speed = 5.0f;
    const VEC3 velocity = bullet->velocity;
    M_Frame();
    const float speed = 5.0f + 0.33333334f;
    const float len = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);
    CHECK(bullet->speed == speed);
    CHECK(bullet->velocity.x == velocity.x * speed / len);
    CHECK(bullet->velocity.y == velocity.y * speed / len);
    M_Teardown();
}

TEST(a_rising_shot_accelerates_upwards)
{
    M_SetupAlive("rising", 1, 0);
    g_GameManager.player.power = 8;
    M_Shoot();
    const PLAYER_BULLET *const bullet = &g_Player.shots.bullets[1];
    CHECK_EQ_INT(bullet->type, PLAYER_BULLET_RISING);
    const float vy = X87_Sin(-M_HALF_PI) * 3.0f;
    M_Frame();
    CHECK(bullet->velocity.y == vy - 0.3f);
    CHECK(bullet->pos.y == M_START_Y + (vy - 0.3f));
    M_Teardown();
}

TEST(a_laser_stays_on_its_option_and_reaches_the_top)
{
    M_SetupAlive("laser", 1, 1);
    g_GameManager.player.power = 8;
    M_Shoot();
    const PLAYER_BULLET *const laser = &g_Player.shots.bullets[1];
    CHECK_EQ_INT(laser->type, PLAYER_BULLET_LASER);
    CHECK_EQ_INT(laser->option, 1);
    CHECK_EQ_INT(g_Player.shots.laser_timers[0].current, 120);
    g_Buttons.held.left = true;
    M_Frame();
    const VEC3 option = g_Player.options.pos[0];
    const float half = option.y / 2.0f;
    CHECK(laser->pos.x == option.x + laser->velocity.x);
    CHECK(laser->pos.y == half + laser->velocity.y);
    CHECK(laser->size.y == half * 2.0f);
    CHECK(laser->vm.scale_y == half * 2.0f / 14.0f);
    CHECK_EQ_INT(g_Player.shots.laser_timers[0].current, 119);
    M_Teardown();
}

TEST(a_laser_gets_its_interrupt_at_70_frames_left)
{
    M_SetupAlive("laser_end", 1, 1);
    g_GameManager.player.power = 8;
    M_Shoot();
    M_Frames(49);
    CHECK_EQ_INT(g_Player.shots.bullets[1].state, PLAYER_BULLET_FLYING);
    M_Frame();
    CHECK_EQ_INT(g_Player.shots.laser_timers[0].current, 70);
    CHECK_EQ_INT(g_Player.shots.bullets[1].state, PLAYER_BULLET_FREE);
    M_Teardown();
}

// Damage

TEST(a_shot_hits_once_and_slows_down)
{
    M_SetupAlive("hit", 0, 0);
    PLAYER_BULLET *const bullet = M_PlaceBullet(0, M_At(100.0f, 200.0f), 48);
    bool by_bomb = true;
    CHECK_EQ_INT(
        Player_CalcDamage(M_At(100.0f, 205.0f), m_Small, &by_bomb), 48);
    CHECK(!by_bomb);
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_HIT);
    CHECK_EQ_INT(bullet->vm.script_index, M_SCRIPT_HIT);
    CHECK(bullet->velocity.y == -1.0f);
    CHECK(bullet->pos.z == 0.1f);
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_PARTICLE + 1);
    CHECK(m_Effects[0].pos.x == 100.0f && m_Effects[0].pos.y == 200.0f);
    CHECK_EQ_INT(Player_CalcDamage(M_At(100.0f, 205.0f), m_Small, &by_bomb), 0);
    M_Teardown();
}

TEST(a_shot_misses_a_box_past_its_edge)
{
    M_SetupAlive("miss", 0, 0);
    M_PlaceBullet(0, M_At(100.0f, 200.0f), 48);
    bool by_bomb;
    CHECK_EQ_INT(Player_CalcDamage(M_At(108.0f, 200.0f), m_Small, &by_bomb), 0);
    CHECK_EQ_INT(
        Player_CalcDamage(M_At(107.0f, 200.0f), m_Small, &by_bomb), 48);
    M_Teardown();
}

TEST(a_hit_shot_is_freed_when_its_hit_script_ends)
{
    M_SetupAlive("hit_end", 0, 0);
    PLAYER_BULLET *const bullet = M_PlaceBullet(0, M_At(100.0f, 200.0f), 48);
    bool by_bomb;
    Player_CalcDamage(M_At(100.0f, 200.0f), m_Small, &by_bomb);
    M_Frames(2);
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_HIT);
    M_Frame();
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_FREE);
    M_Teardown();
}

TEST(shots_deal_a_third_during_a_bomb)
{
    M_SetupAlive("bomb_third", 0, 0);
    g_Player.bomb.running = true;
    M_PlaceBullet(0, M_At(100.0f, 200.0f), 48);
    M_PlaceBullet(1, M_At(100.0f, 200.0f), 2);
    bool by_bomb;
    CHECK_EQ_INT(
        Player_CalcDamage(M_At(100.0f, 200.0f), m_Small, &by_bomb), 16 + 1);
    M_Teardown();
}

TEST(a_rising_shot_keeps_hitting_weaker_and_larger)
{
    M_SetupAlive("rising_hit", 0, 0);
    PLAYER_BULLET *const bullet = M_PlaceBullet(0, M_At(100.0f, 200.0f), 18);
    bullet->type = PLAYER_BULLET_RISING;
    Anm_StartScript(&bullet->vm, M_SCRIPT_RISING);
    bool by_bomb;
    CHECK_EQ_INT(
        Player_CalcDamage(M_At(100.0f, 200.0f), m_Small, &by_bomb), 18);
    CHECK_EQ_INT(bullet->damage, 4);
    CHECK(bullet->size.x == 32.0f && bullet->size.y == 32.0f);
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_HIT);
    CHECK_EQ_INT(m_EffectCount, 2);
    CHECK_EQ_INT(Player_CalcDamage(M_At(100.0f, 200.0f), m_Small, &by_bomb), 4);
    CHECK_EQ_INT(Player_CalcDamage(M_At(100.0f, 200.0f), m_Small, &by_bomb), 1);
    CHECK_EQ_INT(bullet->damage, 1);
    M_Teardown();
}

TEST(a_rising_shot_bursts_every_6_frames_of_its_life)
{
    M_SetupAlive("rising_burst", 0, 0);
    PLAYER_BULLET *const bullet = M_PlaceBullet(0, M_At(100.0f, 200.0f), 18);
    bullet->type = PLAYER_BULLET_RISING;
    bullet->state = PLAYER_BULLET_HIT;
    bullet->timer.current = 5;
    bool by_bomb;
    Player_CalcDamage(M_At(100.0f, 200.0f), m_Small, &by_bomb);
    CHECK_EQ_INT(m_EffectCount, 0);
    bullet->timer.current = 6;
    Player_CalcDamage(M_At(100.0f, 200.0f), m_Small, &by_bomb);
    CHECK_EQ_INT(m_EffectCount, 1);
    M_Teardown();
}

TEST(a_laser_keeps_flying_and_bursts_every_8_hits)
{
    M_SetupAlive("laser_hit", 0, 0);
    PLAYER_BULLET *const bullet = M_PlaceBullet(0, M_At(100.0f, 200.0f), 3);
    bullet->type = PLAYER_BULLET_LASER;
    bool by_bomb;
    for (int32_t i = 0; i < 7; i++) {
        CHECK_EQ_INT(
            Player_CalcDamage(M_At(102.0f, 205.0f), m_Small, &by_bomb), 3);
    }
    CHECK_EQ_INT(m_EffectCount, 0);
    Player_CalcDamage(M_At(102.0f, 205.0f), m_Small, &by_bomb);
    CHECK_EQ_INT(bullet->state, PLAYER_BULLET_FLYING);
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK(m_Effects[0].pos.x == 100.0f && m_Effects[0].pos.y == 205.0f);
    M_Teardown();
}

TEST(a_bomb_box_deals_its_damage_and_counts_it)
{
    M_SetupAlive("bomb_hit", 0, 0);
    PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[3];
    hit->pos = M_At(100.0f, 100.0f);
    hit->size = (VEC3) { 50.0f, 50.0f, 0.0f };
    hit->damage = 8;
    bool by_bomb = true;
    CHECK_EQ_INT(Player_CalcDamage(M_At(125.0f, 125.0f), m_Small, &by_bomb), 8);
    CHECK(!by_bomb);
    g_Player.bomb.running = true;
    CHECK_EQ_INT(Player_CalcDamage(M_At(125.0f, 125.0f), m_Small, &by_bomb), 8);
    CHECK(by_bomb);
    CHECK_EQ_INT(hit->damage_dealt, 16);
    CHECK_EQ_INT(Player_CalcDamage(M_At(127.0f, 125.0f), m_Small, &by_bomb), 0);
    hit->size.x = 0.0f;
    CHECK_EQ_INT(Player_CalcDamage(M_At(100.0f, 100.0f), m_Small, &by_bomb), 0);
    CHECK(!by_bomb);
    M_Teardown();
}

TEST(a_bomb_box_sparks_every_4_hits)
{
    M_SetupAlive("bomb_spark", 0, 0);
    PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[0];
    hit->pos = M_At(100.0f, 100.0f);
    hit->size = (VEC3) { 50.0f, 50.0f, 0.0f };
    hit->damage = 8;
    bool by_bomb;
    for (int32_t i = 0; i < 3; i++) {
        Player_CalcDamage(M_At(110.0f, 100.0f), m_Small, &by_bomb);
    }
    CHECK_EQ_INT(m_EffectCount, 0);
    Player_CalcDamage(M_At(110.0f, 100.0f), m_Small, &by_bomb);
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_BOMB_SPARK);
    CHECK(m_Effects[0].pos.x == 110.0f);
    M_Teardown();
}

// Hits and grazes

TEST(a_bullet_on_the_hitbox_kills_the_player)
{
    M_SetupAlive("die", 0, 0);
    g_Enemies.spell_card.capturable = true;
    g_Player.shots.laser_timers[1].current = 50;
    const float x = M_START_X - 1.25f - 1.0f;
    CHECK(Player_CheckHit(M_At(x, M_START_Y), m_Small) == PLAYER_CONTACT_BOX);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_DYING);
    CHECK_EQ_INT(g_Player.state.timer.current, 0);
    CHECK_EQ_INT(g_GameManager.stats.deaths, 1);
    CHECK(!g_Enemies.spell_card.capturable);
    CHECK_EQ_INT(g_Player.shots.laser_timers[1].current, 2);
    CHECK_EQ_INT(m_EffectCount, 2);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_SHOCKWAVE);
    CHECK_EQ_INT(m_Effects[0].color, 0xff4040ffu);
    CHECK_EQ_INT(m_Effects[1].type, EFFECT_PARTICLE + 2);
    CHECK_EQ_INT(m_Effects[1].count, 16);
    M_Teardown();
}

TEST(a_death_queues_the_death_sound)
{
    M_SetupAlive("die_sound", 0, 0);
    Sound_Reset();
    CHECK(Player_CheckHit(g_Player.pos, m_Small) == PLAYER_CONTACT_BOX);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_PLAYER_DEATH);
    M_Teardown();
}

TEST(a_bullet_past_the_hitbox_misses)
{
    M_SetupAlive("miss_player", 0, 0);
    const float x = M_START_X - 1.25f - 1.5f;
    CHECK(Player_CheckHit(M_At(x, M_START_Y), m_Small) == PLAYER_CONTACT_NONE);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_ALIVE);
    M_Teardown();
}

TEST(a_hit_only_kills_a_live_player)
{
    M_SetupAlive("invulnerable_hit", 0, 0);
    g_Player.state.value = PLAYER_INVULNERABLE;
    CHECK(Player_CheckHit(g_Player.pos, m_Small) == PLAYER_CONTACT_BOX);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_INVULNERABLE);
    CHECK_EQ_INT(g_GameManager.stats.deaths, 0);
    M_Teardown();
}

TEST(a_clear_area_wins_over_the_hitbox)
{
    M_SetupAlive("clear_area", 0, 0);
    g_Player.bomb.clear_areas[2] =
        (PLAYER_CLEAR_AREA) { 300.0f, 300.0f, 0.0f, 400.0f };
    CHECK(Player_CheckHit(g_Player.pos, m_Small) == PLAYER_CONTACT_BOX);
    M_Teardown();

    M_SetupAlive("clear_area_2", 0, 0);
    g_Player.bomb.clear_areas[2] =
        (PLAYER_CLEAR_AREA) { 300.0f, 300.0f, 220.0f, 200.0f };
    CHECK(Player_CheckHit(g_Player.pos, m_Small) == PLAYER_CONTACT_CLEARED);
    CHECK(Player_CheckGraze(g_Player.pos, m_Small) == PLAYER_CONTACT_CLEARED);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_ALIVE);
    M_Teardown();
}

TEST(a_graze_reaches_20_past_the_bullet)
{
    M_SetupAlive("graze", 0, 0);
    const float x = M_START_X - 1.25f - 20.0f - 1.0f;
    CHECK(Player_CheckGraze(M_At(x, M_START_Y), m_Small) == PLAYER_CONTACT_BOX);
    CHECK_EQ_INT(g_GameManager.player.graze, 1);
    CHECK_EQ_INT(g_GameManager.player.graze_total, 1);
    CHECK_EQ_INT(g_GameManager.score.value, 500);
    CHECK_EQ_INT(g_GameManager.rank.sub, 6);
    CHECK_EQ_INT(m_EffectCount, 1);
    CHECK_EQ_INT(m_Effects[0].type, EFFECT_SMALL_PARTICLE);
    CHECK(m_Effects[0].pos.x == (M_START_X + x) * 0.5f);
    CHECK(
        Player_CheckGraze(M_At(x - 0.5f, M_START_Y), m_Small)
        == PLAYER_CONTACT_NONE);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_ALIVE);
    M_Teardown();
}

TEST(a_graze_queues_the_graze_sound)
{
    M_SetupAlive("graze_sound", 0, 0);
    Sound_Reset();
    CHECK(Player_CheckGraze(g_Player.pos, m_Small) == PLAYER_CONTACT_BOX);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_GRAZE);
    M_Teardown();
}

TEST(the_graze_counters_stop_at_their_limits)
{
    M_SetupAlive("graze_limit", 0, 0);
    g_GameManager.player.graze = 9999;
    g_GameManager.player.graze_total = 999999;
    Player_CheckGraze(g_Player.pos, m_Small);
    CHECK_EQ_INT(g_GameManager.player.graze, 9999);
    CHECK_EQ_INT(g_GameManager.player.graze_total, 999999);
    M_Teardown();
}

TEST(a_graze_during_a_bomb_scores_but_does_not_count)
{
    M_SetupAlive("graze_bomb", 0, 0);
    g_Player.bomb.running = true;
    CHECK(Player_CheckGraze(g_Player.pos, m_Small) == PLAYER_CONTACT_BOX);
    CHECK_EQ_INT(g_GameManager.player.graze, 0);
    CHECK_EQ_INT(g_GameManager.score.value, 500);
    M_Teardown();
}

TEST(no_graze_while_dying_or_respawning)
{
    M_SetupAlive("graze_dying", 0, 0);
    g_Player.state.value = PLAYER_DYING;
    CHECK(Player_CheckGraze(g_Player.pos, m_Small) == PLAYER_CONTACT_NONE);
    g_Player.state.value = PLAYER_RESPAWNING;
    CHECK(Player_CheckGraze(g_Player.pos, m_Small) == PLAYER_CONTACT_NONE);
    g_Player.state.value = PLAYER_INVULNERABLE;
    CHECK(Player_CheckGraze(g_Player.pos, m_Small) == PLAYER_CONTACT_BOX);
    M_Teardown();
}

TEST(a_laser_test_turns_the_player_into_the_laser)
{
    M_SetupAlive("laser_turn", 0, 0);
    const VEC3 origin = M_At(M_START_X - 100.0f, M_START_Y);
    Player_CheckLaserHit(
        g_Player.pos, (VEC3) { 4.0f, 4.0f, 0.0f }, origin, M_HALF_PI, true);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_ALIVE);
    CHECK_EQ_INT(g_GameManager.player.graze, 0);
    Player_CheckLaserHit(
        M_At(M_START_X - 100.0f, M_START_Y - 100.0f),
        (VEC3) { 4.0f, 4.0f, 0.0f }, origin, M_HALF_PI, true);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_DYING);
    M_Teardown();
}

TEST(a_laser_grazes_within_48)
{
    M_SetupAlive("laser_graze", 0, 0);
    const VEC3 near = M_At(M_START_X + 1.25f + 1.0f + 48.0f, M_START_Y);
    Player_CheckLaserHit(near, m_Small, M_At(0.0f, 0.0f), 0.0f, false);
    CHECK_EQ_INT(g_GameManager.player.graze, 0);
    Player_CheckLaserHit(near, m_Small, M_At(0.0f, 0.0f), 0.0f, true);
    CHECK_EQ_INT(g_GameManager.player.graze, 1);
    CHECK(m_Effects[0].pos.x == M_START_X && m_Effects[0].pos.y == M_START_Y);
    const VEC3 far = M_At(near.x + 0.5f, M_START_Y);
    Player_CheckLaserHit(far, m_Small, M_At(0.0f, 0.0f), 0.0f, true);
    CHECK_EQ_INT(g_GameManager.player.graze, 1);
    g_Player.state.value = PLAYER_RESPAWNING;
    Player_CheckLaserHit(near, m_Small, M_At(0.0f, 0.0f), 0.0f, true);
    CHECK_EQ_INT(g_GameManager.player.graze, 1);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_RESPAWNING);
    M_Teardown();
}

TEST(items_touch_the_graze_box_while_alive_or_invulnerable)
{
    M_SetupAlive("graze_box", 0, 0);
    const VEC3 item = M_At(M_START_X + 12.0f + 1.0f, M_START_Y);
    CHECK(Player_TouchesGrazeBox(item, m_Small));
    CHECK(!Player_TouchesGrazeBox(M_At(item.x + 0.5f, M_START_Y), m_Small));
    g_Player.state.value = PLAYER_INVULNERABLE;
    CHECK(Player_TouchesGrazeBox(item, m_Small));
    g_Player.state.value = PLAYER_RESPAWNING;
    CHECK(!Player_TouchesGrazeBox(item, m_Small));
    M_Teardown();
}

TEST(the_angles_point_between_the_player_and_a_position)
{
    M_SetupAlive("angles", 0, 0);
    const VEC3 above = M_At(M_START_X + 30.0f, M_START_Y - 100.0f);
    CHECK(Player_AngleFrom(above) == X87_Atan2(100.0f, -30.0f));
    CHECK(Player_AngleTo(above) == X87_Atan2(-100.0f, 30.0f));
    CHECK(Player_AngleFrom(g_Player.pos) == M_HALF_PI);
    CHECK(Player_AngleTo(g_Player.pos) == M_HALF_PI);
    M_Teardown();
}

// Bombs

TEST(the_bomb_button_starts_a_bomb)
{
    M_SetupAlive("bomb", 0, 0);
    g_GameManager.player.bombs = 3;
    g_Enemies.spell_card.state = ENEMY_SPELL_TIMED_OUT;
    g_Enemies.spell_card.capturable = true;
    g_Buttons.pressed.bomb = true;
    M_Frame();
    CHECK(g_Player.bomb.running);
    CHECK_EQ_INT(g_Player.bomb.duration, 999);
    CHECK_EQ_INT(g_Player.bomb.timer.current, 0);
    CHECK_EQ_INT(m_BombCalls, 1);
    CHECK_EQ_INT(g_GameManager.player.bombs, 2);
    CHECK_EQ_INT(g_GameManager.stats.bombs_used, 1);
    CHECK_EQ_INT(g_GameManager.rank.value, 18);
    CHECK(!g_Enemies.spell_card.capturable);
    CHECK(g_Enemies.spell_card.bombed);
    M_Frame();
    CHECK_EQ_INT(m_BombCalls, 2);
    CHECK_EQ_INT(g_GameManager.player.bombs, 2);
    M_Teardown();
}

TEST(a_bomb_outside_a_spell_card_clears_bombed)
{
    M_SetupAlive("bomb_no_spell", 0, 0);
    g_GameManager.player.bombs = 1;
    g_Enemies.spell_card.bombed = true;
    g_Buttons.pressed.bomb = true;
    M_Frame();
    CHECK(!g_Enemies.spell_card.bombed);
    M_Teardown();
}

TEST(no_bomb_without_bombs_during_a_dialogue_or_while_respawning)
{
    M_SetupAlive("no_bomb", 0, 0);
    g_Buttons.pressed.bomb = true;
    M_Frame();
    CHECK(!g_Player.bomb.running);
    g_GameManager.player.bombs = 3;
    m_MsgRunning = true;
    M_Frame();
    CHECK(!g_Player.bomb.running);
    m_MsgRunning = false;
    g_Player.state.bomb_window = 0;
    M_Frame();
    CHECK(!g_Player.bomb.running);
    CHECK_EQ_INT(m_BombCalls, 0);
    M_Teardown();
}

TEST(the_update_clears_the_bomb_sizes_but_keeps_the_places)
{
    M_SetupAlive("bomb_clear", 0, 0);
    g_Player.bomb.running = true;
    g_Player.bomb.hits[0].size = (VEC3) { 5.0f, 6.0f, 0.0f };
    g_Player.bomb.clear_areas[0] =
        (PLAYER_CLEAR_AREA) { 1.0f, 2.0f, 3.0f, 4.0f };
    M_Frame();
    CHECK(m_BombSeenHit.size.x == 0.0f && m_BombSeenHit.size.y == 6.0f);
    CHECK(m_BombSeenArea.width == 0.0f);
    CHECK(m_BombSeenArea.x == 1.0f && m_BombSeenArea.y == 2.0f);
    CHECK(m_BombSeenArea.height == 4.0f);
    M_Teardown();
}

// Death

TEST(a_bomb_in_the_window_after_a_hit_saves_the_player)
{
    M_SetupAlive("deathbomb", 0, 0);
    g_GameManager.player.bombs = 3;
    g_GameManager.player.lives = 2;
    m_BombInvulnerable = true;
    Player_CheckHit(g_Player.pos, m_Small);
    M_Frames(5);
    CHECK_EQ_INT(g_Player.state.bomb_window, 1);
    g_Buttons.pressed.bomb = true;
    M_Frame();
    g_Buttons.pressed.bomb = false;
    M_Frames(40);
    CHECK_EQ_INT(m_ItemCount, 0);
    CHECK_EQ_INT(g_GameManager.player.lives, 2);
    M_Teardown();
}

TEST(the_items_drop_when_the_window_closes)
{
    M_SetupAlive("drop", 0, 0);
    g_GameManager.player.lives = 2;
    g_GameManager.player.power = 20;
    g_GameManager.player.power_item_combo = 5;
    Player_CheckHit(g_Player.pos, m_Small);
    M_Frames(5);
    CHECK_EQ_INT(m_ItemCount, 0);
    M_Frame();
    CHECK_EQ_INT(m_ItemCount, 6);
    CHECK_EQ_INT(m_Items[0].type, ITEM_BIG_POWER);
    for (int32_t i = 1; i < 6; i++) {
        CHECK_EQ_INT(m_Items[i].type, ITEM_POWER);
    }
    CHECK_EQ_INT(m_Items[5].state, ITEM_THROWN);
    CHECK(m_Items[0].pos.x == M_START_X && m_Items[0].pos.y == M_START_Y);
    CHECK_EQ_INT(g_GameManager.player.power, 4);
    CHECK_EQ_INT(g_GameManager.player.power_item_combo, 0);
    CHECK_EQ_INT(g_GameManager.rank.value, 4);
    M_Teardown();
}

TEST(a_death_below_16_power_takes_all_of_it)
{
    M_SetupAlive("drop_16", 0, 0);
    g_GameManager.player.lives = 2;
    g_GameManager.player.power = 10;
    Player_CheckHit(g_Player.pos, m_Small);
    M_Frames(6);
    CHECK_EQ_INT(g_GameManager.player.power, 0);
    M_Teardown();
}

TEST(the_last_death_drops_full_power_and_ends_the_extends)
{
    M_SetupAlive("drop_last", 0, 0);
    g_GameManager.player.power = 100;
    g_GameManager.score.next_extend = 2;
    Player_CheckHit(g_Player.pos, m_Small);
    M_Frames(6);
    CHECK_EQ_INT(m_ItemCount, 5);
    for (int32_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(m_Items[i].type, ITEM_FULL_POWER);
    }
    CHECK_EQ_INT(g_GameManager.player.power, 0);
    CHECK_EQ_INT(g_GameManager.score.next_extend, -1);
    M_Teardown();
}

TEST(the_player_respawns_30_frames_after_the_hit)
{
    M_SetupAlive("respawn", 0, 0);
    g_GameManager.player.lives = 2;
    g_GameManager.player.bombs = 0;
    g_GameManager.run.start_bombs = 2;
    Player_CheckHit(g_Player.pos, m_Small);
    g_Player.pos = M_At(100.0f, 100.0f);
    M_Frames(30);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_DYING);
    M_Frame();
    CHECK_EQ_INT(g_Player.state.value, PLAYER_RESPAWNING);
    CHECK(g_Player.pos.x == M_START_X && g_Player.pos.y == M_START_Y);
    CHECK(g_Player.pos.z == 0.2f);
    CHECK_EQ_INT(g_Player.state.timer.current, 1);
    CHECK_EQ_INT(g_Player.state.respawn_clear_frames, 89);
    CHECK_EQ_INT(g_GameManager.player.lives, 1);
    CHECK_EQ_INT(g_GameManager.player.bombs, 2);
    M_Frames(29);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_RESPAWNING);
    M_Frame();
    CHECK_EQ_INT(g_Player.state.value, PLAYER_INVULNERABLE);
    M_Teardown();
}

TEST(practice_and_the_extra_stage_respawn_with_3_bombs)
{
    M_SetupAlive("respawn_practice", 0, 0);
    g_GameManager.player.lives = 2;
    g_GameManager.run.start_bombs = 2;
    g_GameManager.mode.in_practice = true;
    Player_CheckHit(g_Player.pos, m_Small);
    M_Frames(31);
    CHECK_EQ_INT(g_GameManager.player.bombs, 3);
    M_Teardown();

    M_SetupAlive("respawn_extra", 0, 0);
    g_GameManager.player.lives = 2;
    g_GameManager.run.start_bombs = 2;
    g_GameManager.run.difficulty = 4;
    Player_CheckHit(g_Player.pos, m_Small);
    M_Frames(31);
    CHECK_EQ_INT(g_GameManager.player.bombs, 3);
    M_Teardown();
}

TEST(the_last_death_opens_the_continue_menu)
{
    M_SetupAlive("continue", 0, 0);
    g_GameManager.player.bombs = 1;
    g_GameManager.run.start_bombs = 2;
    Player_CheckHit(g_Player.pos, m_Small);
    M_Frames(31);
    CHECK(g_GameManager.mode.in_continue_menu);
    CHECK_EQ_INT(g_Player.state.value, PLAYER_RESPAWNING);
    CHECK_EQ_INT(g_GameManager.player.lives, 0);
    CHECK_EQ_INT(g_GameManager.player.bombs, 1);
    CHECK_EQ_INT(g_Player.state.timer.current, 1);
    CHECK_EQ_INT(g_Player.state.bomb_window, 0);
    M_Teardown();
}

TEST(player_options_and_shots_record_distinct_motion)
{
    M_SetupAlive("player_presentation", 0, 0);
    CHECK(IS_OK(Anm_CreateTexture(5, 64, 64, ANM_TEXTURE_RGBA8)));
    g_GameManager.player.power = 8;
    g_GameManager.camera.viewport = (VIEWPORT) { 0, 0, 640, 480, 0, 1 };
    g_GameManager.camera.view = Mat4_Identity();
    g_GameManager.camera.projection = Mat4_Identity();
    M_Shoot();
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK(GfxRecorder_QuadCount() >= 4);
    bool player = false;
    for (int32_t i = 0; i < GfxRecorder_QuadCount(); i++) {
        const uint64_t id = GfxRecorder_QuadId(i);
        CHECK(id != 0);
        player |= id == ((uint64_t)g_Player.vm.presentation_generation << 32);
    }
    CHECK(player);
    GfxRecorder_Stop();
    M_Teardown();
}
