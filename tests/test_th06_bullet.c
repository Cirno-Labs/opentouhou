#include "th06/bullet.h"

#include "anm_builder.h"
#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include "th06/angle.h"
#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/chain.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"
#include "th06/scene.h"
#include "th06/sound.h"

#include "core/base/memory.h"
#include "core/gfx/private.h"
#include "core/base/utils.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#include <SDL3/SDL.h>

#define M_PI 3.1415927f
#define M_TWO_PI 6.2831855f
#define M_SPRITE 0x200
#define M_SPRITE_8 (M_SPRITE + 0)
#define M_SPRITE_16 (M_SPRITE + 16)
#define M_SPRITE_32 (M_SPRITE + 32)
#define M_SPRITE_ORIGIN (M_SPRITE + 0x8c)
#define M_SPRITE_64 0x29a
#define M_MAX_HITS 16

// The test links these instead of the modules they stand for. The
// player is at m_AimAngle from everything, and the contacts it reports
// are m_Graze and m_Hit. Items are never collected, and each test of the
// graze box records where an item was.

typedef struct {
    VEC3 pos, size, origin;
    float angle;
    bool can_graze;
} M_LASER_HIT;

PLAYER g_Player;

static char *m_Directory;
static float m_AimAngle;
static int32_t m_AngleCalls;
static VEC3 m_AngleFrom;
static PLAYER_CONTACT m_Graze;
static PLAYER_CONTACT m_Hit;
static int32_t m_GrazeCalls;
static int32_t m_HitCalls;
static M_LASER_HIT m_LaserHits[M_MAX_HITS];
static int32_t m_LaserHitCount;
static VEC3 m_Items[ITEM_COUNT];
static int32_t m_ItemCount;
static int32_t m_BonusPopups;
static int32_t m_BonusPopup;

float Player_AngleFrom(const VEC3 pos)
{
    m_AngleCalls++;
    m_AngleFrom = pos;
    return m_AimAngle;
}

PLAYER_CONTACT Player_CheckGraze(const VEC3 pos, const VEC3 size)
{
    m_GrazeCalls++;
    return m_Graze;
}

PLAYER_CONTACT Player_CheckHit(const VEC3 pos, const VEC3 size)
{
    m_HitCalls++;
    return m_Hit;
}

void Player_CheckLaserHit(
    const VEC3 pos, const VEC3 size, const VEC3 origin, const float angle,
    const bool can_graze)
{
    if (m_LaserHitCount < M_MAX_HITS) {
        m_LaserHits[m_LaserHitCount] =
            (M_LASER_HIT) { pos, size, origin, angle, can_graze };
    }
    m_LaserHitCount++;
}

bool Player_TouchesGrazeBox(const VEC3 pos, const VEC3 size)
{
    if (m_ItemCount < ITEM_COUNT) {
        m_Items[m_ItemCount] = pos;
    }
    m_ItemCount++;
    return false;
}

void Gui_ShowBonusPopup(const int32_t bonus)
{
    m_BonusPopups++;
    m_BonusPopup = bonus;
}

void Gui_ShowFullPowerPopup(void)
{
}

typedef struct {
    int32_t id;
    int32_t sprite;
    int16_t end;
} M_SCRIPT;

static void M_WriteAnm(
    const char *const name, const int32_t first_sprite,
    const M_SCRIPT *const scripts, const int32_t count, const int32_t size)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder, 256, 256);
    if (first_sprite == M_SPRITE) {
        for (int32_t i = 0; i < 16; i++) {
            AnmBuilder_AddSprite(&builder, i, 0.0f, 0.0f, 8.0f, 8.0f);
            AnmBuilder_AddSprite(&builder, 16 + i, 0.0f, 0.0f, 16.0f, 16.0f);
            AnmBuilder_AddSprite(&builder, 32 + i, 0.0f, 0.0f, 32.0f, 32.0f);
        }
        for (int32_t i = 0; i < 5; i++) {
            AnmBuilder_AddSprite(&builder, 0x8c + i, 0.0f, 0.0f, 16.0f, 16.0f);
        }
    } else {
        for (int32_t i = 0; i < 16; i++) {
            AnmBuilder_AddSprite(
                &builder, i, 0.0f, 0.0f, (float)size, (float)size);
        }
    }
    for (int32_t i = 0; i < count; i++) {
        const int32_t script = AnmBuilder_BeginScript(&builder, scripts[i].id);
        if (scripts[i].sprite >= 0) {
            const uint32_t sprite = (uint32_t)scripts[i].sprite;
            AnmBuilder_Instr(&builder, script, 0, 1, &sprite, 1);
        }
        AnmBuilder_Instr(&builder, script, scripts[i].end, 0, nullptr, 0);
    }
    uint8_t bytes[ANM_BUILDER_MAX_SIZE];
    const size_t bytes_size = AnmBuilder_Build(&builder, bytes);
    Th06Data_WriteFile(m_Directory, name, bytes, bytes_size);
}

// Writes etama3.anm: sprites 8, 16 and 32 high from 0, 16 and 32, and the
// laser origins from 0x8c. Each type of a group of heights shows the
// first sprite of its group. Spawn scripts end at frames 2 to 4, fade
// scripts at 4, the others at 1000.
static void M_WriteEtama3(void)
{
    static const M_SCRIPT scripts[] = {
        { 0, 0, 1000 },  { 1, 16, 1000 }, { 2, 16, 1000 },  { 3, 16, 1000 },
        { 4, 16, 1000 }, { 5, 16, 1000 }, { 6, 32, 1000 },  { 7, 32, 1000 },
        { 8, 32, 1000 }, { 9, 16, 1000 }, { 10, 32, 1000 }, { 11, 0, 4 },
        { 12, 16, 4 },   { 13, 32, 4 },   { 14, 0, 2 },     { 15, 0, 3 },
        { 16, 0, 4 },    { 17, 16, 2 },   { 18, 16, 3 },    { 19, 16, 4 },
        { 20, 32, 3 },
    };
    M_WriteAnm(
        "data/etama3.anm", M_SPRITE, scripts, (int32_t)ARRAY_SIZE(scripts), 0);
}

static void M_WriteEtama4(const int32_t flying_sprite)
{
    const M_SCRIPT scripts[] = {
        { 0, flying_sprite, 1000 },
        { 1, 0, 4 },
        { 2, 0, 3 },
    };
    M_WriteAnm(
        "data/etama4.anm", M_SPRITE_64, scripts, (int32_t)ARRAY_SIZE(scripts),
        64);
}

static void M_Prepare(const char *const case_name, const int32_t flying_sprite)
{
    m_Directory = TestDir_Prepare(case_name);
    Th06Data_WriteArchives(m_Directory, -1);
    char *const data = TestDir_Join(m_Directory, "data");
    SDL_CreateDirectory(data);
    Mem_Free(data);
    M_WriteEtama3();
    M_WriteEtama4(flying_sprite);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    AsciiText_Initialize();
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    Scene_Request(SCENE_GAMEPLAY);
    g_Player = (PLAYER) {};
    m_AimAngle = 0.0f;
    m_AngleCalls = 0;
    m_Graze = PLAYER_CONTACT_NONE;
    m_Hit = PLAYER_CONTACT_NONE;
    m_GrazeCalls = 0;
    m_HitCalls = 0;
    m_LaserHitCount = 0;
    m_ItemCount = 0;
    m_BonusPopups = 0;
    m_BonusPopup = 0;
}

