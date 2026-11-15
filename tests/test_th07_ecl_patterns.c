#include "th07/ecl/patterns.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/bullet.h"
#include "th07/effect.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/stubs.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <SDL3/SDL.h>
#include <string.h>

#define M_SPRITE 0x200
#define M_SPRITE_8 (M_SPRITE + 0)
#define M_SPRITE_16 (M_SPRITE + 16)
#define M_SPRITE_32 (M_SPRITE + 32)
#define M_SPRITE_64 (M_SPRITE + 48)
#define M_SPRITE_OP 3
#define M_DELETE_OP 1
#define M_SEED 99
#define M_MAX_CALLS 64
// The pattern functions fire into the slots from here on.
#define M_FIRED 512
#define M_SLOW_SPRITE 0x26f
#define M_UNTURNED_SPRITE 0x27c

// The test links these instead of the modules they stand for. The player
// is at m_AimAngle from everything. The screen effects and the scripts of
// spawned enemies record their calls.

typedef enum {
    M_SHAKE,
    M_FLASH,
} M_SCREEN_KIND;

typedef struct {
    M_SCREEN_KIND kind;
    int32_t duration;
    int32_t a;
    uint32_t b;
} M_SCREEN_CALL;

typedef struct {
    int16_t sub;
    ECL_VARS vars;
    VEC3 pos;
} M_SPAWN_CALL;

RNG g_Rng;
CHAIN g_Chain;
float g_GameSpeed;
PLAYER g_Player;
GUI g_Gui;
ASCII_TEXT g_AsciiText;
uint8_t g_EffectsQuality = 2;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;
static ENEMY m_Enemy;
static ENEMY m_Boss;
static float m_AimAngle;
static VEC3 m_AngleFrom;
static M_SCREEN_CALL m_Screen[M_MAX_CALLS];
static int32_t m_ScreenCount;
static M_SPAWN_CALL m_Spawns[M_MAX_CALLS];
static int32_t m_SpawnCount;
static int16_t m_StartedSub;
static ANM_SPRITE m_TallSprite;
static ANM_SPRITE m_EdgeSprite;
static ANM_SPRITE m_BurstEdgeSprite;

void ScreenEffect_Shake(
    const int32_t duration, const int32_t from, const int32_t to)
{
    if (m_ScreenCount < M_MAX_CALLS) {
        m_Screen[m_ScreenCount] =
            (M_SCREEN_CALL) { M_SHAKE, duration, from, (uint32_t)to };
    }
    m_ScreenCount++;
}

void ScreenEffect_Flash(
    const int32_t duration, const int32_t count, const uint32_t color)
{
    if (m_ScreenCount < M_MAX_CALLS) {
        m_Screen[m_ScreenCount] =
            (M_SCREEN_CALL) { M_FLASH, duration, count, color };
    }
    m_ScreenCount++;
}

RESULT Ecl_Load(void)
{
    return OK;
}

void Ecl_Release(void)
{
}

int32_t Ecl_GetTimelineCount(void)
{
    return 0;
}

const ECL_TIMELINE_ENTRY *Ecl_GetTimeline(const int32_t index)
{
    static const ECL_TIMELINE_ENTRY end = { .time = -1 };
    return &end;
}

void Ecl_StartSub(ECL_STATE *const state, const int16_t sub)
{
    state->sub = sub;
    m_StartedSub = sub;
}

ECL_RUN Ecl_RunEnemy(ENEMY *const enemy)
{
    if (m_SpawnCount < M_MAX_CALLS) {
        m_Spawns[m_SpawnCount] =
            (M_SPAWN_CALL) { m_StartedSub, enemy->script.state.vars,
                             enemy->move.pos };
    }
    m_SpawnCount++;
    return ECL_RUN_CONTINUE;
}

float Player_AngleFrom(const VEC3 pos)
{
    m_AngleFrom = pos;
    return m_AimAngle;
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

static void M_AddScript(
    TH07_ANM_BUILDER_ANM *const anm, const int32_t script, const int32_t sprite,
    const int16_t end)
{
    const int32_t index = Th07AnmBuilder_BeginScript(anm, script - M_SPRITE);
    const uint32_t arg = (uint32_t)(sprite - M_SPRITE);
    Th07AnmBuilder_Instr(anm, index, M_SPRITE_OP, 0, 0, &arg, 1);
    Th07AnmBuilder_Instr(anm, index, M_DELETE_OP, end, 0, nullptr, 0);
    Th07AnmBuilder_End(anm, index);
}

// Writes etama.anm as th07/bullet's test does: 16 sprites each 8, 16, 32
// and 64 high. The flying scripts of types 0, 1 to 6, 7 to 9 and 10 show
// the first sprite of each height.
static void M_WriteEtama(void)
{
    static TH07_ANM_BUILDER builder;
    Th07AnmBuilder_Init(&builder);
    TH07_ANM_BUILDER_ANM *const anm =
        Th07AnmBuilder_Add(&builder, 256, 256, "@");
    for (int32_t i = 0; i < 16; i++) {
        Th07AnmBuilder_AddSprite(anm, i, 0.0f, 0.0f, 8.0f, 8.0f);
        Th07AnmBuilder_AddSprite(anm, 16 + i, 0.0f, 0.0f, 16.0f, 16.0f);
        Th07AnmBuilder_AddSprite(anm, 32 + i, 0.0f, 0.0f, 32.0f, 32.0f);
        Th07AnmBuilder_AddSprite(anm, 48 + i, 0.0f, 0.0f, 64.0f, 64.0f);
    }
    M_AddScript(anm, 512, M_SPRITE_8, 1000);
    for (int32_t i = 513; i <= 518; i++) {
        M_AddScript(anm, i, M_SPRITE_16, 1000);
    }
    for (int32_t i = 519; i <= 521; i++) {
        M_AddScript(anm, i, M_SPRITE_32, 1000);
    }
    M_AddScript(anm, 680, M_SPRITE_64, 1000);
    M_AddScript(anm, 527, M_SPRITE_8, 4);
    M_AddScript(anm, 528, M_SPRITE_16, 4);
    M_AddScript(anm, 529, M_SPRITE_32, 4);
    M_AddScript(anm, 681, M_SPRITE_64, 4);
    for (int32_t i = 0; i < 3; i++) {
        M_AddScript(anm, 530 + i, M_SPRITE_8, (int16_t)(2 + i));
        M_AddScript(anm, 533 + i, M_SPRITE_16, (int16_t)(2 + i));
    }
    M_AddScript(anm, 536, M_SPRITE_32, 3);
    M_AddScript(anm, 682, M_SPRITE_64, 3);
    Th07AnmBuilder_WriteFile(&builder, m_Directory, "data/etama.anm");
}

static void M_WriteOneSprite(const char *const name)
{
    TH07_ANM_BUILDER builder;
    Th07AnmBuilder_Init(&builder);
    TH07_ANM_BUILDER_ANM *const anm = Th07AnmBuilder_Add(&builder, 64, 64, "@");
    Th07AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 8.0f, 8.0f);
    Th07AnmBuilder_End(anm, Th07AnmBuilder_BeginScript(anm, 0));
    Th07AnmBuilder_WriteFile(&builder, m_Directory, name);
}

