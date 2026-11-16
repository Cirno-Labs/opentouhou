#include "th07/bullet.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/scene.h"
#include "th07/sound.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <string.h>

#define M_SPRITE 0x200
#define M_SPRITE_8 (M_SPRITE + 0)
#define M_SPRITE_16 (M_SPRITE + 16)
#define M_SPRITE_32 (M_SPRITE + 32)
#define M_SPRITE_64 (M_SPRITE + 48)
#define M_SPRITE_ORIGIN 0x292
#define M_LASER_SCRIPT 0x20a
#define M_SLOT 0xb
#define M_SPRITE_OP 3
#define M_DELETE_OP 1
#define M_MAX_CALLS 64
#define M_SOUND 7

// The test links these instead of the modules they stand for. The player
// is at m_AimAngle from everything, and the contacts it reports are
// m_Graze and m_Hit. Each stand-in records its calls.

typedef struct {
    VEC3 pos;
    int32_t type;
    int32_t state;
} M_ITEM_CALL;

typedef struct {
    VEC3 pos;
    int32_t value;
    uint32_t color;
} M_POPUP_CALL;

typedef struct {
    VEC3 pos;
    VEC3 size;
    VEC3 origin;
    float angle;
    bool can_graze;
} M_LASER_HIT;

CHAIN g_Chain;
RNG g_Rng;
float g_GameSpeed;
PLAYER g_Player;

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static char *m_Directory;
static SCENE m_Scene;
static float m_AimAngle;
static int32_t m_AngleCalls;
static VEC3 m_AngleFrom;
static PLAYER_CONTACT m_Graze;
static PLAYER_CONTACT m_Hit;
static int32_t m_GrazeCalls;
static int32_t m_HitCalls;
static float m_GrazeXs[M_MAX_CALLS];
static M_LASER_HIT m_LaserHits[M_MAX_CALLS];
static int32_t m_LaserHitCount;
static M_ITEM_CALL m_Items[M_MAX_CALLS];
static int32_t m_ItemCount;
static int32_t m_ItemResets;
static int32_t m_ItemUpdates;
static int32_t m_CountAtItemUpdate;
static M_POPUP_CALL m_Popups[M_MAX_CALLS];
static int32_t m_PopupCount;
static int32_t m_Sounds[M_MAX_CALLS];
static int32_t m_SoundCount;

SCENE Scene_GetRequested(void)
{
    return m_Scene;
}

float Player_AngleFrom(const VEC3 pos)
{
    m_AngleCalls++;
    m_AngleFrom = pos;
    return m_AimAngle;
}

PLAYER_CONTACT Player_CheckGraze(const VEC3 pos, const VEC3 size)
{
    if (m_GrazeCalls < M_MAX_CALLS) {
        m_GrazeXs[m_GrazeCalls] = pos.x;
    }
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
    if (m_LaserHitCount < M_MAX_CALLS) {
        m_LaserHits[m_LaserHitCount] =
            (M_LASER_HIT) { pos, size, origin, angle, can_graze };
    }
    m_LaserHitCount++;
}

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
    if (m_ItemCount < M_MAX_CALLS) {
        m_Items[m_ItemCount] = (M_ITEM_CALL) { pos, type, state };
    }
    m_ItemCount++;
}

void Item_Reset(void)
{
    m_ItemResets++;
}

void Item_Update(void)
{
    m_ItemUpdates++;
    m_CountAtItemUpdate = g_Bullets.count;
}

// Draws an untextured quad, which marks where the items are drawn.
void Item_Draw(void)
{
    Anm_DrawRect((GFX_RECT) { 0, 0, 1, 1 }, COLOR_ARGB(0xff, 0x00, 0xff, 0x00));
}

void AsciiText_AddPopup(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
    if (m_PopupCount < M_MAX_CALLS) {
        m_Popups[m_PopupCount] = (M_POPUP_CALL) { pos, value, color };
    }
    m_PopupCount++;
}

void Sound_Queue(const int32_t sound)
{
    if (m_SoundCount < M_MAX_CALLS) {
        m_Sounds[m_SoundCount] = sound;
    }
    m_SoundCount++;
}

static void M_AddScript(
    ANM_BUILDER_ANM *const anm, const int32_t script, const int32_t sprite,
    const int16_t end)
{
    const int32_t index = AnmBuilder_BeginScript(anm, script - M_SPRITE);
    if (sprite >= 0) {
        const uint32_t arg = (uint32_t)(sprite - M_SPRITE);
        AnmBuilder_Instr(anm, index, M_SPRITE_OP, 0, 0, &arg, 1);
    }
    AnmBuilder_Instr(anm, index, M_DELETE_OP, end, 0, nullptr, 0);
    AnmBuilder_End(anm, index);
}

// Writes etama.anm: 16 sprites each 8, 16, 32 and 64 high from M_SPRITE_8,
// M_SPRITE_16, M_SPRITE_32 and M_SPRITE_64. The flying scripts of types 0,
// 1 to 6, 7 to 9 and 10 show the first sprite of each height, type 10 the
// sprite large_sprite. Spawn scripts end at frames 2 to 4, fade scripts at
// 4, flying scripts at 1000. Laser type 0 shows M_SPRITE_16.
static void M_WriteEtama(const int32_t large_sprite)
{
    static ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 256, 256, "@");
    for (int32_t i = 0; i < 16; i++) {
        AnmBuilder_AddSprite(anm, i, 0.0f, 0.0f, 8.0f, 8.0f);
        AnmBuilder_AddSprite(anm, 16 + i, 0.0f, 0.0f, 16.0f, 16.0f);
        AnmBuilder_AddSprite(anm, 32 + i, 0.0f, 0.0f, 32.0f, 32.0f);
        AnmBuilder_AddSprite(anm, 48 + i, 0.0f, 0.0f, 64.0f, 64.0f);
    }
    M_AddScript(anm, 512, M_SPRITE_8, 1000);
    for (int32_t i = 513; i <= 518; i++) {
        M_AddScript(anm, i, M_SPRITE_16, 1000);
    }
    for (int32_t i = 519; i <= 521; i++) {
        M_AddScript(anm, i, M_SPRITE_32, 1000);
    }
    M_AddScript(anm, 680, large_sprite, 1000);
    M_AddScript(anm, 527, M_SPRITE_8, 4);
    M_AddScript(anm, 528, M_SPRITE_16, 4);
    M_AddScript(anm, 529, M_SPRITE_32, 4);
    M_AddScript(anm, 681, M_SPRITE_64, 4);
    for (int32_t i = 0; i < 3; i++) {
        M_AddScript(anm, 530 + i, M_SPRITE_8, (int16_t)(2 + i));
        M_AddScript(anm, 533 + i, M_SPRITE_16, (int16_t)(2 + i));
    }
    M_AddScript(anm, 536, M_SPRITE_32, 3);
    M_AddScript(anm, M_LASER_SCRIPT, M_SPRITE_16, 1000);
    M_AddScript(anm, 682, M_SPRITE_64, 3);
    AnmBuilder_WriteFile(&builder, m_Directory, "data/etama.anm");
}

static void M_Prepare(const char *const case_name, const int32_t large_sprite)
{
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    CHECK(SDL_CreateDirectory(data));
    Mem_Free(data);
    M_WriteEtama(large_sprite);
    CHECK(Result_Log(Files_Init(m_Directory), LOG_LEVEL_ERROR));
    g_GameSpeed = 1.0f;
    Rng_SetSeed(&g_Rng, 99);
    Anm_Init(&g_HeapAllocator, &g_Rng);
    Chain_Init(&g_Chain);
    m_Scene = SCENE_GAMEPLAY;
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.clear_item = ITEM_STAR;
    m_AimAngle = 0.0f;
    m_AngleCalls = 0;
    m_Graze = PLAYER_CONTACT_NONE;
    m_Hit = PLAYER_CONTACT_NONE;
    m_GrazeCalls = 0;
    m_HitCalls = 0;
    m_LaserHitCount = 0;
    m_ItemCount = 0;
    m_ItemResets = 0;
    m_ItemUpdates = 0;
    m_PopupCount = 0;
    m_SoundCount = 0;
}