static void M_Setup(const char *const case_name)
{
    M_Prepare(case_name, 0);
    CHECK(Result_Log(Bullet_Register(), LOG_LEVEL_ERROR));
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

static int32_t M_CountItems(void)
{
    m_ItemCount = 0;
    Item_Update();
    return m_ItemCount;
}

// Returns a shooter of one bullet of type at 100, 100, flying at angle
// with speed.
static BULLET_SHOOTER M_Shooter(
    const int16_t type, const float angle, const float speed)
{
    return (BULLET_SHOOTER) {
        .type = type,
        .pos = { 100.0f, 100.0f, 0.0f },
        .angle1 = angle,
        .speed1 = speed,
        .speed2 = speed,
        .columns = 1,
        .rows = 1,
        .aim = BULLET_AIM_FAN,
    };
}

static BULLET *M_Fire(const BULLET_SHOOTER *const shooter)
{
    BULLET *const bullet = &g_Bullets.bullets[g_Bullets.next_bullet];
    Bullet_FirePattern(shooter);
    return bullet;
}

static VEC3 M_Velocity(const float angle, const float speed)
{
    VEC3 v = {};
    X87_SinCosScale(angle, speed, &v.x, &v.y);
    return v;
}

static int32_t M_CountBullets(void)
{
    int32_t count = 0;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        if (g_Bullets.bullets[i].state != BULLET_FREE) {
            count++;
        }
    }
    return count;
}

// Returns the frames that script runs until it ends, from its start.
static int32_t M_ScriptFrames(const int32_t script)
{
    ANM_VM vm = {};
    Anm_StartScript(&vm, script);
    int32_t frames = 1;
    while (!Anm_Step(&vm)) {
        frames++;
    }
    return frames;
}

static bool M_SameVec(const VEC3 a, const VEC3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

TEST(registering_loads_the_bullet_sprites_and_removing_releases_them)
{
    M_Setup("register");
    CHECK(Anm_GetScript(512) != nullptr);
    CHECK(Anm_GetScript(666) != nullptr);
    Bullet_Remove();
    CHECK(Anm_GetScript(512) == nullptr);
    CHECK(Anm_GetScript(666) == nullptr);
    M_Teardown();
}

TEST(the_next_stage_keeps_the_bullet_sprites)
{
    M_Setup("next_stage");
    Scene_Request(SCENE_NEXT_STAGE);
    Bullet_Remove();
    CHECK(Anm_GetScript(512) != nullptr);
    char *const path = TestDir_Join(m_Directory, "data/etama3.anm");
    SDL_RemovePath(path);
    Mem_Free(path);
    CHECK(Result_Log(Bullet_Register(), LOG_LEVEL_ERROR));
    const BULLET_SHOOTER shooter = M_Shooter(7, 0.0f, 1.0f);
    CHECK(M_Fire(&shooter)->sprites.hitbox_size.x == 11.0f);
    Scene_Request(SCENE_GAMEPLAY);
    Bullet_Remove();
    CHECK(Anm_GetScript(512) == nullptr);
    CHECK(!Result_Log(Bullet_Register(), LOG_LEVEL_DEBUG));
    M_Teardown();
}

TEST(a_bullet_script_without_a_sprite_fails_the_register)
{
    M_Prepare("no_sprite", -1);
    CHECK(!Result_Log(Bullet_Register(), LOG_LEVEL_DEBUG));
    M_Teardown();
}

TEST(registering_removes_every_item_and_bullet)
{
    M_Setup("register_reset");
    const BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 1.0f);
    M_Fire(&shooter);
    Item_Spawn((VEC3) { 10.0f, 10.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    Bullet_Remove();
    CHECK(Result_Log(Bullet_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_CountItems(), 0);
    CHECK_EQ_INT(M_CountBullets(), 0);
    CHECK_EQ_INT(g_Bullets.next_bullet, 0);
    M_Teardown();
}

TEST(the_hitbox_follows_the_sprite_height_and_the_script)
{
    M_Setup("hitbox");
    static const float sizes[BULLET_TYPE_COUNT] = {
        4.0f, 6.0f, 4.0f, 6.0f, 5.0f, 4.0f, 16.0f, 11.0f, 9.0f, 32.0f,
    };
    for (int16_t type = 0; type < BULLET_TYPE_COUNT; type++) {
        const BULLET_SHOOTER shooter = M_Shooter(type, 0.0f, 1.0f);
        const BULLET *const bullet = M_Fire(&shooter);
        CHECK(bullet->sprites.hitbox_size.x == sizes[type]);
        CHECK(bullet->sprites.hitbox_size.y == sizes[type]);
        CHECK(bullet->sprites.hitbox_size.z == 0.0f);
    }
    M_Teardown();
}

// Fires fans of 3 and 8 columns in both fan modes from angle1. Adding
// nothing keeps the last bits of the spread itself in the angles.
static void M_CheckFans(const float angle1)
{
    static const int16_t column_counts[] = { 3, 8 };
    for (int32_t i = 0; i < 2; i++) {
        const int16_t columns = column_counts[i];
        BULLET_SHOOTER shooter = M_Shooter(1, angle1, 4.0f);
        shooter.angle2 = 0.025f;
        shooter.speed2 = 0.41f;
        shooter.columns = columns;
        shooter.rows = 3;
        for (int32_t aim = BULLET_AIM_FAN_AT_PLAYER; aim <= BULLET_AIM_FAN;
             aim++) {
            shooter.aim = (BULLET_AIM)aim;
            const int32_t first = g_Bullets.next_bullet;
            m_AngleCalls = 0;
            Bullet_FirePattern(&shooter);
            CHECK_EQ_INT(m_AngleCalls, 1);
            CHECK(M_SameVec(m_AngleFrom, shooter.pos));
            for (int32_t row = 0; row < 3; row++) {
                const float speed = 4.0f - (4.0f - 0.41f) * (float)row / 3.0f;
                for (int32_t col = 0; col < columns; col++) {
                    float angle = 0.0f;
                    if ((columns & 1) != 0) {
                        angle += (float)((col + 1) / 2) * 0.025f;
                    } else {
                        angle += (float)(col / 2) * 0.025f + 0.025f * 0.5f;
                    }
                    if ((col & 1) != 0) {
                        angle *= -1.0f;
                    }
                    if (aim == BULLET_AIM_FAN_AT_PLAYER) {
                        angle += m_AimAngle;
                    }
                    angle += angle1;
                    const BULLET *const bullet =
                        &g_Bullets.bullets[first + row * columns + col];
                    CHECK(bullet->angle == angle);
                    CHECK(bullet->speed == speed);
                    CHECK(
                        M_SameVec(bullet->velocity, M_Velocity(angle, speed)));
                    CHECK(bullet->pos.x == 100.0f && bullet->pos.z == 0.1f);
                }
            }
        }
    }
}

TEST(a_fan_spreads_its_columns_around_angle1)
{
    M_Setup("fan");
    M_CheckFans(0.25f);
    m_AimAngle = 0.0f;
    M_CheckFans(0.0f);
    m_AimAngle = 0.3f;
    M_CheckFans(0.0f);
    M_Teardown();
}

TEST(a_ring_spaces_its_columns_and_turns_each_row)
{
    M_Setup("ring");
    m_AimAngle = 0.3f;
    for (int32_t aim = BULLET_AIM_RING_AT_PLAYER; aim <= BULLET_AIM_RING_OFFSET;
         aim++) {
        BULLET_SHOOTER shooter = M_Shooter(1, 0.25f, 2.0f);
        shooter.angle2 = 0.1f;
        shooter.columns = 5;
        shooter.rows = 2;
        shooter.aim = (BULLET_AIM)aim;
        const int32_t first = g_Bullets.next_bullet;
        Bullet_FirePattern(&shooter);
        for (int32_t row = 0; row < 2; row++) {
            for (int32_t col = 0; col < 5; col++) {
                float angle = 0.0f;
                if (aim == BULLET_AIM_RING_AT_PLAYER
                    || aim == BULLET_AIM_RING_OFFSET_AT_PLAYER) {
                    angle += 0.3f;
                }
                if (aim <= BULLET_AIM_RING) {
                    angle += (float)col * M_TWO_PI / 5.0f;
                    angle += (float)row * 0.1f + 0.25f;
                } else {
                    angle += M_PI / 5.0f;
                    angle += (float)col * M_TWO_PI / 5.0f;
                    angle += 0.25f;
                }
                const BULLET *const bullet =
                    &g_Bullets.bullets[first + row * 5 + col];
                CHECK(bullet->angle == Angle_AddNormalize(angle, 0.0f));
            }
        }
    }
    M_Teardown();
}

TEST(the_random_modes_draw_the_angle_before_the_speed)
{
    M_Setup("random");
    for (int32_t aim = BULLET_AIM_RANDOM_ANGLE; aim <= BULLET_AIM_RANDOM;
         aim++) {
        BULLET_SHOOTER shooter = M_Shooter(1, 1.5f, 3.0f);
        shooter.angle2 = -0.5f;
        shooter.speed2 = 1.0f;
        shooter.columns = 3;
        shooter.rows = 2;
        shooter.aim = (BULLET_AIM)aim;
        RNG expected = g_Rng;
        const int32_t first = g_Bullets.next_bullet;
        Bullet_FirePattern(&shooter);
        for (int32_t row = 0; row < 2; row++) {
            for (int32_t col = 0; col < 3; col++) {
                float angle = 0.0f;
                float speed = 3.0f - (3.0f - 1.0f) * (float)row / 2.0f;
                if (aim != BULLET_AIM_RING_RANDOM_SPEED) {
                    angle = Rng_F32(&expected) * (1.5f - -0.5f) + -0.5f;
                }
                if (aim != BULLET_AIM_RANDOM_ANGLE) {
                    speed = Rng_F32(&expected) * (3.0f - 1.0f) + 1.0f;
                }
                if (aim == BULLET_AIM_RING_RANDOM_SPEED) {
                    angle += (float)col * M_TWO_PI / 3.0f;
                    angle += (float)row * -0.5f + 1.5f;
                }
                const BULLET *const bullet =
                    &g_Bullets.bullets[first + row * 3 + col];
                CHECK(bullet->angle == Angle_AddNormalize(angle, 0.0f));
                CHECK(bullet->speed == speed);
            }
        }
        CHECK(g_Rng.seed == expected.seed);
        CHECK_EQ_INT(g_Rng.generated, expected.generated);
    }
    M_Teardown();
}

TEST(a_full_pool_stops_the_pattern)
{
    M_Setup("full");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 0.0f);
    shooter.columns = BULLET_COUNT + 1;
    shooter.aim = BULLET_AIM_RANDOM_ANGLE;
    RNG expected = g_Rng;
    Bullet_FirePattern(&shooter);
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        Rng_F32(&expected);
    }
    CHECK_EQ_INT(g_Rng.generated, expected.generated);
    CHECK_EQ_INT(M_CountBullets(), BULLET_COUNT);
    CHECK_EQ_INT(g_Bullets.next_bullet, 0);
    Bullet_FirePattern(&shooter);
    CHECK_EQ_INT(g_Rng.generated, expected.generated);
    CHECK_EQ_INT(g_Bullets.next_bullet, 0);
    M_Teardown();
}

TEST(the_sound_flag_queues_the_sound_also_from_a_full_pool)
{
    M_Setup("sound");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 0.0f);
    shooter.columns = BULLET_COUNT + 1;
    shooter.flags = BULLET_FLAG_SOUND;
    shooter.sound = SOUND_SHOT_1;
    Sound_Reset();
    Bullet_FirePattern(&shooter);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_SHOT_1);
    Sound_Reset();
    Bullet_FirePattern(&shooter);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_SHOT_1);
    M_Teardown();
}