static void M_SetUp(const char *const case_name)
{
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    CHECK(SDL_CreateDirectory(data));
    Mem_Free(data);
    M_WriteEtama();
    M_WriteOneSprite("data/eff01.anm");
    M_WriteOneSprite("data/stg1enm.anm");
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, M_SEED);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    g_GameManager.run.stage = 1;
    g_GameManager.run.difficulty = 1;
    memset(&g_Player, 0, sizeof(g_Player));
    memset(&g_Stage, 0, sizeof(g_Stage));
    g_GameSpeed = 1.0f;
    CHECK(Result_Log(Bullet_Register(), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Effect_Register(), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Enemy_Register(), LOG_LEVEL_ERROR));
    // Gives every effect type a script that runs on.
    for (int32_t i = 0x2ab; i <= 0x304; i++) {
        Anm_SetScriptEntry(i, Anm_GetScript(0x2dc));
    }
    // Stands in for the sprites that the slow down and the patterns pick,
    // which lie past the test sprites.
    Anm_SetSpriteEntry(M_SLOW_SPRITE, Anm_GetSpriteEntry(M_SPRITE_16));
    Anm_SetSpriteEntry(M_SLOW_SPRITE - 4, Anm_GetSpriteEntry(M_SPRITE_16 + 1));
    m_TallSprite = *Anm_GetSpriteEntry(M_SPRITE_64);
    m_TallSprite.height = 61.0f;
    m_EdgeSprite = m_TallSprite;
    m_EdgeSprite.height = 60.0f;
    m_BurstEdgeSprite = m_TallSprite;
    m_BurstEdgeSprite.height = 48.0f;
    memset(&m_Enemy, 0, sizeof(m_Enemy));
    memset(&m_Boss, 0, sizeof(m_Boss));
    m_Enemy.script.state.sub = 5;
    g_Bullets.next_bullet = M_FIRED;
    m_AimAngle = 0.0f;
    m_ScreenCount = 0;
    m_SpawnCount = 0;
    Sound_Reset();
}

static void M_TearDown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Files_Shutdown();
    Mem_Free(m_Directory);
}

static void M_Run(const ECL_FUNC func, const int32_t arg)
{
    Ecl_RunPattern(&m_Enemy, func, arg);
}

// Puts a bullet of type 1 in slot index, flying from pos at speed.
static BULLET *M_AddBullet(
    const int32_t index, const int16_t color, const VEC3 pos, const float speed)
{
    BULLET *const bullet = &g_Bullets.bullets[index];
    memset(bullet, 0, sizeof(*bullet));
    bullet->sprites = g_Bullets.types[1];
    bullet->state = BULLET_FLYING;
    bullet->color = color;
    bullet->pos = pos;
    bullet->speed = speed;
    return bullet;
}

static BULLET *M_AddTall(
    const int32_t index, const int16_t color, const VEC3 pos)
{
    BULLET *const bullet = M_AddBullet(index, color, pos, 1.0f);
    bullet->sprites.flying.sprite_data = &m_TallSprite;
    return bullet;
}

static BULLET *M_Fired(const int32_t n)
{
    return &g_Bullets.bullets[M_FIRED + n];
}

static int32_t M_FiredCount(void)
{
    int32_t count = 0;
    for (int32_t i = M_FIRED; i < BULLET_COUNT; i++) {
        if (g_Bullets.bullets[i].state != BULLET_FREE) {
            count++;
        }
    }
    return count;
}

static bool M_CommandIs(
    const BULLET_COMMAND *const command, const uint32_t flag,
    const int32_t frames, const float arg0, const float arg1)
{
    return command->flag == flag && command->frames == frames
        && command->args[0] == arg0 && command->args[1] == arg1
        && !command->concurrent;
}

static bool M_ScreenIs(
    const int32_t index, const M_SCREEN_KIND kind, const int32_t duration,
    const int32_t a, const uint32_t b)
{
    const M_SCREEN_CALL *const call = &m_Screen[index];
    return call->kind == kind && call->duration == duration && call->a == a
        && call->b == b;
}

static VEC3 M_Polar(const float angle, const float speed)
{
    VEC3 v = {};
    X87_SinCosScale(angle, speed, &v.x, &v.y);
    return v;
}

static LASER *M_AddLaser(
    const int32_t index, const VEC3 pos, const float angle, const float width)
{
    LASER *const laser = &g_Bullets.lasers[index];
    memset(laser, 0, sizeof(*laser));
    laser->active = true;
    laser->state = LASER_FIRING;
    laser->pos = pos;
    laser->angle = angle;
    laser->start_offset = 10.0f;
    laser->end_offset = 110.0f;
    laser->width = width;
    return laser;
}

TEST(warp_to_boss_takes_its_position_velocity_and_angle)
{
    M_SetUp("warp");
    m_Boss.move.pos = (VEC3) { 1.0f, 2.0f, 3.0f };
    m_Boss.move.velocity = (VEC3) { 4.0f, 5.0f, 6.0f };
    m_Boss.move.angle = 0.5f;
    g_Enemies.bosses[2] = &m_Boss;
    M_Run(ECL_FUNC_WARP_TO_BOSS, 2);
    CHECK(m_Enemy.move.pos.x == 1.0f && m_Enemy.move.pos.z == 3.0f);
    CHECK(m_Enemy.move.velocity.y == 5.0f);
    CHECK(m_Enemy.move.angle == 0.5f);
    CHECK(m_Enemy.flags.warping_to_boss);
    M_TearDown();
}

static float M_SpinAngle(RNG *const rng, const float sign)
{
    return sign * MATH_PI / (Rng_F32(rng) * 60.0f + 180.0f);
}

TEST(spin_bullets_curves_each_unturned_bullet_by_its_color)
{
    M_SetUp("spin");
    const VEC3 pos = { 100.0f, 100.0f, 0.0f };
    BULLET *const red = M_AddBullet(0, 2, pos, 2.0f);
    BULLET *const blue = M_AddBullet(1, 6, pos, 2.0f);
    // Keeps the angle of the bullet before it.
    BULLET *const other = M_AddBullet(2, 3, pos, 2.0f);
    BULLET *const turned = M_AddBullet(3, 4, pos, 2.0f);
    turned->pattern_state = 1;
    BULLET *const fading = M_AddBullet(4, 8, pos, 2.0f);
    fading->state = BULLET_FADING;
    red->commands[1].flag = BULLET_FLAG_TURN;
    red->next_command = 2;
    RNG expected = g_Rng;
    M_Run(ECL_FUNC_SPIN_BULLETS, 0);
    CHECK_EQ_INT(m_ScreenCount, 2);
    CHECK(M_ScreenIs(0, M_SHAKE, 30, 12, 0));
    CHECK(M_ScreenIs(1, M_FLASH, 4, 3, COLOR_ARGB(0x80, 0xff, 0xcf, 0xcf)));
    const float red_angle = M_SpinAngle(&expected, -1.0f);
    const float blue_angle = M_SpinAngle(&expected, 1.0f);
    CHECK(M_CommandIs(
        &red->commands[0], BULLET_FLAG_CURVE, 60, 1.0f / 60.0f, red_angle));
    CHECK_EQ_INT(red->commands[1].flag, 0);
    CHECK_EQ_INT(red->next_command, 0);
    CHECK(red->speed == 0.3f);
    CHECK_EQ_INT(red->pattern_state, 1);
    CHECK(M_CommandIs(
        &blue->commands[0], BULLET_FLAG_CURVE, 60, 1.0f / 60.0f, blue_angle));
    CHECK(other->commands[0].args[1] == blue_angle);
    CHECK_EQ_INT(turned->commands[0].flag, 0);
    CHECK_EQ_INT(fading->pattern_state, 0);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_TearDown();
}