static void M_Setup(const char *const case_name)
{
    M_Prepare(case_name, M_SPRITE_64);
    CHECK(Result_Log(Bullet_Register(), LOG_LEVEL_ERROR));
    // Stands in for the laser origins, which lie past the test sprites.
    for (int32_t i = 0; i < 5; i++) {
        Anm_SetSpriteEntry(
            M_SPRITE_ORIGIN + i, Anm_GetSpriteEntry(M_SPRITE_16 + i));
    }
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

// Returns a shooter of one bullet of type at 100, 100, flying at angle
// with speed.
static BULLET_SHOOTER M_Shooter(
    const int16_t type, const float angle, const float speed)
{
    BULLET_SHOOTER shooter;
    BulletShooter_Init(&shooter);
    shooter.type = type;
    shooter.pos = (VEC3) { 100.0f, 100.0f, 0.0f };
    shooter.angle1 = angle;
    shooter.speed1 = speed;
    shooter.speed2 = speed;
    shooter.columns = 1;
    shooter.rows = 1;
    shooter.aim = BULLET_AIM_FAN;
    return shooter;
}

static BULLET *M_Fire(const BULLET_SHOOTER *const shooter)
{
    BULLET *const bullet = &g_Bullets.bullets[g_Bullets.next_bullet];
    Bullet_FirePattern(shooter);
    return bullet;
}

// Fires a bullet of type 1 at 100, 100 that stays there.
static BULLET *M_FireStill(void)
{
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.0f);
    return M_Fire(&shooter);
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

static bool M_SameVec(const VEC3 a, const VEC3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static int32_t M_CountSounds(const int32_t sound)
{
    int32_t count = 0;
    for (int32_t i = 0; i < m_SoundCount && i < M_MAX_CALLS; i++) {
        if (m_Sounds[i] == sound) {
            count++;
        }
    }
    return count;
}

TEST(registering_loads_the_bullet_sprites_and_removing_releases_them)
{
    M_Setup("register");
    CHECK(Anm_GetScript(512) != nullptr);
    CHECK(Anm_GetScript(680) != nullptr);
    CHECK(g_Bullets.types[0].flying.z_write_off);
    CHECK(g_Bullets.types[0].spawning[2].z_write_off);
    CHECK(g_Bullets.types[10].fading.z_write_off);
    CHECK_EQ_INT(g_Bullets.types[10].flying.base_sprite, M_SPRITE_64);
    M_FireStill();
    Bullet_Remove();
    CHECK(Anm_GetScript(512) == nullptr);
    CHECK(Anm_GetScript(680) == nullptr);
    CHECK_EQ_INT(M_CountBullets(), 0);
    CHECK(g_Bullets.types[0].flying.script == nullptr);
    M_Teardown();
}

TEST(the_stage_scenes_keep_the_bullet_sprites)
{
    static const SCENE scenes[] = {
        SCENE_NEXT_STAGE,
        SCENE_RESTART_STAGE,
        SCENE_NEXT_STAGE_AGAIN,
    };
    M_Setup("kept_sprites");
    char *const path = TestDir_Join(m_Directory, "data/etama.anm");
    SDL_RemovePath(path);
    Mem_Free(path);
    for (int32_t i = 0; i < (int32_t)ARRAY_SIZE(scenes); i++) {
        m_Scene = scenes[i];
        Bullet_Remove();
        CHECK(Anm_GetScript(512) != nullptr);
        CHECK(Result_Log(Bullet_Register(), LOG_LEVEL_ERROR));
        CHECK(g_Bullets.types[7].hitbox_size.x == 10.0f);
    }
    m_Scene = SCENE_GAMEPLAY;
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

TEST(registering_resets_the_bullets_and_removes_every_item)
{
    M_Setup("register_reset");
    CHECK_EQ_INT(m_ItemResets, 1);
    CHECK_EQ_INT(g_Bullets.clear_item, ITEM_STAR);
    M_FireStill();
    g_Bullets.clear_item = ITEM_BOSS_STAR;
    g_Bullets.clear_frames = 3;
    Bullet_Remove();
    CHECK(Result_Log(Bullet_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(m_ItemResets, 2);
    CHECK_EQ_INT(M_CountBullets(), 0);
    CHECK_EQ_INT(g_Bullets.next_bullet, 0);
    CHECK_EQ_INT(g_Bullets.clear_frames, 0);
    CHECK_EQ_INT(g_Bullets.clear_item, ITEM_STAR);
    M_Teardown();
}

TEST(the_hitbox_and_size_class_follow_the_sprite_height_and_the_script)
{
    M_Setup("hitbox");
    static const float sizes[BULLET_TYPE_COUNT] = {
        4.0f, 6.0f, 4.0f, 6.0f, 4.0f, 4.0f, 4.0f, 10.0f, 5.0f, 8.0f, 24.0f,
    };
    static const uint8_t classes[BULLET_TYPE_COUNT] = {
        5, 3, 4, 3, 4, 4, 4, 2, 1, 2, 0,
    };
    for (int16_t type = 0; type < BULLET_TYPE_COUNT; type++) {
        const BULLET_SHOOTER shooter = M_Shooter(type, 0.0f, 1.0f);
        const BULLET *const bullet = M_Fire(&shooter);
        CHECK(bullet->sprites.hitbox_size.x == sizes[type]);
        CHECK(bullet->sprites.hitbox_size.y == sizes[type]);
        CHECK(bullet->sprites.hitbox_size.z == 0.0f);
        CHECK_EQ_INT(bullet->sprites.size_class, classes[type]);
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
                    angle += (float)col * MATH_TWO_PI / 5.0f;
                    angle += (float)row * 0.1f + 0.25f;
                } else {
                    angle += MATH_PI / 5.0f;
                    angle += (float)col * MATH_TWO_PI / 5.0f;
                    angle += 0.25f;
                }
                const BULLET *const bullet =
                    &g_Bullets.bullets[first + row * 5 + col];
                CHECK(bullet->angle == Angle_AddNormalize(angle, 0.0f));
                CHECK(M_SameVec(bullet->velocity, M_Velocity(angle, 2.0f)));
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
                    angle += (float)col * MATH_TWO_PI / 3.0f;
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

TEST(a_single_row_takes_speed1)
{
    M_Setup("single_row");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 3.0f);
    shooter.speed2 = 1.0f;
    CHECK(M_Fire(&shooter)->speed == 3.0f);
    M_Teardown();
}

TEST(the_velocity_takes_the_unnormalized_angle_at_the_game_speed)
{
    M_Setup("velocity");
    g_GameSpeed = 0.5f;
    const BULLET_SHOOTER shooter = M_Shooter(1, 4.0f, 3.0f);
    const BULLET *const bullet = M_Fire(&shooter);
    CHECK(bullet->angle == Angle_AddNormalize(4.0f, 0.0f));
    CHECK(bullet->speed == 3.0f);
    CHECK(M_SameVec(bullet->velocity, M_Velocity(4.0f, 3.0f * 0.5f)));
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
    shooter.sound = M_SOUND;
    Bullet_FirePattern(&shooter);
    Bullet_FirePattern(&shooter);
    CHECK_EQ_INT(M_CountSounds(M_SOUND), 2);
    shooter.flags = 0;
    Bullet_FirePattern(&shooter);
    CHECK_EQ_INT(m_SoundCount, 2);
    M_Teardown();
}

TEST(a_full_count_at_the_last_update_fires_nothing_and_no_sound)
{
    M_Setup("full_count");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 0.0f);
    shooter.flags = BULLET_FLAG_SOUND;
    shooter.sound = M_SOUND;
    g_Bullets.count = BULLET_COUNT;
    Bullet_FirePattern(&shooter);
    CHECK_EQ_INT(M_CountBullets(), 0);
    CHECK_EQ_INT(m_SoundCount, 0);
    CHECK_EQ_INT(m_AngleCalls, 0);
    g_Bullets.count = BULLET_COUNT - 1;
    Bullet_FirePattern(&shooter);
    CHECK_EQ_INT(M_CountBullets(), 1);
    M_Teardown();
}

TEST(a_spawn_takes_the_first_free_bullet_from_next_bullet)
{
    M_Setup("next_free");
    BULLET_SHOOTER shooter = M_Shooter(0, 0.0f, 0.0f);
    shooter.columns = BULLET_COUNT;
    Bullet_FirePattern(&shooter);
    g_Bullets.bullets[5].state = BULLET_FREE;
    g_Bullets.bullets[4].state = BULLET_FREE;
    g_Bullets.bullets[2].state = BULLET_FREE;
    g_Bullets.next_bullet = 4;
    shooter.columns = 1;
    Bullet_FirePattern(&shooter);
    CHECK(g_Bullets.bullets[4].state == BULLET_FLYING);
    CHECK(g_Bullets.bullets[5].state == BULLET_FREE);
    CHECK_EQ_INT(g_Bullets.next_bullet, 5);
    g_Bullets.bullets[5].state = BULLET_FLYING;
    g_Bullets.bullets[BULLET_COUNT - 1].state = BULLET_FREE;
    g_Bullets.next_bullet = BULLET_COUNT - 1;
    Bullet_FirePattern(&shooter);
    CHECK(g_Bullets.bullets[BULLET_COUNT - 1].state == BULLET_FLYING);
    CHECK_EQ_INT(g_Bullets.next_bullet, 0);
    g_Bullets.next_bullet = 6;
    Bullet_FirePattern(&shooter);
    CHECK(g_Bullets.bullets[2].state == BULLET_FLYING);
    CHECK_EQ_INT(g_Bullets.next_bullet, 3);
    M_Teardown();
}

TEST(a_bullet_takes_the_sprites_of_its_color)
{
    M_Setup("colors");
    static const struct {
        int16_t type;
        int32_t fading;
    } cases[] = {
        { 0, M_SPRITE_8 + 2 },
        { 1, M_SPRITE_16 + 2 },
        { 7, M_SPRITE_32 + 3 },
        { 10, M_SPRITE_64 + 5 },
    };
    for (int32_t i = 0; i < (int32_t)ARRAY_SIZE(cases); i++) {
        BULLET_SHOOTER shooter = M_Shooter(cases[i].type, 0.0f, 0.0f);
        shooter.color = 5;
        shooter.flags = BULLET_FLAG_SPAWN_2;
        const BULLET *const bullet = M_Fire(&shooter);
        const BULLET_SPRITES *const type = &g_Bullets.types[cases[i].type];
        CHECK_EQ_INT(bullet->sprites.flying.sprite, type->flying.sprite + 5);
        CHECK_EQ_INT(bullet->sprites.fading.sprite, cases[i].fading);
        CHECK_EQ_INT(bullet->sprites.spawning[1].sprite, cases[i].fading);
        CHECK_EQ_INT(bullet->color, 5);
    }
    M_Teardown();
}

TEST(a_type_whose_script_ended_leaves_the_slot_its_last_sprite)
{
    M_Setup("ended_type");
    BULLET *const bullet = M_FireStill();
    M_Frames(5);
    const int32_t time = bullet->sprites.flying.script_time.current;
    CHECK(time != g_Bullets.types[1].flying.script_time.current);
    bullet->state = BULLET_FREE;
    g_Bullets.next_bullet = 0;
    g_Bullets.types[1].flying.pc = -1;
    M_FireStill();
    CHECK_EQ_INT(bullet->sprites.flying.script_time.current, time);
    bullet->state = BULLET_FREE;
    g_Bullets.next_bullet = 0;
    g_Bullets.types[1].flying.pc = 0;
    M_FireStill();
    CHECK_EQ_INT(
        bullet->sprites.flying.script_time.current,
        g_Bullets.types[1].flying.script_time.current);
    M_Teardown();
}

TEST(a_spawning_bullet_starts_four_steps_back_and_moves_slower)
{
    M_Setup("spawning");
    static const BULLET_FLAG flags[] = {
        BULLET_FLAG_SPAWN_1,
        BULLET_FLAG_SPAWN_2,
        BULLET_FLAG_SPAWN_3,
    };
    static const float divisors[] = { 2.0f, 2.5f, 3.0f };
    for (int32_t i = 0; i < 3; i++) {
        BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 2.0f);
        shooter.flags = (int32_t)(flags[i] | BULLET_FLAG_SPAWN_3);
        BULLET *const bullet = M_Fire(&shooter);
        CHECK(bullet->state == (BULLET_STATE)(BULLET_SPAWNING_1 + i));
        const VEC3 velocity = M_Velocity(0.5f, 2.0f);
        VEC3 pos = Vec3_Sub(
            (VEC3) { 100.0f, 100.0f, 0.1f }, Vec3_Scale(velocity, 4.0f));
        CHECK(M_SameVec(bullet->pos, pos));
        const VEC3 step = Vec3_Scale(velocity, 1.0f / divisors[i]);
        int32_t frames = 0;
        while (bullet->state != BULLET_FLYING && frames < 10) {
            M_Frame();
            pos = Vec3_Add(pos, step);
            frames++;
            CHECK_EQ_INT(bullet->graze_timer.current, 0);
        }
        CHECK_EQ_INT(frames, 2 + i);
        pos = Vec3_Add(pos, velocity);
        CHECK(M_SameVec(bullet->pos, pos));
        CHECK_EQ_INT(bullet->timer.current, 1);
    }
    M_Teardown();
}

TEST(a_bullet_grazes_after_sixteen_frames_at_the_game_speed)
{
    M_Setup("graze_timer");
    g_GameSpeed = 0.5f;
    M_FireStill();
    m_Graze = PLAYER_CONTACT_NONE;
    M_Frames(32);
    CHECK_EQ_INT(m_GrazeCalls, 0);
    M_Frame();
    CHECK_EQ_INT(m_GrazeCalls, 1);
    M_Teardown();
}

TEST(a_fading_bullet_moves_at_half_speed_and_is_freed_with_its_script)
{
    M_Setup("fading");
    const BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 2.0f);
    BULLET *const bullet = M_Fire(&shooter);
    bullet->state = BULLET_FADING;
    VEC3 pos = bullet->pos;
    for (int32_t i = 0; i < 3; i++) {
        M_Frame();
        pos = Vec3_Add(pos, Vec3_Scale(bullet->velocity, 1.0f / 2.0f));
        CHECK(M_SameVec(bullet->pos, pos));
    }
    M_Frame();
    CHECK(bullet->state == BULLET_FREE);
    M_Teardown();
}

TEST(a_freed_bullet_keeps_its_sprites_and_position)
{
    M_Setup("freed");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.0f);
    shooter.pos.x = -100.0f;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    CHECK(bullet->state == BULLET_FREE);
    CHECK(bullet->pos.x == -100.0f);
    CHECK_EQ_INT(bullet->sprites.flying.script_index, 513);
    CHECK_EQ_INT(g_Bullets.count, 1);
    M_Frame();
    CHECK_EQ_INT(g_Bullets.count, 0);
    M_Teardown();
}

TEST(the_update_walks_bullet_0_then_the_rest_from_the_last_down)
{
    M_Setup("walk");
    static const int32_t slots[] = { 0, 1, 2, BULLET_COUNT - 1 };
    for (int32_t i = 0; i < 4; i++) {
        g_Bullets.next_bullet = slots[i];
        BULLET *const bullet = M_FireStill();
        bullet->pos.x = (float)(10 + i);
        bullet->graze_timer.current = 16;
    }
    M_Frame();
    CHECK_EQ_INT(m_GrazeCalls, 4);
    CHECK(m_GrazeXs[0] == 10.0f);
    CHECK(m_GrazeXs[1] == 13.0f);
    CHECK(m_GrazeXs[2] == 12.0f);
    CHECK(m_GrazeXs[3] == 11.0f);
    static const int32_t drawn[] = { 1, 2, BULLET_COUNT - 1, 0 };
    const BULLET *bullet = g_Bullets.draw_lists[3];
    for (int32_t i = 0; i < 4; i++) {
        CHECK(bullet == &g_Bullets.bullets[drawn[i]]);
        if (bullet == nullptr) {
            return;
        }
        bullet = bullet->next_drawn;
    }
    CHECK(bullet == nullptr);
    CHECK(g_Bullets.draw_lists[5] == nullptr);
    M_Teardown();
}

TEST(zeroing_a_bullet_after_the_update_cuts_its_draw_list)
{
    M_Setup("cut_list");
    g_Bullets.next_bullet = 5;
    M_FireStill();
    BULLET *const fading = M_FireStill();
    M_Frame();
    fading->state = BULLET_FADING;
    Bullet_ClearAllToItems();
    CHECK(g_Bullets.draw_lists[3] == &g_Bullets.bullets[5]);
    CHECK(g_Bullets.bullets[5].state == BULLET_FREE);
    CHECK(g_Bullets.bullets[5].next_drawn == nullptr);
    M_Teardown();
}

TEST(the_update_runs_the_items_first)
{
    M_Setup("items_first");
    M_FireStill();
    M_FireStill();
    M_Frame();
    CHECK_EQ_INT(m_ItemUpdates, 1);
    CHECK_EQ_INT(m_CountAtItemUpdate, 0);
    M_Frame();
    CHECK_EQ_INT(m_ItemUpdates, 2);
    CHECK_EQ_INT(m_CountAtItemUpdate, 2);
    CHECK_EQ_INT(Bullet_Count(), 2);
    M_Teardown();
}

TEST(a_flying_bullet_moves_by_its_velocity_unscaled)
{
    M_Setup("flying");
    g_GameSpeed = 0.5f;
    const BULLET_SHOOTER shooter = M_Shooter(1, 1.0f, 2.0f);
    BULLET *const bullet = M_Fire(&shooter);
    const VEC3 velocity = bullet->velocity;
    VEC3 pos = bullet->pos;
    for (int32_t i = 0; i < 3; i++) {
        M_Frame();
        pos = Vec3_Add(pos, velocity);
    }
    CHECK(M_SameVec(bullet->pos, pos));
    CHECK(bullet->timer.current == 1 && bullet->timer.sub_frame == 0.5f);
    M_Teardown();
}

// Fires a still bullet of type 1 with the commands, and their flags.
static BULLET *M_FireCommands(
    const BULLET_COMMAND *const commands, const int32_t count)
{
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 2.0f);
    shooter.bullet_sound = M_SOUND;
    for (int32_t i = 0; i < count; i++) {
        BulletShooter_SetCommand(&shooter, i, commands[i]);
    }
    return M_Fire(&shooter);
}