TEST(a_shooter_without_the_sound_flag_queues_nothing)
{
    M_Setup("no_sound");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 0.0f);
    shooter.sound = SOUND_SHOT_1;
    Sound_Reset();
    Bullet_FirePattern(&shooter);
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    M_Teardown();
}

TEST(a_spawn_takes_the_first_free_bullet_from_next_bullet)
{
    M_Setup("next_free");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 0.0f);
    shooter.columns = BULLET_COUNT;
    Bullet_FirePattern(&shooter);
    g_Bullets.bullets[5].state = BULLET_FREE;
    g_Bullets.bullets[2].state = BULLET_FREE;
    g_Bullets.next_bullet = 4;
    shooter.columns = 1;
    Bullet_FirePattern(&shooter);
    CHECK(g_Bullets.bullets[5].state == BULLET_FLYING);
    CHECK(g_Bullets.bullets[2].state == BULLET_FREE);
    CHECK_EQ_INT(g_Bullets.next_bullet, 6);
    g_Bullets.bullets[BULLET_COUNT - 1].state = BULLET_FLYING;
    g_Bullets.next_bullet = BULLET_COUNT - 1;
    Bullet_FirePattern(&shooter);
    CHECK(g_Bullets.bullets[2].state == BULLET_FLYING);
    CHECK_EQ_INT(g_Bullets.next_bullet, 3);
    M_Teardown();
}

TEST(a_bullet_takes_the_sprites_of_its_color)
{
    M_Setup("color");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    shooter.color = 5;
    shooter.flags = BULLET_FLAG_SPAWN_1;
    const BULLET *bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->sprites.flying.sprite, M_SPRITE_16 + 5);
    CHECK_EQ_INT(bullet->sprites.spawning[0].sprite, M_SPRITE_16 + 2);
    CHECK_EQ_INT(bullet->sprites.fading.sprite, M_SPRITE_16 + 2);
    CHECK_EQ_INT(bullet->color, 5);

    shooter.type = 6;
    shooter.color = 3;
    shooter.flags = BULLET_FLAG_SPAWN_3;
    bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->sprites.flying.sprite, M_SPRITE_32 + 3);
    CHECK_EQ_INT(bullet->sprites.spawning[2].sprite, M_SPRITE_32 + 2);
    CHECK_EQ_INT(bullet->sprites.fading.sprite, M_SPRITE_32 + 2);

    shooter.type = 7;
    bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->sprites.spawning[2].sprite, M_SPRITE_32 + 1);
    CHECK_EQ_INT(bullet->sprites.fading.sprite, M_SPRITE_32 + 1);

    shooter.type = 9;
    bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->sprites.flying.sprite, M_SPRITE_64 + 3);
    CHECK_EQ_INT(bullet->sprites.fading.sprite, M_SPRITE_64 + 3);
    M_Teardown();
}