TEST(spin_bullets_on_lunatic_curves_longer)
{
    M_SetUp("spin_lunatic");
    g_GameManager.run.difficulty = 3;
    BULLET *const bullet = M_AddBullet(0, 8, (VEC3) {}, 2.0f);
    RNG expected = g_Rng;
    M_Run(ECL_FUNC_SPIN_BULLETS, 0);
    CHECK(M_CommandIs(
        &bullet->commands[0], BULLET_FLAG_CURVE, 240, 1.0f / 190.0f,
        M_SpinAngle(&expected, 1.0f)));
    M_TearDown();
}

TEST(spin_bullets_variants_pick_one_color)
{
    M_SetUp("spin_variants");
    BULLET *const red = M_AddBullet(0, 4, (VEC3) {}, 2.0f);
    BULLET *const green = M_AddBullet(1, 8, (VEC3) {}, 2.0f);
    M_Run(ECL_FUNC_SPIN_BULLETS, 1);
    CHECK_EQ_INT(red->pattern_state, 0);
    CHECK_EQ_INT(green->pattern_state, 1);
    M_Run(ECL_FUNC_SPIN_BULLETS, 2);
    CHECK_EQ_INT(red->pattern_state, 1);
    M_TearDown();
}

TEST(fans_from_near_bullets_replace_the_near_red_bullets)
{
    M_SetUp("fans_near");
    m_Enemy.move.pos = (VEC3) { 100.0f, 100.0f, 0.0f };
    BULLET *const near =
        M_AddBullet(0, 2, (VEC3) { 160.0f, 180.0f, 0.0f }, 1.0f);
    BULLET *const far =
        M_AddBullet(1, 2, (VEC3) { 200.0f, 180.0f, 0.0f }, 1.0f);
    BULLET *const blue =
        M_AddBullet(2, 6, (VEC3) { 100.0f, 110.0f, 0.0f }, 1.0f);
    RNG expected = g_Rng;
    M_Run(ECL_FUNC_FANS_FROM_NEAR_BULLETS, 0);
    CHECK(M_ScreenIs(0, M_SHAKE, 32, 12, 0));
    CHECK(M_ScreenIs(1, M_FLASH, 4, 1, COLOR_ARGB(0x80, 0xcf, 0xcf, 0xff)));
    CHECK_EQ_INT(near->state, BULLET_FREE);
    CHECK_EQ_INT(far->state, BULLET_FLYING);
    CHECK_EQ_INT(blue->state, BULLET_FLYING);
    CHECK_EQ_INT(M_FiredCount(), 2);
    const float accel = Rng_F32InRange(&expected, 0.005f) + 0.013f;
    for (int32_t i = 0; i < 2; i++) {
        const BULLET *const fired = M_Fired(i);
        CHECK_EQ_INT(fired->color, 6);
        CHECK_EQ_INT(fired->state, BULLET_SPAWNING_1);
        CHECK(M_CommandIs(
            &fired->commands[0], BULLET_FLAG_ACCELERATE, 180, accel,
            MATH_HALF_PI));
        CHECK_EQ_INT(
            fired->flags, BULLET_FLAG_SPAWN_1 | BULLET_FLAG_ACCELERATE);
        CHECK(fired->speed == 0.7f);
    }
    M_TearDown();
}

TEST(fans_from_near_bullets_variants_reach_further)
{
    M_SetUp("fans_range");
    m_Enemy.move.pos = (VEC3) { 0.0f, 0.0f, 0.0f };
    BULLET *const bullet =
        M_AddBullet(0, 2, (VEC3) { 0.0f, 192.0f, 0.0f }, 1.0f);
    M_Run(ECL_FUNC_FANS_FROM_NEAR_BULLETS, 1);
    CHECK_EQ_INT(m_ScreenCount, 0);
    CHECK_EQ_INT(bullet->state, BULLET_FLYING);
    M_Run(ECL_FUNC_FANS_FROM_NEAR_BULLETS, 2);
    CHECK_EQ_INT(bullet->state, BULLET_FREE);
    BULLET *const far = M_AddBullet(0, 2, (VEC3) { 0.0f, 998.0f, 0.0f }, 1.0f);
    M_Run(ECL_FUNC_FANS_FROM_NEAR_BULLETS, 3);
    CHECK_EQ_INT(far->state, BULLET_FREE);
    M_TearDown();
}

TEST(nop_does_nothing)
{
    M_SetUp("nop");
    BULLET *const bullet = M_AddBullet(0, 2, (VEC3) {}, 1.0f);
    const RNG expected = g_Rng;
    M_Run(ECL_FUNC_NOP, 0);
    CHECK_EQ_INT(bullet->state, BULLET_FLYING);
    CHECK_EQ_INT(m_ScreenCount, 0);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_TearDown();
}