TEST(a_command_that_is_not_concurrent_waits_for_the_running_ones)
{
    M_Setup("waits");
    for (int32_t concurrent = 0; concurrent < 2; concurrent++) {
        const BULLET_COMMAND commands[] = {
            { .flag = BULLET_FLAG_START_FAST },
            {
                .flag = BULLET_FLAG_CURVE,
                .frames = 100,
                .concurrent = concurrent != 0,
            },
        };
        BULLET *const bullet = M_FireCommands(commands, 2);
        CHECK_EQ_INT(bullet->running, BULLET_FLAG_START_FAST);
        CHECK_EQ_INT(bullet->next_command, 1);
        M_Frame();
        if (concurrent != 0) {
            CHECK_EQ_INT(
                bullet->running, BULLET_FLAG_START_FAST | BULLET_FLAG_CURVE);
            continue;
        }
        CHECK_EQ_INT(bullet->running, BULLET_FLAG_START_FAST);
        M_Frames(17);
        CHECK_EQ_INT(bullet->running, 0);
        CHECK_EQ_INT(bullet->next_command, 1);
        M_Frame();
        CHECK_EQ_INT(bullet->running, BULLET_FLAG_CURVE);
        CHECK_EQ_INT(bullet->next_command, 2);
    }
    M_Teardown();
}