TEST(the_fade_sprite_takes_the_height_of_the_colored_sprite)
{
    M_Setup("color_height");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    shooter.color = 18;
    shooter.flags = BULLET_FLAG_SPAWN_2;
    const BULLET *const bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->sprites.flying.sprite, M_SPRITE_32 + 2);
    CHECK_EQ_INT(bullet->sprites.spawning[1].sprite, M_SPRITE_16 + 1);
    CHECK_EQ_INT(bullet->sprites.fading.sprite, M_SPRITE_16);
    M_Teardown();
}

TEST(a_color_past_the_offset_tables_keeps_the_sprite)
{
    M_Setup("color_past");
    BULLET_SHOOTER shooter = M_Shooter(6, 0.0f, 1.0f);
    shooter.color = 8;
    const BULLET *const bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->sprites.flying.sprite, M_SPRITE_32 + 8);
    CHECK_EQ_INT(bullet->sprites.fading.sprite, M_SPRITE_32);
    M_Teardown();
}

TEST(a_spawning_bullet_moves_slower_until_its_script_ends)
{
    M_Setup("spawning");
    static const float divisors[] = { 2.0f, 2.5f, 3.0f };
    static const int32_t scripts[] = { 529, 530, 531 };
    for (int32_t i = 0; i < 3; i++) {
        BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.15f);
        shooter.pos.x = 0.0f;
        shooter.flags = BULLET_FLAG_SPAWN_1 << i;
        BULLET *const bullet = M_Fire(&shooter);
        CHECK(bullet->state == (BULLET_STATE)(BULLET_SPAWNING_1 + i));
        const VEC3 v = M_Velocity(0.0f, 0.15f);
        const VEC3 slow = Vec3_Scale(v, 1.0f / divisors[i]);
        VEC3 pos = bullet->pos;
        const int32_t frames = M_ScriptFrames(scripts[i]);
        for (int32_t frame = 1; frame < frames; frame++) {
            M_Frame();
            pos = Vec3_Add(pos, slow);
            CHECK(bullet->state == (BULLET_STATE)(BULLET_SPAWNING_1 + i));
            CHECK(M_SameVec(bullet->pos, pos));
            CHECK_EQ_INT(bullet->timer.current, frame);
        }
        M_Frame();
        pos = Vec3_Add(Vec3_Add(pos, slow), v);
        CHECK(bullet->state == BULLET_FLYING);
        CHECK(M_SameVec(bullet->pos, pos));
        CHECK_EQ_INT(bullet->timer.current, 1);
        CHECK_EQ_INT(m_GrazeCalls, i + 1);
        *bullet = (BULLET) {};
    }
    M_Teardown();
}

TEST(a_fading_bullet_moves_at_half_speed_and_is_freed_with_its_script)
{
    M_Setup("fading");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 3.0f);
    BULLET *const bullet = M_Fire(&shooter);
    Bullet_ClearAll();
    CHECK(bullet->state == BULLET_FADING);
    const VEC3 half = Vec3_Scale(M_Velocity(0.5f, 3.0f), 0.5f);
    VEC3 pos = bullet->pos;
    const int32_t frames = M_ScriptFrames(524);
    for (int32_t frame = 1; frame < frames; frame++) {
        M_Frame();
        pos = Vec3_Add(pos, half);
        CHECK(bullet->state == BULLET_FADING);
        CHECK(M_SameVec(bullet->pos, pos));
    }
    M_Frame();
    CHECK(bullet->state == BULLET_FREE);
    CHECK(bullet->pos.x == 0.0f && bullet->timer.current == 0);
    CHECK_EQ_INT(m_GrazeCalls, 0);
    M_Teardown();
}

TEST(a_flying_bullet_moves_by_its_velocity)
{
    M_Setup("flying");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 3.0f);
    BULLET *const bullet = M_Fire(&shooter);
    const VEC3 v = M_Velocity(0.5f, 3.0f);
    VEC3 pos = bullet->pos;
    for (int32_t frame = 1; frame <= 3; frame++) {
        M_Frame();
        pos = Vec3_Add(pos, v);
        CHECK(M_SameVec(bullet->pos, pos));
        CHECK_EQ_INT(bullet->timer.current, frame);
    }
    M_Teardown();
}

// Returns an angle above pi whose sine and cosine scaled by radius differ
// from those of the angle normalized.
static float M_UnnormalizedAngle(const float radius)
{
    for (float angle = 7.0f; angle < 8.0f; angle += 0.001f) {
        if (!M_SameVec(
                M_Velocity(angle, radius),
                M_Velocity(Angle_AddNormalize(angle, 0.0f), radius))) {
            return angle;
        }
    }
    TEST_FAIL("no angle found");
    return 7.0f;
}

TEST(an_accelerating_bullet_takes_the_angle_before_it_is_normalized)
{
    M_Setup("accel_angle");
    const float angle = M_UnnormalizedAngle(0.25f);
    BULLET_SHOOTER shooter = M_Shooter(1, angle, 1.0f);
    shooter.flags = BULLET_FLAG_ACCELERATE;
    shooter.ex_floats[0] = 0.25f;
    shooter.ex_floats[1] = -999.0f;
    const BULLET *bullet = M_Fire(&shooter);
    CHECK(bullet->angle == Angle_AddNormalize(angle, 0.0f));
    CHECK(M_SameVec(bullet->change.accel, M_Velocity(angle, 0.25f)));
    CHECK_EQ_INT(bullet->change.frames, 99999);

    shooter.ex_floats[1] = 1.0f;
    shooter.ex_ints[0] = 3;
    bullet = M_Fire(&shooter);
    CHECK(M_SameVec(bullet->change.accel, M_Velocity(1.0f, 0.25f)));
    CHECK_EQ_INT(bullet->change.frames, 3);
    M_Teardown();
}

TEST(an_accelerating_bullet_turns_with_its_velocity_for_its_frames)
{
    M_Setup("accel");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    shooter.flags = BULLET_FLAG_ACCELERATE;
    shooter.ex_floats[0] = 0.5f;
    shooter.ex_floats[1] = 1.2f;
    shooter.ex_ints[0] = 3;
    BULLET *const bullet = M_Fire(&shooter);
    VEC3 v = M_Velocity(0.0f, 1.0f);
    const VEC3 accel = M_Velocity(1.2f, 0.5f);
    VEC3 pos = bullet->pos;
    for (int32_t frame = 0; frame < 5; frame++) {
        M_Frame();
        if (frame < 3) {
            v = Vec3_Add(v, accel);
            CHECK(bullet->angle == X87_Atan2(v.y, v.x));
        }
        pos = Vec3_Add(pos, v);
        CHECK(M_SameVec(bullet->pos, pos));
    }
    CHECK_EQ_INT(bullet->flags, 0);
    M_Teardown();
}

TEST(a_curving_bullet_changes_speed_and_angle_for_its_frames)
{
    M_Setup("curve");
    BULLET_SHOOTER shooter = M_Shooter(1, 3.0f, 1.0f);
    shooter.flags = BULLET_FLAG_CURVE;
    shooter.ex_floats[0] = 0.1f;
    shooter.ex_floats[1] = 0.3f;
    shooter.ex_ints[0] = 2;
    BULLET *const bullet = M_Fire(&shooter);
    float angle = 3.0f;
    float speed = 1.0f;
    for (int32_t frame = 0; frame < 4; frame++) {
        M_Frame();
        if (frame < 2) {
            angle = Angle_AddNormalize(angle, 0.3f);
            speed = 0.1f + speed;
        }
        CHECK(bullet->angle == angle);
        CHECK(bullet->speed == speed);
        CHECK(M_SameVec(bullet->velocity, M_Velocity(angle, speed)));
    }
    CHECK(angle < 0.0f);
    CHECK_EQ_INT(bullet->flags, 0);
    M_Teardown();
}