TEST(tall_bullet_to_effect_replaces_the_first_tall_bullet)
{
    M_SetUp("tall_effect");
    m_Enemy.script.state.vars.floats[1] = 7.0f;
    BULLET *const edge = M_AddBullet(0, 0, (VEC3) { 1.0f, 2.0f, 0.0f }, 1.0f);
    edge->sprites.flying.sprite_data = &m_EdgeSprite;
    edge->state = BULLET_FADING;
    BULLET *const short_one =
        M_AddBullet(1, 0, (VEC3) { 3.0f, 4.0f, 0.0f }, 1.0f);
    BULLET *const first = M_AddTall(2, 0, (VEC3) { 50.0f, 60.0f, 0.0f });
    BULLET *const second = M_AddTall(3, 0, (VEC3) { 70.0f, 80.0f, 0.0f });
    // The effect goes into the slot after this one.
    EFFECT *const before =
        Effect_Spawn(EFFECT_EXPLOSION, (VEC3) {}, 1, COLOR_WHITE);
    M_Run(ECL_FUNC_TALL_BULLET_TO_EFFECT, 0);
    CHECK(m_Enemy.script.state.vars.floats[0] == 50.0f);
    CHECK(m_Enemy.script.state.vars.floats[1] == 60.0f);
    CHECK_EQ_INT(first->state, BULLET_FREE);
    CHECK_EQ_INT(second->state, BULLET_FLYING);
    CHECK_EQ_INT(short_one->state, BULLET_FLYING);
    CHECK_EQ_INT(edge->state, BULLET_FADING);
    const EFFECT *const effect = before + 1;
    CHECK(effect->active);
    CHECK_EQ_INT(effect->type, EFFECT_EXPLOSION_LARGE);
    CHECK(effect->pos.x == 50.0f && effect->pos.y == 60.0f);
    M_Run(ECL_FUNC_TALL_BULLET_TO_EFFECT, 0);
    CHECK(m_Enemy.script.state.vars.floats[0] == 70.0f);
    second->state = BULLET_FREE;
    M_Run(ECL_FUNC_TALL_BULLET_TO_EFFECT, 0);
    CHECK(m_Enemy.script.state.vars.floats[0] == -999.0f);
    CHECK(m_Enemy.script.state.vars.floats[1] == 80.0f);
    edge->state = BULLET_FLYING;
    M_Run(ECL_FUNC_TALL_BULLET_TO_EFFECT, 0);
    CHECK(m_Enemy.script.state.vars.floats[0] == 1.0f);
    M_TearDown();
}

TEST(copy_boss_circle_takes_the_circle_of_boss_0)
{
    M_SetUp("boss_circle");
    m_Boss.move.pos = (VEC3) { 10.0f, 20.0f, 30.0f };
    m_Boss.move.circle.radius = 40.0f;
    m_Boss.move.circle.angular_velocity = 0.25f;
    m_Boss.move.circle.angle = 1.0f;
    g_Enemies.bosses[0] = &m_Boss;
    M_Run(ECL_FUNC_COPY_BOSS_CIRCLE, 0);
    CHECK(m_Enemy.move.start.x == 10.0f && m_Enemy.move.start.z == 30.0f);
    CHECK(m_Enemy.move.circle.radius == 40.0f);
    CHECK(m_Enemy.move.circle.angular_velocity == 0.25f);
    CHECK(m_Enemy.move.circle.angle == 0.0f);
    M_TearDown();
}

// Checks the three fans that fans back from bullets fires: 4 columns (or
// 2 on Lunatic), 2 and 1, with the flags of each.
static void M_CheckFansBack(
    const int16_t color, const uint16_t first_flags, const uint16_t flags)
{
    const int32_t first = g_GameManager.run.difficulty < 3 ? 4 : 2;
    CHECK_EQ_INT(M_FiredCount(), first + 3);
    for (int32_t i = 0; i < first + 3; i++) {
        const BULLET *const fired = M_Fired(i);
        CHECK_EQ_INT(fired->color, color);
        CHECK_EQ_INT(fired->flags, i < first ? first_flags : flags);
        CHECK(M_CommandIs(
            &fired->commands[0], BULLET_FLAG_STAY_OUTSIDE, 130, 0.0f, 0.0f));
    }
    CHECK(M_Fired(0)->speed == 2.0f * 1.1f);
    CHECK(M_Fired(first)->speed == 2.0f * 0.7f);
    CHECK(M_Fired(first + 2)->speed == 2.0f * 0.85f);
}

TEST(fans_back_from_bullets_fire_three_fans_back)
{
    M_SetUp("fans_back");
    BULLET *const source =
        M_AddBullet(0, 6, (VEC3) { 100.0f, 100.0f, 0.0f }, 2.0f);
    source->angle = 1.0f;
    BULLET *const other =
        M_AddBullet(1, 2, (VEC3) { 100.0f, 100.0f, 0.0f }, 2.0f);
    M_Run(ECL_FUNC_FANS_BACK_FROM_BULLETS, 0);
    CHECK_EQ_INT(source->state, BULLET_FREE);
    CHECK_EQ_INT(other->state, BULLET_FLYING);
    M_CheckFansBack(
        15, BULLET_FLAG_STAY_OUTSIDE | BULLET_FLAG_SPAWN_1,
        BULLET_FLAG_STAY_OUTSIDE);
    const float back = Angle_AddNormalize(1.0f, MATH_PI);
    CHECK(M_Fired(6)->angle == Angle_AddNormalize(back, 0.0f));
    M_TearDown();
}

TEST(fans_back_from_bullets_drop_their_command_flag_but_on_lunatic)
{
    M_SetUp("fans_back_flags");
    M_AddBullet(0, 15, (VEC3) { 100.0f, 100.0f, 0.0f }, 2.0f);
    M_Run(ECL_FUNC_FANS_BACK_FROM_BULLETS, 1);
    M_CheckFansBack(2, BULLET_FLAG_SPAWN_1, 0);
    memset(g_Bullets.bullets, 0, sizeof(g_Bullets.bullets));
    g_Bullets.next_bullet = M_FIRED;
    g_GameManager.run.difficulty = 3;
    M_AddBullet(0, 15, (VEC3) { 100.0f, 100.0f, 0.0f }, 2.0f);
    M_Run(ECL_FUNC_FANS_BACK_FROM_BULLETS, 1);
    M_CheckFansBack(
        2, BULLET_FLAG_STAY_OUTSIDE | BULLET_FLAG_SPAWN_1,
        BULLET_FLAG_STAY_OUTSIDE);
    memset(g_Bullets.bullets, 0, sizeof(g_Bullets.bullets));
    g_Bullets.next_bullet = M_FIRED;
    M_AddBullet(0, 2, (VEC3) { 100.0f, 100.0f, 0.0f }, 2.0f);
    M_Run(ECL_FUNC_FANS_BACK_FROM_BULLETS, 2);
    M_CheckFansBack(10, BULLET_FLAG_SPAWN_1, 0);
    M_TearDown();
}

TEST(turn_at_laser_edges_turns_the_bullets_on_a_laser)
{
    M_SetUp("laser_edges");
    // Along x from 110 to 210 at y 100, 20 wide.
    M_AddLaser(0, (VEC3) { 100.0f, 100.0f, 0.0f }, 0.0f, 20.0f);
    BULLET *const inside =
        M_AddBullet(0, 3, (VEC3) { 200.0f, 109.0f, 0.0f }, 2.0f);
    inside->velocity = (VEC3) { 0.0f, -1.0f, 0.0f };
    BULLET *const outside =
        M_AddBullet(1, 3, (VEC3) { 200.0f, 111.0f, 0.0f }, 2.0f);
    BULLET *const counting =
        M_AddBullet(2, 3, (VEC3) { 120.0f, 95.0f, 0.0f }, 0.4f);
    counting->pattern_state = 2;
    counting->velocity = (VEC3) { 1.0f, 0.0f, 0.0f };
    g_GameSpeed = 0.5f;
    M_Run(ECL_FUNC_TURN_AT_LASER_EDGES, 0);
    CHECK(inside->speed == 2.0f - 0.1f);
    CHECK(inside->angle == -MATH_HALF_PI);
    const VEC3 v = M_Polar(-MATH_HALF_PI, 0.5f * (2.0f - 0.1f));
    CHECK(inside->velocity.x == v.x && inside->velocity.y == v.y);
    CHECK_EQ_INT(inside->pattern_state, 10);
    CHECK_EQ_INT(
        inside->sprites.flying.sprite, g_Bullets.types[5].flying.sprite + 3);
    CHECK_EQ_INT(outside->pattern_state, 0);
    CHECK_EQ_INT(counting->pattern_state, 1);
    M_Run(ECL_FUNC_TURN_AT_LASER_EDGES, 0);
    CHECK_EQ_INT(inside->pattern_state, 9);
    CHECK_EQ_INT(counting->pattern_state, 10);
    CHECK(counting->speed == 0.4f);
    CHECK(counting->angle == MATH_HALF_PI);
    M_TearDown();
}