TEST(a_command_without_its_flag_is_skipped_and_a_zero_flag_ends_them)
{
    M_Setup("skipped");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 2.0f);
    shooter.commands[0] = (BULLET_COMMAND) { .flag = BULLET_FLAG_CURVE };
    BulletShooter_SetCommand(
        &shooter, 1, (BULLET_COMMAND) { .flag = BULLET_FLAG_START_FAST });
    BULLET *bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->running, BULLET_FLAG_START_FAST);
    CHECK_EQ_INT(bullet->next_command, 2);
    shooter.commands[0] = (BULLET_COMMAND) {};
    bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->running, 0);
    CHECK_EQ_INT(bullet->next_command, 0);
    M_Teardown();
}

TEST(stay_outside_skips_the_play_area_test_and_starts_the_next_command)
{
    M_Setup("stay_outside");
    for (int32_t stay = 0; stay < 2; stay++) {
        BULLET_SHOOTER shooter = M_Shooter(1, MATH_PI, 1.0f);
        shooter.pos.x = -100.0f;
        if (stay != 0) {
            BulletShooter_SetCommand(
                &shooter, 0,
                (BULLET_COMMAND) {
                    .flag = BULLET_FLAG_STAY_OUTSIDE,
                    .frames = 3,
                });
            BulletShooter_SetCommand(
                &shooter, 1,
                (BULLET_COMMAND) { .flag = BULLET_FLAG_START_FAST });
        }
        BULLET *const bullet = M_Fire(&shooter);
        if (stay == 0) {
            M_Frame();
            CHECK(bullet->state == BULLET_FREE);
            continue;
        }
        CHECK_EQ_INT(bullet->running, BULLET_FLAG_START_FAST);
        CHECK_EQ_INT(bullet->keep_outside_frames, 3);
        M_Frames(2);
        CHECK(bullet->state == BULLET_FLYING);
        M_Frame();
        CHECK(bullet->state == BULLET_FREE);
    }
    M_Teardown();
}

TEST(a_later_acceleration_or_curve_plays_the_sound_as_it_starts)
{
    M_Setup("start_sound");
    const BULLET_COMMAND commands[] = {
        { .flag = BULLET_FLAG_ACCELERATE, .frames = 50 },
        { .flag = BULLET_FLAG_CURVE, .frames = 50, .concurrent = true },
        { .flag = BULLET_FLAG_ACCELERATE, .frames = 50, .concurrent = true },
    };
    M_FireCommands(commands, 3);
    CHECK_EQ_INT(m_SoundCount, 0);
    M_Frame();
    CHECK_EQ_INT(M_CountSounds(M_SOUND), 1);
    M_Frame();
    CHECK_EQ_INT(M_CountSounds(M_SOUND), 2);
    M_Teardown();
}

TEST(a_fast_start_slows_to_the_speed_over_sixteen_frames)
{
    M_Setup("start_fast");
    g_GameSpeed = 0.5f;
    const BULLET_COMMAND command = { .flag = BULLET_FLAG_START_FAST };
    BULLET *const bullet = M_FireCommands(&command, 1);
    for (int32_t i = 0; i < 33; i++) {
        const float t = Timer_AsFloat(&bullet->start_fast.timer);
        M_Frame();
        const float extra = 5.0f - t * 5.0f / 16.0f;
        CHECK(M_SameVec(
            bullet->velocity, M_Velocity(0.0f, (extra + 2.0f) * 0.5f)));
    }
    CHECK_EQ_INT(bullet->running, BULLET_FLAG_START_FAST);
    M_Frames(2);
    CHECK_EQ_INT(bullet->running, 0);
    M_Teardown();
}

TEST(an_acceleration_adds_along_its_angle_and_turns_with_the_velocity)
{
    M_Setup("accelerate");
    static const float angles[] = { -990.0f, -989.5f, 1.0f };
    for (int32_t i = 0; i < 3; i++) {
        const BULLET_COMMAND command = {
            .args = { 0.25f, angles[i] },
            .frames = 3,
            .flag = BULLET_FLAG_ACCELERATE,
        };
        BULLET *const bullet = M_FireCommands(&command, 1);
        const float angle = i == 0 ? 0.0f : angles[i];
        const VEC3 accel = M_Velocity(angle, 1.0f * 0.25f);
        CHECK(M_SameVec(bullet->accelerate.accel, accel));
        VEC3 velocity = bullet->velocity;
        for (int32_t frame = 0; frame < 3; frame++) {
            M_Frame();
            velocity = Vec3_Add(velocity, Vec3_Scale(accel, 1.0f));
            CHECK(M_SameVec(bullet->velocity, velocity));
            CHECK(bullet->angle == X87_Atan2(velocity.y, velocity.x));
        }
        CHECK(bullet->running != 0);
        M_Frame();
        CHECK_EQ_INT(bullet->running, 0);
        CHECK(M_SameVec(bullet->velocity, velocity));
    }
    M_Teardown();
}

TEST(a_curve_changes_speed_and_angle_at_the_game_speed)
{
    M_Setup("curve");
    g_GameSpeed = 0.5f;
    const BULLET_COMMAND command = {
        .args = { 0.25f, 0.125f },
        .frames = 2,
        .flag = BULLET_FLAG_CURVE,
    };
    BULLET *const bullet = M_FireCommands(&command, 1);
    float angle = 0.0f;
    float speed = 2.0f;
    for (int32_t i = 0; i < 4; i++) {
        M_Frame();
        angle = Angle_AddNormalize(angle, 0.5f * 0.125f);
        speed = 0.5f * 0.25f + speed;
        CHECK(bullet->angle == angle);
        CHECK(bullet->speed == speed);
        CHECK(M_SameVec(bullet->velocity, M_Velocity(angle, 0.5f * speed)));
    }
    M_Frame();
    CHECK_EQ_INT(bullet->running, 0);
    CHECK(bullet->speed == speed);
    M_Teardown();
}