TEST(a_fast_start_slows_to_the_speed_over_sixteen_frames)
{
    M_Setup("start_fast");
    BULLET_SHOOTER shooter = M_Shooter(1, 1.5f, 0.5f);
    shooter.flags = BULLET_FLAG_START_FAST | BULLET_FLAG_CURVE;
    shooter.ex_floats[0] = 1.0f;
    shooter.ex_ints[0] = 100;
    BULLET *const bullet = M_Fire(&shooter);
    VEC3 pos = bullet->pos;
    for (int32_t frame = 0; frame <= 17; frame++) {
        M_Frame();
        if (frame <= 16) {
            const float extra = 5.0f - (float)frame * 5.0f / 16.0f;
            pos = Vec3_Add(pos, M_Velocity(1.5f, extra + 0.5f));
            CHECK(M_SameVec(bullet->pos, pos));
        }
    }
    CHECK_EQ_INT(bullet->flags, BULLET_FLAG_CURVE);
    CHECK(bullet->speed == 0.5f);
    M_Frame();
    CHECK(bullet->speed == 1.5f);
    M_Teardown();
}

TEST(a_turning_bullet_slows_between_turns_and_adds_the_turn_angle)
{
    M_Setup("turn");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 1.3f);
    shooter.flags = BULLET_FLAG_TURN;
    shooter.ex_floats[0] = 4.0f;
    shooter.ex_floats[1] = -1.0f;
    shooter.ex_ints[0] = 3;
    shooter.ex_ints[1] = 2;
    BULLET *const bullet = M_Fire(&shooter);
    CHECK(bullet->turn.speed == 1.3f);
    float angle = 0.5f;
    for (int32_t frame = 0; frame < 8; frame++) {
        M_Frame();
        float speed;
        if (frame == 3 || frame == 6) {
            angle += 4.0f;
            speed = 1.3f;
        } else {
            const int32_t count = frame < 3 ? 0 : 1;
            speed = 1.3f - ((float)frame - (float)(3 * count)) * 1.3f / 3.0f;
        }
        if (frame <= 6) {
            CHECK(bullet->angle == angle);
            CHECK(M_SameVec(bullet->velocity, M_Velocity(angle, speed)));
        }
    }
    CHECK(bullet->angle == angle);
    CHECK(bullet->angle > M_TWO_PI);
    CHECK_EQ_INT(bullet->turn.count, 2);
    CHECK_EQ_INT(bullet->flags, 0);
    M_Teardown();
}

TEST(a_turn_takes_the_turn_speed)
{
    M_Setup("turn_speed");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 0.75f);
    shooter.flags = BULLET_FLAG_TURN_TO;
    shooter.ex_floats[0] = 2.0f;
    shooter.ex_floats[1] = 1.25f;
    shooter.ex_ints[0] = 1;
    shooter.ex_ints[1] = 1;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    CHECK(bullet->speed == 0.75f);
    M_Frame();
    CHECK(bullet->angle == 2.0f);
    CHECK(bullet->speed == 1.25f);
    CHECK(M_SameVec(bullet->velocity, M_Velocity(2.0f, 1.25f)));
    CHECK_EQ_INT(bullet->flags, 0);
    M_Teardown();
}

TEST(a_turn_at_the_player_aims_from_the_bullet)
{
    M_Setup("turn_at_player");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 0.75f);
    shooter.flags = BULLET_FLAG_TURN_AT_PLAYER;
    shooter.ex_floats[0] = 0.25f;
    shooter.ex_floats[1] = 1.0f;
    shooter.ex_ints[0] = 2;
    shooter.ex_ints[1] = 3;
    BULLET *const bullet = M_Fire(&shooter);
    m_AimAngle = 3.0f;
    M_Frame();
    M_Frame();
    const VEC3 before = bullet->pos;
    m_AngleCalls = 0;
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 1);
    CHECK(M_SameVec(m_AngleFrom, before));
    CHECK(bullet->angle == 3.0f + 0.25f);
    CHECK_EQ_INT(bullet->flags, BULLET_FLAG_TURN_AT_PLAYER);
    M_Teardown();
}

TEST(the_turns_run_one_at_a_time_in_order)
{
    M_Setup("turn_order");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 0.75f);
    shooter.flags = BULLET_FLAG_TURN_AT_PLAYER | BULLET_FLAG_TURN_TO;
    shooter.ex_floats[0] = 2.0f;
    shooter.ex_ints[0] = 1;
    shooter.ex_ints[1] = 1;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    m_AngleCalls = 0;
    M_Frame();
    CHECK_EQ_INT(m_AngleCalls, 0);
    CHECK(bullet->angle == 2.0f);
    CHECK_EQ_INT(bullet->flags, BULLET_FLAG_TURN_AT_PLAYER);
    M_Teardown();
}

TEST(a_bouncing_bullet_turns_back_off_a_side)
{
    M_Setup("bounce_side");
    BULLET_SHOOTER shooter = M_Shooter(0, 1.603f, 6.0f);
    shooter.pos.x = -4.5f;
    shooter.flags = BULLET_FLAG_BOUNCE;
    shooter.ex_floats[0] = 2.0f;
    shooter.ex_ints[0] = 2;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    const float angle = Angle_AddNormalize(-1.603f - M_PI, 0.0f);
    CHECK(angle != M_PI - 1.603f);
    CHECK(bullet->angle == angle);
    CHECK(bullet->speed == 2.0f);
    CHECK(M_SameVec(bullet->velocity, M_Velocity(angle, 2.0f)));
    CHECK_EQ_INT(bullet->turn.count, 1);
    CHECK_EQ_INT(bullet->flags, BULLET_FLAG_BOUNCE);
    CHECK(bullet->state == BULLET_FLYING);
    CHECK_EQ_INT(bullet->out_of_bounds_frames, 1);
    M_Teardown();
}

TEST(a_bouncing_bullet_turns_back_off_the_bottom)
{
    M_Setup("bounce_bottom");
    BULLET_SHOOTER shooter = M_Shooter(0, 1.0f, 6.0f);
    shooter.pos.y = 460.0f;
    shooter.flags = BULLET_FLAG_BOUNCE;
    shooter.ex_floats[0] = -1.0f;
    shooter.ex_ints[0] = 3;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    CHECK(bullet->angle == -1.0f);
    CHECK(bullet->speed == 6.0f);
    CHECK_EQ_INT(bullet->turn.count, 1);
    CHECK_EQ_INT(bullet->flags, BULLET_FLAG_BOUNCE);
    M_Teardown();
}

TEST(a_bounce_without_the_bottom_still_counts_off_the_bottom)
{
    M_Setup("bounce_no_bottom");
    BULLET_SHOOTER shooter = M_Shooter(0, 1.0f, 6.0f);
    shooter.pos.y = 460.0f;
    shooter.flags = BULLET_FLAG_BOUNCE_NO_BOTTOM;
    shooter.ex_floats[0] = 3.0f;
    shooter.ex_ints[0] = 3;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    CHECK(bullet->angle == 1.0f);
    CHECK(bullet->speed == 3.0f);
    CHECK_EQ_INT(bullet->turn.count, 1);
    M_Teardown();
}