TEST(turn_at_laser_edges_only_ever_matches_lasers_0_and_1)
{
    M_SetUp("laser_edges_index");
    M_AddLaser(2, (VEC3) { 100.0f, 100.0f, 0.0f }, 0.0f, 20.0f);
    // A laser turned to point down: along y from 110 to 210 at x 100.
    M_AddLaser(1, (VEC3) { 100.0f, 100.0f, 0.0f }, MATH_HALF_PI, 20.0f);
    BULLET *const on_two =
        M_AddBullet(0, 3, (VEC3) { 200.0f, 100.0f, 0.0f }, 2.0f);
    BULLET *const on_one =
        M_AddBullet(1, 3, (VEC3) { 105.0f, 200.0f, 0.0f }, 2.0f);
    for (int32_t frame = 0; frame < 4; frame++) {
        m_Enemy.callbacks.timer.current = frame;
        M_Run(ECL_FUNC_TURN_AT_LASER_EDGES, 0);
    }
    CHECK_EQ_INT(on_two->pattern_state, 0);
    CHECK_EQ_INT(on_one->pattern_state, 9);
    g_Bullets.lasers[1].state = LASER_ENDING;
    on_one->pattern_state = 0;
    m_Enemy.callbacks.timer.current = 1;
    M_Run(ECL_FUNC_TURN_AT_LASER_EDGES, 0);
    CHECK_EQ_INT(on_one->pattern_state, 0);
    M_TearDown();
}

TEST(deflect_off_lasers_sends_bullets_along_its_normal)
{
    M_SetUp("deflect");
    g_GameSpeed = 0.5f;
    M_AddLaser(0, (VEC3) { 100.0f, 100.0f, 0.0f }, 0.0f, 20.0f);
    // Inside 1.5 times the width only.
    BULLET *const bullet =
        M_AddBullet(0, 3, (VEC3) { 200.0f, 114.0f, 0.0f }, 2.0f);
    bullet->velocity = (VEC3) { 1.0f, 1.0f, 0.0f };
    BULLET *const up = M_AddBullet(1, 3, (VEC3) { 200.0f, 90.0f, 0.0f }, 2.0f);
    up->velocity = (VEC3) { 1.0f, -1.0f, 0.0f };
    BULLET *const outside =
        M_AddBullet(2, 3, (VEC3) { 200.0f, 116.0f, 0.0f }, 2.0f);
    RNG expected = g_Rng;
    M_Run(ECL_FUNC_DEFLECT_OFF_LASERS, 0);
    const float speed = 2.0f * (Rng_F32(&expected) * 0.3f + 0.7f);
    CHECK(bullet->speed == speed);
    const float angle = X87_Atan2(1.0f, -0.0f);
    CHECK(bullet->angle == angle);
    const VEC3 v = M_Polar(angle, speed);
    CHECK(bullet->velocity.x == v.x && bullet->velocity.y == v.y);
    CHECK_EQ_INT(bullet->pattern_state, -1);
    CHECK_EQ_INT(
        bullet->sprites.flying.sprite, g_Bullets.types[5].flying.sprite + 3);
    Rng_F32(&expected);
    CHECK(up->angle == X87_Atan2(-1.0f, 0.0f));
    CHECK_EQ_INT(outside->pattern_state, 0);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_Run(ECL_FUNC_DEFLECT_OFF_LASERS, 0);
    CHECK(bullet->speed == speed);
    M_TearDown();
}

TEST(deflect_off_lasers_on_hard_marks_the_laser)
{
    M_SetUp("deflect_hard");
    g_GameManager.run.difficulty = 2;
    for (int32_t i = 0; i < 4; i++) {
        M_AddLaser(i, (VEC3) { 100.0f, 100.0f, 0.0f }, 0.0f, 20.0f);
    }
    BULLET *const bullet =
        M_AddBullet(0, 3, (VEC3) { 200.0f, 100.0f, 0.0f }, 2.0f);
    bullet->velocity = (VEC3) { 0.0f, 1.0f, 0.0f };
    RNG expected = g_Rng;
    m_Enemy.callbacks.timer.current = 4;
    M_Run(ECL_FUNC_DEFLECT_OFF_LASERS, 0);
    // Of lasers 0 to 3, only laser 1 matches frame 4.
    CHECK_EQ_INT(bullet->pattern_state, 2);
    CHECK(bullet->speed == 2.0f * (Rng_F32(&expected) * 0.4f + 0.8f));
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    m_Enemy.callbacks.timer.current = 0;
    M_Run(ECL_FUNC_DEFLECT_OFF_LASERS, 0);
    CHECK_EQ_INT(bullet->pattern_state, 4);
    M_TearDown();
}

TEST(start_petals_falling_shakes_and_drops_the_petals)
{
    M_SetUp("petals");
    EFFECT *const petal =
        Effect_Spawn(EFFECT_PETAL_DRIFT, (VEC3) {}, 1, COLOR_WHITE);
    petal->acceleration.z = 0.0f;
    M_Run(ECL_FUNC_START_PETALS_FALLING, 0);
    CHECK(M_ScreenIs(0, M_SHAKE, 80, 8, 0));
    CHECK(petal->acceleration.z == -0.01f);
    M_TearDown();
}