TEST(a_turn_slows_between_turns_and_plays_the_sound_at_each)
{
    M_Setup("turn");
    const BULLET_COMMAND command = {
        .args = { 0.5f, 3.0f },
        .frames = 4,
        .count = 2,
        .flag = BULLET_FLAG_TURN,
    };
    BULLET *const bullet = M_FireCommands(&command, 1);
    for (int32_t i = 0; i < 4; i++) {
        M_Frame();
        const float t = (float)i;
        CHECK(M_SameVec(
            bullet->velocity, M_Velocity(0.0f, 2.0f - t * 2.0f / 4.0f)));
    }
    CHECK_EQ_INT(m_SoundCount, 0);
    M_Frame();
    CHECK(bullet->angle == 0.5f);
    CHECK(bullet->speed == 3.0f);
    CHECK(M_SameVec(bullet->velocity, M_Velocity(0.5f, 3.0f)));
    CHECK_EQ_INT(M_CountSounds(M_SOUND), 1);
    CHECK_EQ_INT(bullet->running, BULLET_FLAG_TURN);
    M_Frame();
    CHECK(M_SameVec(
        bullet->velocity, M_Velocity(0.5f, 3.0f - 1.0f * 3.0f / 4.0f)));
    M_Frames(3);
    CHECK(bullet->angle == 0.5f + 0.5f);
    CHECK_EQ_INT(M_CountSounds(M_SOUND), 2);
    CHECK_EQ_INT(bullet->running, 0);
    M_Teardown();
}

TEST(a_turn_aims_at_the_player_or_to_its_angle)
{
    M_Setup("turn_to");
    m_AimAngle = 3.0f;
    const BULLET_COMMAND commands[] = {
        {
            .args = { 0.5f, -999.0f },
            .frames = 1,
            .count = 1,
            .flag = BULLET_FLAG_TURN_AT_PLAYER,
        },
        {
            .args = { 0.75f, -998.0f },
            .frames = 1,
            .count = 1,
            .flag = BULLET_FLAG_TURN_TO,
        },
    };
    BULLET *bullet = M_FireCommands(&commands[0], 1);
    M_Frames(2);
    CHECK(bullet->angle == Angle_AddNormalize(3.0f, 0.5f));
    CHECK(bullet->speed == 2.0f);
    CHECK(M_SameVec(m_AngleFrom, bullet->pos) == false);
    bullet = M_FireCommands(&commands[1], 1);
    M_Frames(2);
    CHECK(bullet->angle == 0.75f);
    CHECK(bullet->speed == -998.0f);
    M_Teardown();
}

TEST(the_behaviours_run_together_in_one_frame)
{
    M_Setup("together");
    const BULLET_COMMAND commands[] = {
        { .flag = BULLET_FLAG_START_FAST },
        {
            .args = { 0.0f, 0.25f },
            .frames = 10,
            .flag = BULLET_FLAG_CURVE,
            .concurrent = true,
        },
    };
    BULLET *const bullet = M_FireCommands(commands, 2);
    M_Frame();
    const float angle = Angle_AddNormalize(0.0f, 1.0f * 0.25f);
    CHECK(bullet->angle == angle);
    CHECK(M_SameVec(bullet->velocity, M_Velocity(angle, 1.0f * 2.0f)));
    CHECK_EQ_INT(bullet->start_fast.timer.current, 1);
    CHECK_EQ_INT(bullet->curve.timer.current, 1);
    M_Teardown();
}

// Fires a bullet of type 1 at pos with angle and the bounce flag, for max
// bounces at speed 3.
static BULLET *M_FireBounce(
    const VEC3 pos, const float angle, const BULLET_FLAG flag,
    const int32_t max)
{
    BULLET_SHOOTER shooter = M_Shooter(1, angle, 5.0f);
    shooter.pos = pos;
    shooter.bullet_sound = M_SOUND;
    BulletShooter_SetCommand(
        &shooter, 0,
        (BULLET_COMMAND) { .args = { 3.0f }, .frames = max, .flag = flag });
    return M_Fire(&shooter);
}

TEST(a_bounce_turns_back_off_a_side_and_off_the_bottom)
{
    M_Setup("bounce");
    BULLET *bullet = M_FireBounce(
        (VEC3) { 395.0f, 100.0f, 0.0f }, 0.0f, BULLET_FLAG_BOUNCE, 2);
    M_Frame();
    CHECK(bullet->angle == Angle_AddNormalize(-0.0f - MATH_PI, 0.0f));
    CHECK(bullet->speed == 3.0f);
    CHECK_EQ_INT(bullet->bounce.count, 1);
    CHECK_EQ_INT(M_CountSounds(M_SOUND), 1);
    bullet = M_FireBounce(
        (VEC3) { 100.0f, 460.0f, 0.0f }, MATH_HALF_PI, BULLET_FLAG_BOUNCE, 1);
    M_Frame();
    CHECK(bullet->angle == -MATH_HALF_PI);
    CHECK_EQ_INT(bullet->running, 0);
    M_Teardown();
}

TEST(a_bounce_without_the_bottom_counts_each_frame_below_it)
{
    M_Setup("bounce_no_bottom");
    BULLET *const bullet = M_FireBounce(
        (VEC3) { 100.0f, 460.0f, 0.0f }, MATH_HALF_PI,
        BULLET_FLAG_BOUNCE_NO_BOTTOM, 3);
    M_Frames(2);
    CHECK(bullet->angle == MATH_HALF_PI);
    CHECK_EQ_INT(bullet->bounce.count, 2);
    CHECK_EQ_INT(M_CountSounds(M_SOUND), 2);
    CHECK_EQ_INT(bullet->out_of_bounds_frames, 2);
    M_Frame();
    CHECK_EQ_INT(bullet->running, 0);
    CHECK_EQ_INT(bullet->out_of_bounds_frames, 1);
    M_Frame();
    CHECK_EQ_INT(bullet->out_of_bounds_frames, 0);
    CHECK(bullet->state == BULLET_FLYING);
    M_Frame();
    CHECK(bullet->state == BULLET_FREE);
    M_Teardown();
}

TEST(a_turning_bullet_is_freed_after_0x80_frames_outside)
{
    M_Setup("outside");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.0f);
    shooter.pos.x = -100.0f;
    BulletShooter_SetCommand(
        &shooter, 0,
        (BULLET_COMMAND) {
            .args = { 0.0f, 0.0f },
            .frames = 1000,
            .count = 1,
            .flag = BULLET_FLAG_TURN,
        });
    BULLET *const bullet = M_Fire(&shooter);
    M_Frames(0x7f);
    CHECK(bullet->state == BULLET_FLYING);
    CHECK_EQ_INT(bullet->out_of_bounds_frames, 0x7f);
    M_Frame();
    CHECK(bullet->state == BULLET_FREE);
    M_Teardown();
}

TEST(a_bullet_back_inside_resets_its_frames_outside)
{
    M_Setup("back_inside");
    BULLET *const bullet = M_FireStill();
    bullet->out_of_bounds_frames = 5;
    M_Frame();
    CHECK_EQ_INT(bullet->out_of_bounds_frames, 0);
    M_Teardown();
}

TEST(a_graze_tests_the_hit_in_the_same_frame)
{
    M_Setup("graze");
    BULLET *const bullet = M_FireStill();
    M_Frames(16);
    CHECK_EQ_INT(m_GrazeCalls, 0);
    CHECK_EQ_INT(m_HitCalls, 16);
    m_Graze = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK(bullet->grazed);
    CHECK_EQ_INT(m_GrazeCalls, 1);
    CHECK_EQ_INT(m_HitCalls, 17);
    M_Frame();
    CHECK_EQ_INT(m_GrazeCalls, 1);
    CHECK_EQ_INT(m_HitCalls, 18);
    M_Teardown();
}

TEST(a_graze_without_contact_skips_the_hit)
{
    M_Setup("no_graze");
    BULLET *const bullet = M_FireStill();
    bullet->graze_timer.current = 16;
    m_Hit = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK_EQ_INT(m_GrazeCalls, 1);
    CHECK_EQ_INT(m_HitCalls, 0);
    CHECK(bullet->state == BULLET_FLYING);
    M_Teardown();
}