TEST(a_bullet_that_leaves_the_play_area_is_freed)
{
    M_Setup("leave");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 10.0f);
    shooter.pos.x = 380.0f;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    CHECK(bullet->state == BULLET_FREE);
    CHECK(bullet->pos.x == 0.0f);
    CHECK_EQ_INT(m_GrazeCalls, 0);
    M_Teardown();
}

TEST(a_turning_bullet_is_freed_after_0x100_frames_outside)
{
    M_Setup("leave_turning");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 10.0f);
    shooter.pos.x = 380.0f;
    shooter.flags = BULLET_FLAG_TURN;
    shooter.ex_ints[0] = 1000;
    BULLET *const bullet = M_Fire(&shooter);
    for (int32_t frame = 1; frame < 0x100; frame++) {
        M_Frame();
        CHECK(bullet->state == BULLET_FLYING);
        CHECK_EQ_INT(bullet->out_of_bounds_frames, frame);
    }
    M_Frame();
    CHECK(bullet->state == BULLET_FREE);
    M_Teardown();
}

TEST(a_bullet_outside_keeps_counting_after_its_turns_end)
{
    M_Setup("leave_counting");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 10.0f);
    shooter.pos.x = 380.0f;
    shooter.flags = BULLET_FLAG_TURN_TO;
    shooter.ex_ints[0] = 2;
    shooter.ex_ints[1] = 1;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    M_Frame();
    M_Frame();
    CHECK_EQ_INT(bullet->flags, 0);
    CHECK(bullet->state == BULLET_FLYING);
    CHECK_EQ_INT(bullet->out_of_bounds_frames, 3);
    bullet->pos.x = 100.0f;
    bullet->velocity = (VEC3) {};
    M_Frame();
    CHECK_EQ_INT(bullet->out_of_bounds_frames, 0);
    M_Teardown();
}

TEST(a_graze_tests_the_hit_in_the_same_frame)
{
    M_Setup("graze");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.0f);
    BULLET *const bullet = M_Fire(&shooter);
    m_Graze = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK(bullet->grazed);
    CHECK_EQ_INT(m_GrazeCalls, 1);
    CHECK_EQ_INT(m_HitCalls, 1);
    CHECK(bullet->state == BULLET_FLYING);
    M_Frame();
    CHECK_EQ_INT(m_GrazeCalls, 1);
    CHECK_EQ_INT(m_HitCalls, 2);
    m_Hit = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK(bullet->state == BULLET_FADING);
    CHECK_EQ_INT(M_CountItems(), 0);
    M_Teardown();
}

TEST(a_graze_in_a_clear_area_fades_the_bullet_into_a_star)
{
    M_Setup("graze_clear");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.0f);
    BULLET *const bullet = M_Fire(&shooter);
    m_Graze = PLAYER_CONTACT_CLEARED;
    M_Frame();
    CHECK(bullet->state == BULLET_FADING);
    CHECK(!bullet->grazed);
    CHECK_EQ_INT(m_HitCalls, 0);
    CHECK_EQ_INT(M_CountItems(), 1);
    CHECK(m_Items[0].x == 108.0f && m_Items[0].y == 100.0f);
    M_Teardown();
}

TEST(a_hit_in_a_clear_area_fades_the_bullet_into_a_star)
{
    M_Setup("hit_clear");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.0f);
    BULLET *const bullet = M_Fire(&shooter);
    m_Graze = PLAYER_CONTACT_BOX;
    m_Hit = PLAYER_CONTACT_CLEARED;
    M_Frame();
    CHECK(bullet->state == BULLET_FADING);
    CHECK_EQ_INT(M_CountItems(), 1);
    M_Teardown();
}