TEST(slow_down_and_end_slow_down_scale_the_bullets)
{
    M_SetUp("slow");
    BULLET *const slow = M_AddBullet(0, 0, (VEC3) {}, 1.0f);
    Anm_SetSprite(&slow->sprites.flying, M_SLOW_SPRITE - 4);
    slow->velocity = (VEC3) { 3.0f, -6.0f, 1.0f };
    BULLET *const fading = M_AddBullet(1, 0, (VEC3) {}, 1.0f);
    fading->state = BULLET_FADING;
    fading->velocity = (VEC3) { 1.0f, 0.0f, 0.0f };
    BULLET *const freed = M_AddBullet(2, 0, (VEC3) {}, 1.0f);
    freed->state = BULLET_FREE;
    freed->velocity = (VEC3) { 1.0f, 0.0f, 0.0f };
    const int16_t sprite = fading->sprites.flying.sprite;
    M_Run(ECL_FUNC_SLOW_DOWN, 3);
    const float speed = 1.0f / 3.0f;
    CHECK(g_GameSpeed == speed);
    CHECK_EQ_INT(g_Stage.spell_background.sprites[0].pending_interrupt, 2);
    CHECK_EQ_INT(g_Stage.spell_background.sprites[1].pending_interrupt, 2);
    CHECK(slow->velocity.x == 3.0f * speed && slow->velocity.z == speed);
    CHECK_EQ_INT(slow->sprites.flying.sprite, M_SLOW_SPRITE);
    CHECK_EQ_INT(slow->sprites.flying.base_sprite, M_SLOW_SPRITE - 4);
    CHECK(fading->velocity.x == speed);
    CHECK_EQ_INT(fading->sprites.flying.sprite, sprite);
    CHECK(freed->velocity.x == 1.0f);
    M_Run(ECL_FUNC_END_SLOW_DOWN, 1);
    CHECK(g_GameSpeed == 1.0f);
    CHECK_EQ_INT(g_Stage.spell_background.sprites[0].pending_interrupt, 1);
    CHECK_EQ_INT(g_Stage.spell_background.sprites[1].pending_interrupt, 1);
    const float back = 1.0f / speed;
    CHECK(slow->velocity.x == 3.0f * speed * back);
    CHECK_EQ_INT(slow->sprites.flying.sprite, M_SLOW_SPRITE - 4);
    CHECK(fading->velocity.x == speed * back);
    M_TearDown();
}

// Checks that bursts fired count bullets from source in RNG order, each
// with the colours of the three small types and the turn, as pattern
// functions 12 and 21 do.
static void M_CheckBurst(
    RNG *const expected, const int32_t first, const VEC3 source,
    const int32_t count, const int16_t colors[3], const bool alternate,
    const int32_t arg)
{
    static const int16_t types[3] = { 0, 3, 7 };
    for (int32_t j = 0; j < count; j++) {
        const BULLET *const fired = M_Fired(first + j);
        const float x = source.x + (Rng_F32(expected) * 32.0f - 16.0f);
        const float y = source.y + (Rng_F32(expected) * 32.0f - 16.0f);
        const uint16_t pick = Rng_U16InRange(expected, 3);
        const float angle = arg == 0
            ? Rng_F32(expected) * 4.712389f - MATH_HALF_PI
            : Angle_AddNormalize(Rng_F32(expected) * 4.712389f, 0.7853982f);
        const float turn = Rng_F32(expected) * 0.008f + 0.01f;
        const bool spawn = alternate && (j & 1) != 0;
        CHECK_EQ_INT(fired->color, colors[pick]);
        CHECK_EQ_INT(
            fired->sprites.flying.base_sprite,
            g_Bullets.types[types[pick]].flying.base_sprite);
        CHECK(fired->angle == Angle_AddNormalize(angle, 0.0f));
        CHECK(fired->speed == 0.1f);
        CHECK(M_CommandIs(
            &fired->commands[0], BULLET_FLAG_CURVE, 100, turn, 0.0f));
        CHECK_EQ_INT(
            fired->flags,
            BULLET_FLAG_CURVE | (spawn ? BULLET_FLAG_SPAWN_1 : 0));
        if (!spawn) {
            CHECK(fired->pos.x == x && fired->pos.y == y);
        }
    }
}

TEST(burst_tall_bullets_bursts_those_near_the_enemy)
{
    static const int16_t colors[3] = { 2, 2, 1 };
    M_SetUp("burst");
    g_GameManager.run.difficulty = 0;
    m_Enemy.move.pos = (VEC3) { 0.0f, 100.0f, 0.0f };
    const VEC3 pos = { 50.0f, 163.0f, 0.0f };
    BULLET *const near = M_AddTall(0, 0, pos);
    near->state = BULLET_FADING;
    BULLET *const far = M_AddTall(1, 0, (VEC3) { 0.0f, 165.0f, 0.0f });
    BULLET *const edge = M_AddBullet(2, 0, (VEC3) { 0.0f, 100.0f, 0.0f }, 1.0f);
    edge->sprites.flying.sprite_data = &m_BurstEdgeSprite;
    RNG expected = g_Rng;
    M_Run(ECL_FUNC_BURST_TALL_BULLETS, 0);
    CHECK(M_ScreenIs(0, M_FLASH, 8, 1, COLOR_ARGB(0x50, 0xcf, 0xcf, 0xff)));
    CHECK_EQ_INT(near->state, BULLET_FREE);
    CHECK_EQ_INT(far->state, BULLET_FLYING);
    CHECK_EQ_INT(edge->state, BULLET_FLYING);
    CHECK_EQ_INT(M_FiredCount(), 10);
    M_CheckBurst(&expected, 0, pos, 10, colors, true, 0);
    CHECK_EQ_INT(g_Rng.seed, expected.seed);
    M_TearDown();
}

TEST(burst_tall_bullets_counts_and_reach_by_difficulty)
{
    static const int16_t colors[3] = { 2, 2, 1 };
    static const int32_t counts[4] = { 10, 18, 22, 25 };
    M_SetUp("burst_difficulty");
    m_Enemy.move.pos = (VEC3) { 0.0f, 100.0f, 0.0f };
    for (int32_t difficulty = 0; difficulty < 4; difficulty++) {
        memset(g_Bullets.bullets, 0, sizeof(g_Bullets.bullets));
        g_Bullets.next_bullet = M_FIRED;
        g_GameManager.run.difficulty = difficulty;
        const VEC3 pos = { 0.0f, 147.0f, 0.0f };
        M_AddTall(0, 0, pos);
        BULLET *const far = M_AddTall(1, 0, (VEC3) { 0.0f, 150.0f, 0.0f });
        RNG expected = g_Rng;
        M_Run(ECL_FUNC_BURST_TALL_BULLETS, 1);
        CHECK_EQ_INT(
            M_FiredCount(), counts[difficulty] * (difficulty < 2 ? 2 : 1));
        CHECK_EQ_INT(far->state, difficulty < 2 ? BULLET_FREE : BULLET_FLYING);
        M_CheckBurst(&expected, 0, pos, counts[difficulty], colors, true, 1);
    }
    M_TearDown();
}