TEST(a_clear_area_turns_a_bullet_into_the_item_of_the_area)
{
    M_Setup("clear_area");
    g_Player.clear_item = ITEM_BOSS_STAR;
    for (int32_t graze = 0; graze < 2; graze++) {
        BULLET *const bullet = M_FireStill();
        bullet->graze_timer.current = graze != 0 ? 16 : 0;
        m_Graze = PLAYER_CONTACT_CLEARED;
        m_Hit = PLAYER_CONTACT_CLEARED;
        m_ItemCount = 0;
        m_HitCalls = 0;
        M_Frame();
        CHECK(bullet->state == BULLET_FADING);
        CHECK_EQ_INT(m_ItemCount, 1);
        CHECK_EQ_INT(m_HitCalls, graze != 0 ? 0 : 1);
        CHECK(M_SameVec(m_Items[0].pos, bullet->pos));
        CHECK_EQ_INT(m_Items[0].type, ITEM_BOSS_STAR);
        CHECK_EQ_INT(m_Items[0].state, ITEM_HOMING);
        bullet->state = BULLET_FREE;
    }
    M_Teardown();
}

TEST(a_resisting_bullet_ignores_clear_areas_but_not_the_hitbox)
{
    M_Setup("resists");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.0f);
    shooter.flags = BULLET_FLAG_RESIST_CLEAR;
    BULLET *const bullet = M_Fire(&shooter);
    bullet->graze_timer.current = 16;
    m_Graze = PLAYER_CONTACT_CLEARED;
    m_Hit = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK(bullet->state == BULLET_FLYING);
    CHECK_EQ_INT(m_HitCalls, 0);
    m_Graze = PLAYER_CONTACT_BOX;
    m_Hit = PLAYER_CONTACT_CLEARED;
    M_Frame();
    CHECK(bullet->state == BULLET_FLYING);
    CHECK_EQ_INT(m_HitCalls, 1);
    m_Hit = PLAYER_CONTACT_BOX;
    M_Frame();
    CHECK(bullet->state == BULLET_FADING);
    CHECK_EQ_INT(m_ItemCount, 0);
    M_Teardown();
}

// Returns a laser shooter at 100, 100 along 0.5 turned to the player, of
// type 0 and color 5, from 2 to 10, at most 100 long, 8 wide, moving 5,
// that starts 3 frames, hits from frame 1, fires 2 and ends 2, hitting 1
// frame of its end.
static BULLET_SHOOTER M_LaserShooter(void)
{
    BULLET_SHOOTER shooter = M_Shooter(0, 0.5f, 5.0f);
    shooter.color = 5;
    shooter.aim = BULLET_AIM_FAN_AT_PLAYER;
    shooter.flags = 0x10000 | BULLET_FLAG_SPAWN_1;
    shooter.laser.start_offset = 2.0f;
    shooter.laser.end_offset = 10.0f;
    shooter.laser.max_length = 100.0f;
    shooter.laser.width = 8.0f;
    shooter.laser.start_frames = 3;
    shooter.laser.firing_frames = 2;
    shooter.laser.end_frames = 2;
    shooter.laser.hit_start_frame = 1;
    shooter.laser.end_hit_frames = 1;
    return shooter;
}