TEST(the_update_runs_the_items_first)
{
    M_Setup("items");
    Item_Spawn((VEC3) { 10.0f, 10.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    M_Frame();
    CHECK_EQ_INT(m_ItemCount, 1);
    M_Teardown();
}

TEST(a_frozen_game_stops_the_bullets_lasers_and_items)
{
    M_Setup("frozen");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    BULLET *const bullet = M_Fire(&shooter);
    Item_Spawn((VEC3) { 10.0f, 10.0f, 0.0f }, ITEM_POWER, ITEM_FALLING);
    BULLET_SHOOTER laser_shooter = shooter;
    laser_shooter.ex_ints[1] = 10;
    LASER *const laser = Bullet_SpawnLaser(&laser_shooter);
    g_GameManager.mode.frozen = true;
    M_Frame();
    CHECK(bullet->pos.x == 100.0f);
    CHECK_EQ_INT(bullet->timer.current, 0);
    CHECK(laser->end_offset == 0.0f);
    CHECK_EQ_INT(m_ItemCount, 0);
    M_Teardown();
}

static BULLET_SHOOTER M_LaserShooter(void)
{
    BULLET_SHOOTER shooter = M_Shooter(0, 0.5f, 5.0f);
    shooter.color = 5;
    shooter.aim = BULLET_AIM_FAN_AT_PLAYER;
    shooter.flags = 0x10000 | BULLET_FLAG_SPAWN_1;
    shooter.ex_floats[0] = 2.0f;
    shooter.ex_floats[1] = 10.0f;
    shooter.ex_floats[2] = 100.0f;
    shooter.ex_floats[3] = 8.0f;
    shooter.ex_ints[0] = 3;
    shooter.ex_ints[1] = 2;
    shooter.ex_ints[2] = 2;
    shooter.ex_ints[3] = 1;
    shooter.laser_end_hit_frames = 1;
    return shooter;
}

TEST(a_laser_takes_its_values_from_the_shooter)
{
    M_Setup("laser");
    m_AimAngle = 1.0f;
    const BULLET_SHOOTER shooter = M_LaserShooter();
    const LASER *const laser = Bullet_SpawnLaser(&shooter);
    CHECK(laser == &g_Bullets.lasers[0]);
    CHECK(laser->active);
    CHECK(laser->angle == 0.5f + 1.0f);
    CHECK(M_SameVec(m_AngleFrom, shooter.pos));
    CHECK(M_SameVec(laser->pos, shooter.pos));
    CHECK_EQ_INT(laser->flags, BULLET_FLAG_SPAWN_1);
    CHECK(laser->start_offset == 2.0f && laser->end_offset == 10.0f);
    CHECK(laser->max_length == 100.0f && laser->width == 8.0f);
    CHECK(laser->speed == 5.0f);
    CHECK_EQ_INT(laser->start_frames, 3);
    CHECK_EQ_INT(laser->duration_frames, 2);
    CHECK_EQ_INT(laser->end_frames, 2);
    CHECK_EQ_INT(laser->hit_start_frame, 1);
    CHECK_EQ_INT(laser->end_hit_frames, 1);
    CHECK(laser->state == LASER_STARTING);
    CHECK(laser->beam.script == Anm_GetScript(0x209));
    CHECK_EQ_INT(laser->beam.sprite, M_SPRITE_16 + 5);
    CHECK_EQ_INT(laser->origin.sprite, M_SPRITE_ORIGIN + 2);
    CHECK(laser->origin.blend_add);
    M_Teardown();
}

TEST(a_laser_without_start_frames_starts_firing_unaimed)
{
    M_Setup("laser_firing");
    m_AimAngle = 1.0f;
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.aim = BULLET_AIM_FAN;
    shooter.ex_ints[0] = 0;
    m_AngleCalls = 0;
    const LASER *const laser = Bullet_SpawnLaser(&shooter);
    CHECK(laser->state == LASER_FIRING);
    CHECK(laser->angle == 0.5f);
    CHECK_EQ_INT(m_AngleCalls, 0);
    M_Teardown();
}

TEST(a_full_laser_pool_returns_a_spare_that_never_runs)
{
    M_Setup("laser_full");
    const BULLET_SHOOTER shooter = M_LaserShooter();
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        CHECK(Bullet_SpawnLaser(&shooter) == &g_Bullets.lasers[i]);
    }
    LASER *const spare = Bullet_SpawnLaser(&shooter);
    CHECK(spare != nullptr);
    CHECK(!spare->active);
    spare->speed = 5.0f;
    spare->end_offset = 1.0f;
    M_Frame();
    CHECK(spare->end_offset == 1.0f);
    CHECK(Bullet_SpawnLaser(&shooter) == spare);
    M_Teardown();
}

TEST(a_laser_hits_through_its_states)
{
    M_Setup("laser_states");
    const BULLET_SHOOTER shooter = M_LaserShooter();
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    static const int32_t hits[] = { 0, 1, 1, 2, 1, 2, 0, 0 };
    static const LASER_STATE states[] = {
        LASER_STARTING, LASER_STARTING, LASER_STARTING, LASER_FIRING,
        LASER_FIRING,   LASER_ENDING,   LASER_ENDING,   LASER_ENDING,
    };
    for (int32_t frame = 0; frame < 8; frame++) {
        m_LaserHitCount = 0;
        M_Frame();
        CHECK_EQ_INT(m_LaserHitCount, hits[frame]);
        CHECK_EQ_INT(laser->state, states[frame]);
        CHECK(laser->active == (frame < 7));
    }
    M_Teardown();
}

TEST(a_laser_grazes_every_twelve_frames)
{
    M_Setup("laser_graze");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.ex_ints[0] = 0;
    shooter.ex_ints[1] = 100;
    Bullet_SpawnLaser(&shooter);
    for (int32_t frame = 0; frame < 25; frame++) {
        m_LaserHitCount = 0;
        M_Frame();
        CHECK_EQ_INT(m_LaserHitCount, 1);
        CHECK(m_LaserHits[0].can_graze == (frame % 12 == 0));
    }
    M_Teardown();
}

TEST(a_laser_without_end_frames_ends_after_firing)
{
    M_Setup("laser_no_end");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.ex_ints[0] = 0;
    shooter.ex_ints[1] = 1;
    shooter.ex_ints[2] = 0;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    M_Frame();
    CHECK(laser->active);
    m_LaserHitCount = 0;
    M_Frame();
    CHECK(!laser->active);
    CHECK_EQ_INT(laser->state, LASER_ENDING);
    CHECK_EQ_INT(laser->timer.current, 0);
    CHECK_EQ_INT(m_LaserHitCount, 1);
    M_Teardown();
}

TEST(a_starting_laser_hits_with_its_width)
{
    M_Setup("laser_box");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.ex_floats[0] = 0.0f;
    shooter.ex_ints[0] = 40;
    shooter.ex_ints[3] = 0;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    for (int32_t frame = 0; frame < 12; frame++) {
        m_LaserHitCount = 0;
        M_Frame();
        const float end = 10.0f + 5.0f * (float)(frame + 1);
        const float width =
            40 - 30 < frame ? (float)frame * 8.0f / 40.0f : 1.2f;
        const M_LASER_HIT *const hit = &m_LaserHits[0];
        CHECK(hit->size.x == width / 2.0f);
        CHECK(hit->size.y == 8.0f / 2.0f);
        CHECK(hit->pos.x == end / 2.0f + 0.0f + 100.0f);
        CHECK(hit->pos.y == 100.0f);
        CHECK(M_SameVec(hit->origin, shooter.pos));
        CHECK(hit->angle == laser->angle);
        CHECK(laser->beam.scale_x == width / 16.0f);
        CHECK(laser->beam.scale_y == end / 16.0f);
    }
    M_Teardown();
}

TEST(a_firing_laser_hits_with_its_length_and_follows_with_its_start)
{
    M_Setup("laser_length");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.ex_floats[0] = -20.0f;
    shooter.ex_floats[1] = 0.0f;
    shooter.ex_floats[2] = 12.0f;
    shooter.ex_ints[0] = 0;
    shooter.ex_ints[1] = 100;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    M_Frame();
    CHECK(laser->start_offset == 0.0f);
    CHECK(laser->end_offset == 5.0f);
    CHECK(m_LaserHits[0].size.x == 5.0f);
    M_Frame();
    M_Frame();
    CHECK(laser->end_offset == 15.0f);
    CHECK(laser->start_offset == 3.0f);
    m_LaserHitCount = 0;
    M_Frame();
    CHECK(m_LaserHits[0].size.x == 12.0f);
    CHECK(m_LaserHits[0].pos.x == 12.0f / 2.0f + 8.0f + 100.0f);
    M_Teardown();
}

TEST(an_ending_laser_narrows_and_hits_for_its_hit_frames)
{
    M_Setup("laser_ending");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.ex_ints[0] = 0;
    shooter.ex_ints[1] = 0;
    shooter.ex_ints[2] = 4;
    shooter.laser_end_hit_frames = 2;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    for (int32_t frame = 0; frame < 4; frame++) {
        m_LaserHitCount = 0;
        M_Frame();
        const float width = 8.0f - (float)frame * 8.0f / 4.0f;
        CHECK(laser->beam.scale_x == width / 16.0f);
        const int32_t expected = frame == 0 ? 2 : frame < 2 ? 1 : 0;
        CHECK_EQ_INT(m_LaserHitCount, expected);
        if (frame == 1) {
            CHECK(m_LaserHits[0].size.x == width / 2.0f);
        }
    }
    M_Frame();
    CHECK(!laser->active);
    M_Teardown();
}

TEST(a_fading_laser_fades_in_again_as_it_ends)
{
    M_Setup("laser_fade");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.flags = BULLET_FLAG_START_FAST;
    shooter.ex_ints[0] = 10;
    shooter.ex_ints[1] = 0;
    shooter.ex_ints[2] = 20;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    for (int32_t frame = 0; frame < 10; frame++) {
        M_Frame();
        const int32_t alpha = X87_Ftol((float)frame * 255.0f / 10.0f);
        CHECK(laser->beam.color == (uint32_t)alpha << 24);
    }
    M_Frame();
    CHECK_EQ_INT(laser->state, LASER_ENDING);
    CHECK(laser->beam.color == 0);
    for (int32_t frame = 1; frame < 15; frame++) {
        M_Frame();
        int32_t alpha = X87_Ftol((float)frame * 255.0f / 10.0f);
        if (alpha > 0xff) {
            alpha = 0xff;
        }
        CHECK(laser->beam.color == (uint32_t)alpha << 24);
    }
    M_Teardown();
}

TEST(a_laser_ends_once_its_start_reaches_640)
{
    M_Setup("laser_far");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.ex_floats[0] = 639.0f;
    shooter.ex_floats[1] = 650.0f;
    shooter.ex_floats[2] = 10.0f;
    shooter.ex_ints[0] = 0;
    shooter.ex_ints[1] = 100;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    M_Frame();
    CHECK(laser->start_offset == 645.0f);
    CHECK(!laser->active);
    CHECK_EQ_INT(laser->timer.current, 1);
    M_Teardown();
}

// Spawns a laser from 100, 100 at angle 0.5, from 0 to 70, ending.
static LASER *M_ClearableLaser(void)
{
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.aim = BULLET_AIM_FAN;
    shooter.ex_floats[0] = 0.0f;
    shooter.ex_floats[1] = 70.0f;
    shooter.laser_end_hit_frames = 9;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    Timer_Set(&laser->timer, 2);
    return laser;
}

static VEC3 M_LaserPoint(const float t)
{
    float s;
    float c;
    X87_SinCos(0.5f, &s, &c);
    return (VEC3) { c * t + 100.0f + 8.0f, s * t + 100.0f, 0.0f };
}

TEST(clear_all_fades_the_bullets_and_ends_the_lasers)
{
    M_Setup("clear");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    BULLET *const flying = M_Fire(&shooter);
    BULLET *const fading = M_Fire(&shooter);
    fading->state = BULLET_FADING;
    fading->timer.current = 7;
    LASER *const laser = M_ClearableLaser();
    LASER *const ending = M_ClearableLaser();
    ending->state = LASER_ENDING;
    Bullet_ClearAll();
    CHECK(flying->state == BULLET_FADING);
    CHECK(fading->state == BULLET_FADING);
    CHECK_EQ_INT(fading->timer.current, 7);
    CHECK_EQ_INT(laser->state, LASER_ENDING);
    CHECK_EQ_INT(laser->timer.current, 0);
    CHECK_EQ_INT(ending->timer.current, 2);
    CHECK_EQ_INT(laser->end_hit_frames, 0);
    CHECK_EQ_INT(ending->end_hit_frames, 0);
    CHECK_EQ_INT(M_CountItems(), 0);
    M_Teardown();
}

TEST(clear_all_to_items_turns_bullets_and_lasers_into_stars)
{
    M_Setup("clear_items");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    BULLET *const flying = M_Fire(&shooter);
    flying->pos.x = 50.0f;
    BULLET *const fading = M_Fire(&shooter);
    fading->state = BULLET_FADING;
    LASER *const laser = M_ClearableLaser();
    LASER *const ending = M_ClearableLaser();
    ending->state = LASER_ENDING;
    Bullet_ClearAllToItems();
    CHECK(flying->state == BULLET_FREE);
    CHECK(flying->pos.x == 0.0f);
    CHECK(fading->state == BULLET_FADING);
    CHECK_EQ_INT(laser->state, LASER_ENDING);
    CHECK_EQ_INT(laser->end_hit_frames, 0);
    CHECK_EQ_INT(ending->end_hit_frames, 0);
    CHECK_EQ_INT(M_CountItems(), 4);
    CHECK(m_Items[0].x == 58.0f && m_Items[0].y == 100.0f);
    CHECK(M_SameVec(m_Items[1], M_LaserPoint(0.0f)));
    CHECK(M_SameVec(m_Items[2], M_LaserPoint(32.0f)));
    CHECK(M_SameVec(m_Items[3], M_LaserPoint(64.0f)));
    M_Teardown();
}

TEST(clear_all_with_bonus_scores_every_bullet_up_to_the_cap)
{
    M_Setup("bonus");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    for (int32_t i = 0; i < 3; i++) {
        M_Fire(&shooter);
    }
    g_Bullets.bullets[1].state = BULLET_FADING;
    g_GameManager.score.value = 7;
    Bullet_ClearAllWithBonus(2015, false);
    CHECK_EQ_INT(g_GameManager.score.value, 7 + 2000 + 2010 + 2015);
    CHECK_EQ_INT(m_BonusPopups, 1);
    CHECK_EQ_INT(m_BonusPopup, 2000 + 2010 + 2015);
    CHECK_EQ_INT(g_AsciiText.next_popup, 3);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[0], 0);
    CHECK_EQ_INT(g_AsciiText.popups[1].digits[1], 1);
    CHECK_EQ_INT(g_AsciiText.popups[2].digits[0], 5);
    CHECK_EQ_INT(g_AsciiText.popups[0].color, 0xffffffffu);
    CHECK_EQ_INT(g_AsciiText.popups[2].color, 0xffffff00u);
    for (int32_t i = 0; i < 3; i++) {
        CHECK(g_Bullets.bullets[i].state == BULLET_FADING);
    }
    CHECK_EQ_INT(M_CountItems(), 0);
    M_Teardown();
}