TEST(turn_bullets_below_curves_the_bullets_in_a_column)
{
    M_SetUp("turn_below");
    m_Enemy.move.pos = (VEC3) { 100.0f, 100.0f, 0.0f };
    BULLET *const even =
        M_AddBullet(0, 0, (VEC3) { 85.0f, 351.0f, 0.0f }, 2.0f);
    BULLET *const odd =
        M_AddBullet(1, 0, (VEC3) { 115.0f, 101.0f, 0.0f }, 2.0f);
    BULLET *const above =
        M_AddBullet(2, 0, (VEC3) { 100.0f, 100.0f, 0.0f }, 2.0f);
    BULLET *const low =
        M_AddBullet(3, 0, (VEC3) { 100.0f, 352.0f, 0.0f }, 2.0f);
    BULLET *const side =
        M_AddBullet(4, 0, (VEC3) { 116.0f, 200.0f, 0.0f }, 2.0f);
    BULLET *const turned =
        M_AddBullet(5, 0, (VEC3) { 100.0f, 200.0f, 0.0f }, 2.0f);
    turned->pattern_state = 2;
    M_Run(ECL_FUNC_TURN_BULLETS_BELOW, 0);
    CHECK(M_CommandIs(
        &even->commands[0], BULLET_FLAG_CURVE, 160, -2.0f / 180.0f,
        -0.05235988f));
    CHECK(M_CommandIs(
        &odd->commands[0], BULLET_FLAG_CURVE, 160, -2.0f / 180.0f,
        0.05235988f));
    CHECK_EQ_INT(even->pattern_state, 1);
    CHECK_EQ_INT(above->pattern_state, 0);
    CHECK_EQ_INT(low->pattern_state, 0);
    CHECK_EQ_INT(side->pattern_state, 0);
    CHECK_EQ_INT(turned->commands[0].flag, 0);
    M_TearDown();
}

TEST(aim_turned_bullets_accelerates_them_at_the_player)
{
    M_SetUp("aim_turned");
    m_AimAngle = 0.75f;
    BULLET *const bullet =
        M_AddBullet(0, 0, (VEC3) { 10.0f, 20.0f, 0.0f }, 2.0f);
    bullet->pattern_state = 1;
    bullet->commands[1].flag = BULLET_FLAG_TURN;
    BULLET *const other =
        M_AddBullet(1, 0, (VEC3) { 10.0f, 20.0f, 0.0f }, 2.0f);
    M_Run(ECL_FUNC_AIM_TURNED_BULLETS, 0);
    CHECK(M_ScreenIs(0, M_FLASH, 16, 1, COLOR_ARGB(0x50, 0xcf, 0xcf, 0xff)));
    CHECK(M_CommandIs(
        &bullet->commands[0], BULLET_FLAG_ACCELERATE, 90, 0.026666667f, 0.75f));
    CHECK(m_AngleFrom.x == 10.0f && m_AngleFrom.y == 20.0f);
    CHECK_EQ_INT(bullet->commands[1].flag, 0);
    CHECK_EQ_INT(bullet->pattern_state, 2);
    CHECK_EQ_INT(other->commands[0].flag, 0);
    M_TearDown();
}

TEST(flash_flashes_for_arg_frames)
{
    M_SetUp("flash");
    M_Run(ECL_FUNC_FLASH, 33);
    CHECK_EQ_INT(m_ScreenCount, 1);
    CHECK(M_ScreenIs(0, M_FLASH, 33, 1, COLOR_ARGB(0xd0, 0xcf, 0xcf, 0xff)));
    M_TearDown();
}

TEST(fire_back_from_unturned_fires_a_fan_from_each)
{
    M_SetUp("fire_back");
    m_Enemy.script.state.vars.floats[1] = 1.5f;
    BULLET *const bullet =
        M_AddBullet(0, 0, (VEC3) { 10.0f, 20.0f, 0.0f }, 2.0f);
    bullet->sprites.flying.sprite = 0x278;
    bullet->angle = 0.5f;
    BULLET *const turned = M_AddBullet(1, 0, (VEC3) {}, 2.0f);
    turned->sprites.flying.sprite = 0x27f;
    turned->pattern_state = 1;
    BULLET *const other = M_AddBullet(2, 0, (VEC3) {}, 2.0f);
    other->sprites.flying.sprite = 0x280;
    M_Run(ECL_FUNC_FIRE_BACK_FROM_UNTURNED, 0);
    CHECK_EQ_INT(M_FiredCount(), 5);
    CHECK_EQ_INT(bullet->state, BULLET_FLYING);
    CHECK_EQ_INT(M_Fired(0)->color, 6);
    CHECK(M_Fired(0)->speed == 1.5f);
    CHECK_EQ_INT(M_Fired(0)->flags, BULLET_FLAG_SPAWN_1);
    CHECK(
        M_Fired(0)->angle
        == Angle_AddNormalize(Angle_AddNormalize(0.5f, MATH_PI), 0.0f));
    M_TearDown();
}

TEST(bullets_to_enemies_spawns_the_next_subroutine_on_each)
{
    M_SetUp("to_enemies");
    m_Enemy.script.state.vars.ints[3] = 9;
    BULLET *const first =
        M_AddBullet(0, 0, (VEC3) { 10.0f, 20.0f, 0.0f }, 2.0f);
    first->sprites.flying.sprite = M_UNTURNED_SPRITE;
    first->angle = 0.5f;
    BULLET *const second =
        M_AddBullet(1, 0, (VEC3) { 30.0f, 40.0f, 0.0f }, 2.0f);
    second->sprites.flying.sprite = M_UNTURNED_SPRITE;
    second->angle = 0.25f;
    BULLET *const turned = M_AddBullet(2, 0, (VEC3) {}, 2.0f);
    turned->sprites.flying.sprite = M_UNTURNED_SPRITE;
    turned->pattern_state = 1;
    BULLET *const near = M_AddBullet(3, 0, (VEC3) {}, 2.0f);
    near->sprites.flying.sprite = 0x278;
    BULLET *const other = M_AddBullet(4, 0, (VEC3) {}, 2.0f);
    other->sprites.flying.sprite = 0x277;
    M_Run(ECL_FUNC_BULLETS_TO_ENEMIES, 0);
    CHECK(M_ScreenIs(0, M_FLASH, 12, 1, COLOR_ARGB(0x80, 0xcf, 0xcf, 0xff)));
    CHECK_EQ_INT(m_SpawnCount, 2);
    CHECK_EQ_INT(m_Spawns[0].sub, 6);
    CHECK(m_Spawns[0].vars.floats[0] == 0.5f);
    CHECK(m_Spawns[0].vars.floats[7] == -MATH_PI);
    CHECK_EQ_INT(m_Spawns[0].vars.ints[3], 9);
    CHECK(m_Spawns[0].pos.x == 10.0f && m_Spawns[0].pos.y == 20.0f);
    CHECK(m_Spawns[1].vars.floats[0] == 0.25f);
    CHECK(m_Spawns[1].vars.floats[7] == -MATH_PI + 0.7853982f);
    const ENEMY *const spawned = &g_Enemies.enemies[0];
    CHECK(spawned->flags.active);
    CHECK_EQ_INT(spawned->life, 1);
    CHECK_EQ_INT(spawned->score, 10);
    CHECK_EQ_INT(spawned->death.item, -2);
    CHECK_EQ_INT(first->state, BULLET_FREE);
    CHECK_EQ_INT(turned->state, BULLET_FREE);
    CHECK_EQ_INT(near->state, BULLET_FREE);
    CHECK_EQ_INT(other->state, BULLET_FLYING);
    M_TearDown();
}