TEST(a_laser_takes_its_values_from_the_shooter)
{
    M_Setup("laser");
    m_AimAngle = 1.0f;
    g_Bullets.lasers[0].hide_origin_while_starting = true;
    g_Bullets.lasers[0].shown_width = 3.0f;
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
    CHECK(!laser->hide_origin_while_starting);
    CHECK(laser->shown_width == 3.0f);
    CHECK(laser->beam.script == Anm_GetScript(M_LASER_SCRIPT));
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
    shooter.laser.start_frames = 0;
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

TEST(a_laser_in_the_clear_frames_returns_laser_0_unless_it_resists)
{
    M_Setup("laser_clear_frames");
    BULLET_SHOOTER shooter = M_LaserShooter();
    LASER *const first = Bullet_SpawnLaser(&shooter);
    Bullet_ClearAll();
    shooter.laser.width = 20.0f;
    CHECK(Bullet_SpawnLaser(&shooter) == first);
    CHECK(first->state == LASER_ENDING);
    CHECK(first->width == 0.0f);
    CHECK(!g_Bullets.lasers[1].active);
    shooter.flags |= LASER_FLAG_RESIST_CLEAR;
    CHECK(Bullet_SpawnLaser(&shooter) == &g_Bullets.lasers[1]);
    CHECK(g_Bullets.lasers[1].active);
    M_Frames(10);
    shooter.flags = 0;
    CHECK(Bullet_SpawnLaser(&shooter) == &g_Bullets.lasers[0]);
    CHECK(g_Bullets.lasers[0].state == LASER_STARTING);
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
    shooter.laser.start_frames = 0;
    shooter.laser.firing_frames = 100;
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
    shooter.laser.start_frames = 0;
    shooter.laser.firing_frames = 1;
    shooter.laser.end_frames = 0;
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

TEST(a_starting_laser_grows_in_its_last_30_frames_and_hits_with_it)
{
    M_Setup("laser_box");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.laser.start_offset = 0.0f;
    shooter.laser.start_frames = 40;
    shooter.laser.hit_start_frame = 0;
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
        CHECK(laser->shown_width == width);
        CHECK(laser->beam.scale_x == width / 16.0f);
        CHECK(laser->beam.scale_y == end / 16.0f);
        CHECK(laser->beam.rotation.z == laser->angle + MATH_HALF_PI);
    }
    M_Teardown();
}

TEST(a_laser_runs_at_the_game_speed)
{
    M_Setup("laser_slow");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.laser.start_frames = 4;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    shooter.flags = LASER_FLAG_FADE;
    shooter.laser.start_frames = 10;
    const LASER *const fading = Bullet_SpawnLaser(&shooter);
    g_GameSpeed = 0.5f;
    M_Frame();
    CHECK(laser->end_offset == 12.5f);
    CHECK(laser->shown_width == 1.2f);
    M_Frame();
    const int32_t alpha = X87_Ftol(0.5f * 255.0f / 10.0f);
    CHECK(fading->beam.color == (uint32_t)alpha << 24);
    M_Frames(2);
    CHECK(laser->end_offset == 20.0f);
    CHECK_EQ_INT(laser->timer.current, 2);
    CHECK(laser->shown_width == 1.5f * 8.0f / 4.0f);
    M_Teardown();
}

TEST(a_firing_laser_hits_with_its_length_and_follows_with_its_start)
{
    M_Setup("laser_length");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.laser.start_offset = -20.0f;
    shooter.laser.end_offset = 0.0f;
    shooter.laser.max_length = 12.0f;
    shooter.laser.start_frames = 0;
    shooter.laser.firing_frames = 100;
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
    shooter.laser.start_frames = 0;
    shooter.laser.firing_frames = 0;
    shooter.laser.end_frames = 4;
    shooter.laser.end_hit_frames = 2;
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

TEST(a_cleared_laser_narrows_from_its_shown_width_without_hitting)
{
    M_Setup("laser_cleared");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.laser.start_frames = 40;
    shooter.laser.end_hit_frames = 5;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    shooter.flags = LASER_FLAG_FADE;
    shooter.laser.start_frames = 1;
    shooter.laser.firing_frames = 100;
    const LASER *const firing = Bullet_SpawnLaser(&shooter);
    M_Frames(12);
    CHECK(firing->state == LASER_FIRING);
    Bullet_ClearAll();
    CHECK(laser->width == 11.0f * 8.0f / 40.0f);
    CHECK(firing->width == 8.0f);
    m_LaserHitCount = 0;
    M_Frame();
    CHECK(laser->beam.scale_x == laser->width / 16.0f);
    CHECK_EQ_INT(m_LaserHitCount, 0);
    M_Teardown();
}

TEST(a_fading_laser_fades_in_again_as_it_ends)
{
    M_Setup("laser_fade");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.flags = LASER_FLAG_FADE;
    shooter.laser.start_frames = 10;
    shooter.laser.firing_frames = 0;
    shooter.laser.end_frames = 20;
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

TEST(a_fading_laser_cleared_as_it_starts_keeps_the_last_shown_width)
{
    M_Setup("laser_fade_clear");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.flags = LASER_FLAG_FADE;
    shooter.laser.start_frames = 10;
    g_Bullets.lasers[0].shown_width = 3.0f;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    M_Frame();
    Bullet_ClearAll();
    CHECK(laser->width == 3.0f);
    M_Teardown();
}

TEST(a_laser_ends_once_its_start_reaches_640)
{
    M_Setup("laser_far");
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.laser.start_offset = 639.0f;
    shooter.laser.end_offset = 650.0f;
    shooter.laser.max_length = 10.0f;
    shooter.laser.start_frames = 0;
    shooter.laser.firing_frames = 100;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    M_Frame();
    CHECK(laser->start_offset == 645.0f);
    CHECK(!laser->active);
    CHECK_EQ_INT(laser->timer.current, 1);
    M_Teardown();
}

static void M_StartDraw(void)
{
    GfxRecorder_Start();
    if (Anm_GetTexture(M_SLOT) == nullptr) {
        CHECK(IS_OK(Anm_CreateTexture(M_SLOT, 256, 256, ANM_TEXTURE_RGBA8)));
    }
    g_GameManager.camera.viewport = (VIEWPORT) { 0, 0, 640, 480, 0, 1 };
    g_GameManager.play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
}

static void M_DrawFrame(void)
{
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
}

static bool M_IsItemQuad(const int32_t index)
{
    return GfxRecorder_Quad(index)->texture == nullptr;
}

static float M_QuadCenterX(const int32_t index)
{
    const GFX_VERTEX *const v = GfxRecorder_Quad(index)->vertices;
    return (v[0].position.x + v[1].position.x + v[2].position.x
            + v[3].position.x)
        / 4.0f;
}

TEST(the_bullets_draw_by_size_class_each_against_the_walk)
{
    M_Setup("draw_classes");
    M_StartDraw();
    static const int16_t types[] = { 1, 0, 10, 1 };
    BULLET *bullets[4];
    for (int32_t i = 0; i < 4; i++) {
        BULLET_SHOOTER shooter = M_Shooter(types[i], 0.0f, 0.0f);
        shooter.pos.x = 110.0f + 10.0f * (float)i;
        bullets[i] = M_Fire(&shooter);
    }
    M_Frame();
    M_DrawFrame();
    static const int32_t order[] = { 2, 3, 0, 1 };
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 5);
    CHECK(M_IsItemQuad(0));
    for (int32_t i = 0; i < 4; i++) {
        const float x = 32.0f + 110.0f + 10.0f * (float)order[i];
        CHECK(fabsf(M_QuadCenterX(1 + i) - x) <= 1.0f);
        CHECK(bullets[i]->sprites.flying.pos.y == 16.0f + 100.0f);
        CHECK(bullets[i]->sprites.flying.pos.z == 0.05f);
    }
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(a_bullet_draws_the_sprite_of_its_state_in_white_and_turned)
{
    M_Setup("draw_bullet");
    M_StartDraw();
    BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 0.0f);
    shooter.flags = BULLET_FLAG_SPAWN_2;
    BULLET *const bullet = M_Fire(&shooter);
    M_Frame();
    ANM_VM *const vm = &bullet->sprites.spawning[1];
    vm->color = COLOR_ARGB(0x80, 0x12, 0x34, 0x56);
    vm->auto_rotate = 1;
    vm->rotation_changed = false;
    M_DrawFrame();
    CHECK(vm->pos.x == 32.0f + bullet->pos.x);
    CHECK(vm->pos.y == 16.0f + bullet->pos.y);
    CHECK(vm->pos.z == 0.05f);
    CHECK(vm->color == COLOR_ARGB(0x80, 0xff, 0xff, 0xff));
    CHECK(vm->rotation.z == Angle_AddNormalize(bullet->angle, MATH_HALF_PI));
    CHECK(vm->rotation_changed);
    CHECK(bullet->sprites.flying.pos.z != 0.05f);
    bullet->state = BULLET_FADING;
    M_DrawFrame();
    CHECK(bullet->sprites.fading.pos.z == 0.05f);
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(a_laser_draws_its_beam_and_origin_before_the_items_and_bullets)
{
    M_Setup("draw_laser");
    M_StartDraw();
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.aim = BULLET_AIM_FAN;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    M_FireStill();
    M_Frame();
    laser->beam.color = COLOR_ARGB(0x40, 0x12, 0x34, 0x56);
    M_DrawFrame();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 4);
    CHECK(!M_IsItemQuad(0) && !M_IsItemQuad(1));
    CHECK(M_IsItemQuad(2) && !M_IsItemQuad(3));
    CHECK(fabsf(M_QuadCenterX(3) - (32.0f + 100.0f)) <= 1.0f);
    float s;
    float c;
    X87_SinCos(laser->angle, &s, &c);
    const float length =
        (laser->end_offset - laser->start_offset) / 2.0f + laser->start_offset;
    CHECK(laser->beam.pos.x == 32.0f + (c * length + 100.0f));
    CHECK(laser->beam.pos.y == 16.0f + (s * length + 100.0f));
    CHECK(laser->beam.pos.z == 0.05f);
    const ANM_VM *const origin = &laser->origin;
    CHECK(origin->pos.x == 32.0f + (c * laser->start_offset + 100.0f));
    CHECK(origin->pos.y == 16.0f + (s * laser->start_offset + 100.0f));
    CHECK(origin->pos.z == 0.05f);
    CHECK(origin->color == COLOR_ARGB(0xff, 0x12, 0x34, 0x56));
    const float scale = 8.0f / 10.0f * ((16.0f - laser->start_offset) / 16.0f);
    CHECK(origin->scale_x == scale && origin->scale_y == scale);
    GfxRecorder_Stop();
    M_Teardown();
}

TEST(a_laser_hides_its_origin_past_16_or_while_starting_hidden)
{
    M_Setup("draw_origin");
    M_StartDraw();
    BULLET_SHOOTER shooter = M_LaserShooter();
    shooter.aim = BULLET_AIM_FAN;
    LASER *const laser = Bullet_SpawnLaser(&shooter);
    laser->hide_origin_while_starting = true;
    M_DrawFrame();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    laser->state = LASER_FIRING;
    M_DrawFrame();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 3);
    laser->start_offset = 16.0f;
    M_DrawFrame();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    laser->speed = 0.0f;
    laser->origin.scale_x = 0.0f;
    M_DrawFrame();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 3);
    CHECK(laser->origin.scale_x == 8.0f / 10.0f);
    CHECK(laser->origin.scale_y == 8.0f / 10.0f);
    GfxRecorder_Stop();
    M_Teardown();
}

// Starts laser i from 50, 60 along angle 0.5, 70 long, starting, unless
// ending.
static LASER *M_Laser(const int32_t i, const uint16_t flags, const bool ending)
{
    LASER *const laser = &g_Bullets.lasers[i];
    *laser = (LASER) {
        .pos = { 50.0f, 60.0f, 0.0f },
        .angle = 0.5f,
        .end_offset = 70.0f,
        .width = 10.0f,
        .shown_width = 3.0f,
        .end_hit_frames = 5,
        .active = true,
        .flags = flags,
        .state = ending ? LASER_ENDING : LASER_STARTING,
    };
    laser->timer.current = 7;
    return laser;
}

static VEC3 M_LaserPoint(const float t)
{
    float s;
    float c;
    X87_SinCos(0.5f, &s, &c);
    return (VEC3) { c * t + 50.0f, s * t + 60.0f, 0.0f };
}

static void M_CheckEnded(const LASER *const laser)
{
    CHECK(laser->state == LASER_ENDING);
    CHECK_EQ_INT(laser->timer.current, 0);
    CHECK(laser->width == 3.0f);
    CHECK_EQ_INT(laser->end_hit_frames, 0);
}

TEST(clear_all_fades_the_bullets_and_ends_the_lasers_that_do_not_resist)
{
    M_Setup("clear_all");
    for (int32_t kept = 0; kept < 2; kept++) {
        BULLET *const flying = M_FireStill();
        BULLET *const fading = M_FireStill();
        fading->state = BULLET_FADING;
        const LASER *const plain = M_Laser(0, 0, false);
        const LASER *const resisting =
            M_Laser(1, LASER_FLAG_RESIST_CLEAR, false);
        const LASER *const ending = M_Laser(2, 0, true);
        if (kept != 0) {
            Bullet_ClearAllAndKeptLasers();
        } else {
            Bullet_ClearAll();
        }
        CHECK(flying->state == BULLET_FADING);
        CHECK(fading->state == BULLET_FADING);
        M_CheckEnded(plain);
        if (kept != 0) {
            M_CheckEnded(resisting);
        } else {
            CHECK(resisting->state == LASER_STARTING);
            CHECK_EQ_INT(resisting->end_hit_frames, 5);
        }
        CHECK_EQ_INT(ending->timer.current, 7);
        CHECK_EQ_INT(ending->end_hit_frames, 0);
        CHECK_EQ_INT(g_Bullets.clear_frames, 10);
        CHECK_EQ_INT(m_ItemCount, 0);
    }
    M_Teardown();
}