TEST(clear_all_with_bonus_gives_the_first_bullet_2000_above_the_cap)
{
    M_Setup("bonus_cap");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    M_Fire(&shooter);
    M_Fire(&shooter);
    Bullet_ClearAllWithBonus(1000, false);
    CHECK_EQ_INT(g_GameManager.score.value, 2000 + 1000);
    M_Teardown();
}

TEST(clear_all_with_no_bullets_shows_no_bonus)
{
    M_Setup("bonus_none");
    M_ClearableLaser();
    Bullet_ClearAllWithBonus(2000, false);
    CHECK_EQ_INT(g_GameManager.score.value, 0);
    CHECK_EQ_INT(m_BonusPopups, 0);
    M_Teardown();
}

TEST(clear_all_with_bonus_to_items_drops_a_star_at_each_laser_too)
{
    M_Setup("bonus_items");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 1.0f);
    BULLET *const bullet = M_Fire(&shooter);
    bullet->state = BULLET_FADING;
    LASER *const laser = M_ClearableLaser();
    Bullet_ClearAllWithBonus(2000, true);
    CHECK(bullet->state == BULLET_FADING);
    CHECK_EQ_INT(laser->state, LASER_ENDING);
    CHECK_EQ_INT(M_CountItems(), 5);
    CHECK(m_Items[0].x == 108.0f && m_Items[0].y == 100.0f);
    CHECK(m_Items[1].x == 108.0f && m_Items[1].y == 100.0f);
    CHECK(M_SameVec(m_Items[2], M_LaserPoint(0.0f)));
    CHECK(M_SameVec(m_Items[4], M_LaserPoint(64.0f)));
    M_Teardown();
}

TEST(bullet_draws_keep_distinct_lifetimes_when_pool_slots_are_reused)
{
    M_Setup("bullet_presentation");
    CHECK(IS_OK(Anm_CreateTexture(6, 256, 256, ANM_TEXTURE_RGBA8)));
    g_GameManager.camera.viewport = (VIEWPORT) { 0, 0, 640, 480, 0, 1 };
    g_GameManager.camera.view = Mat4_Identity();
    g_GameManager.camera.projection = Mat4_Identity();
    const BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 1.0f);
    BULLET *const first = M_Fire(&shooter);
    BULLET *const second = M_Fire(&shooter);
    GfxRecorder_Start();
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    const float x = GfxRecorder_Quad(1)->vertices[0].position.x;
    first->state = BULLET_FREE;
    g_Bullets.next_bullet = 0;
    CHECK(M_Fire(&shooter) == first);
    first->pos.x += 10.0f;
    second->pos.x += 10.0f;
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK(GfxRecorder_Blend(0.0f, 0)[0].position.x == x + 10.0f);
    CHECK(GfxRecorder_Blend(0.0f, 1)[0].position.x == x);
    GfxRecorder_Stop();
    M_Teardown();
}