TEST(count_unturned_counts_into_ints_0)
{
    M_SetUp("count");
    m_Enemy.script.state.vars.ints[0] = 50;
    for (int32_t i = 0; i < 3; i++) {
        M_AddBullet(i, 0, (VEC3) {}, 1.0f)->sprites.flying.sprite =
            M_UNTURNED_SPRITE;
    }
    g_Bullets.bullets[1].pattern_state = 1;
    M_AddBullet(3, 0, (VEC3) {}, 1.0f)->sprites.flying.sprite = 0x27b;
    M_Run(ECL_FUNC_COUNT_UNTURNED, 0);
    CHECK_EQ_INT(m_Enemy.script.state.vars.ints[0], 2);
    M_TearDown();
}

TEST(music_functions_fade_out_and_play_song_2)
{
    M_SetUp("music");
    M_Run(ECL_FUNC_FADE_OUT_MUSIC, 0);
    CHECK(g_SoundEvents.fade_out);
    CHECK_EQ_INT(g_SoundEvents.fade_seconds, 3);
    Sound_Reset();
    char path[] = "bgm/th07_02.mid";
    char stage_path[] = "bgm/th07_01.mid";
    g_Stage.data.song_paths[1] = stage_path;
    g_Stage.data.song_paths[2] = path;
    M_Run(ECL_FUNC_PLAY_BOSS_MUSIC, 0);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th07_02.wav");
    memset(g_Stage.data.song_paths, 0, sizeof(g_Stage.data.song_paths));
    M_TearDown();
}

TEST(burst_tall_bullets_fixed_fires_15_by_reach)
{
    static const int16_t colors[3] = { 4, 4, 2 };
    M_SetUp("burst_fixed");
    m_Enemy.move.pos = (VEC3) { 0.0f, 100.0f, 0.0f };
    const VEC3 pos = { 0.0f, 279.0f, 0.0f };
    BULLET *const near = M_AddTall(0, 0, pos);
    RNG expected = g_Rng;
    M_Run(ECL_FUNC_BURST_TALL_BULLETS_FIXED, 0);
    CHECK(M_ScreenIs(0, M_FLASH, 8, 1, COLOR_ARGB(0x50, 0xcf, 0xcf, 0xff)));
    CHECK_EQ_INT(near->state, BULLET_FREE);
    CHECK_EQ_INT(M_FiredCount(), 15);
    M_CheckBurst(&expected, 0, pos, 15, colors, false, 0);
    memset(g_Bullets.bullets, 0, sizeof(g_Bullets.bullets));
    g_Bullets.next_bullet = M_FIRED;
    g_GameManager.run.difficulty = 2;
    BULLET *const far = M_AddTall(0, 0, (VEC3) { 0.0f, 228.0f, 0.0f });
    M_Run(ECL_FUNC_BURST_TALL_BULLETS_FIXED, 0);
    CHECK_EQ_INT(far->state, BULLET_FLYING);
    M_TearDown();
}

// Puts the bullets that fire from tall bullets picks from in slots 0 to 4:
// one of color in reach, one that turns, one at y 320, one 60 high and one
// of color 0 in reach.
static void M_AddTallSources(const int16_t color)
{
    M_AddTall(0, color, (VEC3) { 10.0f, 319.0f, 0.0f });
    M_AddTall(1, color, (VEC3) {})->running = BULLET_FLAG_TURN;
    M_AddTall(2, color, (VEC3) { 0.0f, 320.0f, 0.0f });
    M_AddBullet(3, color, (VEC3) {}, 1.0f)->sprites.flying.sprite_data =
        &m_EdgeSprite;
    M_AddTall(4, 0, (VEC3) { 20.0f, 0.0f, 0.0f });
}

TEST(fire_from_tall_bullets_skips_every_third_frame)
{
    M_SetUp("fire_tall");
    M_AddTallSources(1);
    m_Enemy.callbacks.timer.current = 3;
    M_Run(ECL_FUNC_FIRE_FROM_TALL_BULLETS, 0);
    CHECK_EQ_INT(M_FiredCount(), 0);
    RNG expected = g_Rng;
    m_Enemy.callbacks.timer.current = 4;
    M_Run(ECL_FUNC_FIRE_FROM_TALL_BULLETS, 0);
    CHECK_EQ_INT(M_FiredCount(), 4);
    const float angle = Rng_F32(&expected) * MATH_TWO_PI - MATH_PI;
    CHECK(M_Fired(0)->angle == Angle_AddNormalize(angle, 0.0f));
    CHECK_EQ_INT(M_Fired(0)->color, 6);
    CHECK_EQ_INT(M_Fired(2)->color, 2);
    CHECK(M_Fired(0)->speed == 0.8f);
    CHECK_EQ_INT(M_Fired(0)->sound, 0x19);
    CHECK_EQ_INT(M_Fired(0)->flags, BULLET_FLAG_SOUND | BULLET_FLAG_SPAWN_3);
    CHECK_EQ_INT(M_Fired(0)->commands[0].flag, 0);
    CHECK_EQ_INT(g_SoundEvents.queue[0], 0);
    m_Enemy.callbacks.timer.current = 5;
    M_Run(ECL_FUNC_FIRE_FROM_TALL_BULLETS, 0);
    CHECK_EQ_INT(M_FiredCount(), 6);
    const BULLET *const turning = M_Fired(4);
    CHECK(turning->speed == 1.2f);
    CHECK_EQ_INT(turning->commands[0].count, 1);
    CHECK(M_CommandIs(
        &turning->commands[0], BULLET_FLAG_TURN_AT_PLAYER, 60, 0.0f, 3.1f));
    M_TearDown();
}

TEST(fire_from_tall_bullets_2_skips_the_frames_after_every_third)
{
    M_SetUp("fire_tall_2");
    M_AddTallSources(2);
    m_Enemy.callbacks.timer.current = 2;
    M_Run(ECL_FUNC_FIRE_FROM_TALL_BULLETS_2, 0);
    CHECK_EQ_INT(M_FiredCount(), 0);
    m_Enemy.callbacks.timer.current = 3;
    M_Run(ECL_FUNC_FIRE_FROM_TALL_BULLETS_2, 0);
    CHECK_EQ_INT(M_FiredCount(), 2);
    CHECK_EQ_INT(M_Fired(0)->color, 10);
    CHECK_EQ_INT(M_Fired(1)->color, 13);
    CHECK(M_Fired(0)->speed == 0.8f);
    CHECK_EQ_INT(M_Fired(0)->commands[0].flag, 0);
    m_Enemy.callbacks.timer.current = 4;
    M_Run(ECL_FUNC_FIRE_FROM_TALL_BULLETS_2, 0);
    CHECK_EQ_INT(M_FiredCount(), 4);
    CHECK(M_Fired(2)->speed == 1.2f);
    CHECK(M_CommandIs(
        &M_Fired(2)->commands[0], BULLET_FLAG_TURN_AT_PLAYER, 40, 0.0f, 2.9f));
    M_TearDown();
}