TEST(clear_all_to_items_turns_bullets_and_lasers_into_clear_items)
{
    M_Setup("to_items");
    g_Bullets.clear_item = ITEM_BOSS_STAR;
    BULLET *const flying = M_FireStill();
    flying->pos.x = 30.0f;
    BULLET *const fading = M_FireStill();
    fading->state = BULLET_FADING;
    const LASER *const plain = M_Laser(0, 0, false);
    const LASER *const resisting = M_Laser(1, LASER_FLAG_RESIST_CLEAR, false);
    M_Laser(2, 0, true);
    Bullet_ClearAllToItems();
    CHECK(flying->state == BULLET_FREE);
    CHECK(flying->sprites.flying.script == nullptr);
    CHECK(fading->state == BULLET_FADING);
    M_CheckEnded(plain);
    CHECK(resisting->state == LASER_STARTING);
    CHECK_EQ_INT(m_ItemCount, 4);
    CHECK(m_Items[0].pos.x == 30.0f);
    for (int32_t i = 0; i < 3; i++) {
        CHECK(M_SameVec(m_Items[1 + i].pos, M_LaserPoint(32.0f * (float)i)));
    }
    for (int32_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(m_Items[i].type, ITEM_BOSS_STAR);
        CHECK_EQ_INT(m_Items[i].state, ITEM_HOMING);
    }
    CHECK_EQ_INT(g_Bullets.clear_frames, 10);
    M_Teardown();
}

TEST(a_bullet_fired_in_the_clear_frames_fades_after_its_commands)
{
    M_Setup("clear_frames");
    Bullet_ClearAll();
    BULLET_SHOOTER shooter = M_Shooter(1, 0.0f, 0.0f);
    shooter.flags = BULLET_FLAG_SPAWN_1;
    BulletShooter_SetCommand(
        &shooter, 0, (BULLET_COMMAND) { .flag = BULLET_FLAG_START_FAST });
    const BULLET *bullet = M_Fire(&shooter);
    CHECK(bullet->state == BULLET_FADING);
    CHECK_EQ_INT(bullet->running, BULLET_FLAG_START_FAST);
    shooter.flags |= BULLET_FLAG_RESIST_CLEAR;
    bullet = M_Fire(&shooter);
    CHECK(bullet->state == BULLET_SPAWNING_1);
    M_Frames(9);
    CHECK_EQ_INT(g_Bullets.clear_frames, 1);
    M_Frame();
    CHECK_EQ_INT(g_Bullets.clear_frames, 0);
    shooter.flags = 0;
    CHECK(M_Fire(&shooter)->state == BULLET_FLYING);
    M_Frame();
    CHECK_EQ_INT(g_Bullets.clear_frames, 0);
    M_Teardown();
}

TEST(clear_all_with_bonus_scores_every_bullet_up_to_the_cap)
{
    M_Setup("bonus");
    for (int32_t i = 0; i < 4; i++) {
        M_FireStill();
    }
    g_Bullets.bullets[2].state = BULLET_FADING;
    const LASER *const resisting = M_Laser(1, LASER_FLAG_RESIST_CLEAR, false);
    CHECK_EQ_INT(Bullet_ClearAllWithBonus(2050, false), 8110);
    static const int32_t scores[] = { 2000, 2020, 2040, 2050 };
    CHECK_EQ_INT(m_PopupCount, 4);
    for (int32_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(m_Popups[i].value, scores[i]);
        CHECK(
            m_Popups[i].color
            == (i == 3 ? COLOR_ARGB(0xff, 0xff, 0xff, 0x00) : COLOR_WHITE));
        CHECK(g_Bullets.bullets[i].state == BULLET_FADING);
    }
    CHECK_EQ_INT(m_ItemCount, 4);
    M_CheckEnded(resisting);
    CHECK_EQ_INT(g_Bullets.clear_frames, 10);
    M_Teardown();
}

TEST(clear_all_with_bonus_gives_the_first_bullet_2000_above_the_cap)
{
    M_Setup("bonus_cap");
    M_FireStill();
    M_FireStill();
    CHECK_EQ_INT(Bullet_ClearAllWithBonus(1000, false), 3000);
    CHECK(m_Popups[0].color == COLOR_ARGB(0xff, 0xff, 0xff, 0x00));
    CHECK_EQ_INT(Bullet_ClearAllWithBonus(1000, false), 3000);
    M_Teardown();
}

TEST(clear_all_with_bonus_to_items_drops_items_at_and_along_each_laser)
{
    M_Setup("bonus_items");
    M_Laser(0, LASER_FLAG_RESIST_CLEAR, false);
    M_Laser(1, 0, true);
    CHECK_EQ_INT(Bullet_ClearAllWithBonus(5000, true), 0);
    CHECK_EQ_INT(m_ItemCount, 4);
    CHECK(M_SameVec(m_Items[0].pos, (VEC3) { 50.0f, 60.0f, 0.0f }));
    CHECK(M_SameVec(m_Items[3].pos, M_LaserPoint(64.0f)));
    M_Teardown();
}

TEST(clear_circle_turns_the_bullets_within_it_into_stars)
{
    M_Setup("circle");
    g_Bullets.clear_item = ITEM_BOSS_STAR;
    BULLET *const near = M_FireStill();
    near->pos.x = 103.0f;
    BULLET *const edge = M_FireStill();
    edge->pos.x = 104.0f;
    BULLET *const fading = M_FireStill();
    fading->state = BULLET_FADING;
    const VEC3 center = { 100.0f, 100.0f, 0.1f };
    Bullet_ClearCircleToItems(center, 3.5f);
    CHECK(near->state == BULLET_FREE);
    CHECK(near->sprites.flying.script == nullptr);
    CHECK(edge->state == BULLET_FLYING);
    CHECK(fading->state == BULLET_FADING);
    CHECK_EQ_INT(m_ItemCount, 1);
    CHECK(m_Items[0].pos.x == 103.0f);
    CHECK_EQ_INT(m_Items[0].type, ITEM_STAR);
    CHECK_EQ_INT(m_Items[0].state, ITEM_HOMING);
    M_Teardown();
}

TEST(stop_all_stops_every_bullet_in_color_0_and_keeps_its_behaviours)
{
    M_Setup("stop_all");
    BULLET_SHOOTER shooter = M_Shooter(1, 0.5f, 2.0f);
    shooter.color = 3;
    BulletShooter_SetCommand(
        &shooter, 0,
        (BULLET_COMMAND) {
            .args = { 0.0f, 0.25f },
            .frames = 10,
            .flag = BULLET_FLAG_CURVE,
        });
    BULLET *const bullet = M_Fire(&shooter);
    CHECK_EQ_INT(bullet->sprites.flying.sprite, M_SPRITE_16 + 3);
    Bullet_StopAll();
    CHECK(M_SameVec(bullet->velocity, (VEC3) {}));
    CHECK(bullet->speed == 0.0f);
    CHECK_EQ_INT(bullet->color, 0);
    CHECK_EQ_INT(bullet->sprites.flying.sprite, M_SPRITE_16);
    CHECK_EQ_INT(bullet->running, BULLET_FLAG_CURVE);
    M_Teardown();
}

TEST(the_setters_add_the_flag_and_a_bullet_starts_its_commands_over)
{
    M_Setup("setters");
    BULLET_SHOOTER shooter;
    BulletShooter_Init(&shooter);
    CHECK_EQ_INT(shooter.bullet_sound, -1);
    CHECK_EQ_INT(shooter.type, 0);
    const BULLET_COMMAND command = {
        .args = { 1.0f, 2.0f },
        .frames = 3,
        .count = 4,
        .flag = BULLET_FLAG_TURN_TO,
        .concurrent = true,
    };
    shooter.flags = BULLET_FLAG_SOUND;
    BulletShooter_SetCommand(&shooter, 4, command);
    CHECK_EQ_INT(shooter.flags, BULLET_FLAG_SOUND | BULLET_FLAG_TURN_TO);
    CHECK(shooter.commands[4].args[1] == 2.0f);
    CHECK_EQ_INT(shooter.commands[4].count, 4);
    BULLET *const bullet = M_FireStill();
    M_Frame();
    bullet->next_command = 3;
    Bullet_SetCommand(
        bullet, 0, (BULLET_COMMAND) { .flag = BULLET_FLAG_START_FAST });
    CHECK_EQ_INT(bullet->flags, BULLET_FLAG_START_FAST);
    CHECK_EQ_INT(bullet->next_command, 0);
    M_Frame();
    CHECK_EQ_INT(bullet->running, BULLET_FLAG_START_FAST);
    M_Teardown();
}
