#include "th07/ecl/patterns.h"

#include "th07/anm.h"
#include "th07/bullet.h"
#include "th07/effect.h"
#include "th07/enemy.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/screen_effect.h"
#include "th07/sound.h"
#include "th07/stage.h"

#include "core/base/assert.h"
#include "core/base/color.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <string.h>

#define M_TALL_HEIGHT 60.0f
#define M_BURST_HEIGHT 48.0f
#define M_BURST_SPREAD 4.712389f
#define M_QUARTER_PI 0.7853982f
// The flying sprites that slowing the game down swaps for its slow sprite.
#define M_SLOW_FIRST_SPRITE 0x260
#define M_SLOW_SPRITE 0x26f
#define M_UNTURNED_FIRST_SPRITE 0x278
#define M_UNTURNED_LAST_SPRITE 0x27f
#define M_UNTURNED_SPRITE 0x27c
// The bullet type that the bullets turned at a laser take.
#define M_LASER_TURN_TYPE 5

static void M_FreeBullet(BULLET *const bullet)
{
    bullet->state = BULLET_FREE;
    Timer_Set(&bullet->timer, 0);
    Timer_Set(&bullet->graze_timer, 0);
}

static bool M_IsLive(const BULLET *const bullet)
{
    return bullet->state != BULLET_FREE && bullet->state != BULLET_FADING;
}

// The original reads the sprite of a bullet without checking it.
static float M_Height(const BULLET *const bullet)
{
    ASSERT(bullet->sprites.flying.sprite_data != nullptr);
    return bullet->sprites.flying.sprite_data->height;
}

static int32_t M_Difficulty(void)
{
    return g_GameManager.run.difficulty;
}

static void M_SetLaserTurnSprites(BULLET *const bullet)
{
    bullet->sprites = g_Bullets.types[M_LASER_TURN_TYPE];
    Anm_RenewPresentation(&bullet->sprites.flying);
    for (int32_t i = 0; i < BULLET_SPAWN_SPRITE_COUNT; i++) {
        Anm_RenewPresentation(&bullet->sprites.spawning[i]);
    }
    Anm_RenewPresentation(&bullet->sprites.fading);
    Anm_SetSprite(
        &bullet->sprites.flying, bullet->sprites.flying.sprite + bullet->color);
}

static void M_WarpToBoss(ENEMY *const enemy, const int32_t arg)
{
    ASSERT(arg >= 0 && arg < ENEMY_BOSS_COUNT);
    const ENEMY *const boss = g_Enemies.bosses[arg];
    ASSERT(boss != nullptr);
    enemy->move.pos = boss->move.pos;
    enemy->move.velocity = boss->move.velocity;
    enemy->move.angle = boss->move.angle;
    enemy->flags.warping_to_boss = true;
}

static void M_SpinBullets(const int32_t arg)
{
    ScreenEffect_Shake(30, 12, 0);
    ScreenEffect_Flash(4, 3, COLOR_ARGB(0x80, 0xff, 0xcf, 0xcf));
    // The other colours keep the angle of the bullet before; the original
    // reads it uninitialised when no bullet came before.
    float angle = 0.0f;
    bool has_angle = false;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsLive(bullet) || bullet->sprites.flying.sprite_data == nullptr
            || bullet->pattern_state != 0) {
            continue;
        }
        if ((arg == 1 && bullet->color != 8)
            || (arg == 2 && bullet->color != 4)) {
            continue;
        }
        switch (bullet->color) {
        case 2:
        case 4:
            angle = -MATH_PI / (Rng_F32(&g_Rng) * 60.0f + 180.0f);
            has_angle = true;
            break;
        case 6:
        case 8:
            angle = MATH_PI / (Rng_F32(&g_Rng) * 60.0f + 180.0f);
            has_angle = true;
            break;
        default:
            ASSERT(has_angle);
            break;
        }
        bullet->speed = 0.3f;
        memset(bullet->commands, 0, sizeof(bullet->commands));
        if (M_Difficulty() < 3) {
            Bullet_SetCommand(
                bullet, 0,
                (BULLET_COMMAND) { .args = { 1.0f / 60.0f, angle },
                                   .frames = 60,
                                   .flag = BULLET_FLAG_CURVE });
        } else {
            Bullet_SetCommand(
                bullet, 0,
                (BULLET_COMMAND) { .args = { 1.0f / 190.0f, angle },
                                   .frames = 240,
                                   .flag = BULLET_FLAG_CURVE });
        }
        bullet->pattern_state = 1;
    }
}

static void M_FansFromNearBullets(const ENEMY *const enemy, const int32_t arg)
{
    float range;
    switch (arg) {
    case 0:
        ScreenEffect_Shake(32, 12, 0);
        ScreenEffect_Flash(4, 1, COLOR_ARGB(0x80, 0xcf, 0xcf, 0xff));
        range = 128.0f;
        break;
    case 1:
        range = 192.0f;
        break;
    case 2:
        range = 256.0f;
        break;
    case 3:
        range = 999.0f;
        break;
    default:
        // The original compares with an uninitialised range.
        ASSERT(false);
        range = 0.0f;
        break;
    }
    BULLET_SHOOTER shooter;
    BulletShooter_Init(&shooter);
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsLive(bullet) || bullet->sprites.flying.sprite_data == nullptr
            || bullet->color != 2) {
            continue;
        }
        const float dx = enemy->move.pos.x - bullet->pos.x;
        const float dy = enemy->move.pos.y - bullet->pos.y;
        if (!(X87_Sqrt(dx * dx + dy * dy) < range)) {
            continue;
        }
        shooter.pos = bullet->pos;
        shooter.type = 0;
        shooter.color = 6;
        shooter.angle1 = 0.0f;
        shooter.angle2 = -MATH_PI;
        shooter.speed1 = 0.7f;
        shooter.columns = 2;
        shooter.rows = 1;
        shooter.flags = BULLET_FLAG_SPAWN_1;
        shooter.aim = BULLET_AIM_RANDOM_ANGLE;
        BulletShooter_SetCommand(
            &shooter, 0,
            (BULLET_COMMAND) {
                .args = { Rng_F32InRange(&g_Rng, 0.005f) + 0.013f,
                          MATH_HALF_PI },
                .frames = 180,
                .flag = BULLET_FLAG_ACCELERATE });
        Bullet_FirePattern(&shooter);
        M_FreeBullet(bullet);
    }
}

static void M_TallBulletToEffect(ENEMY *const enemy)
{
    enemy->script.state.vars.floats[0] = -999.0f;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsLive(bullet) || !(M_Height(bullet) >= M_TALL_HEIGHT)) {
            continue;
        }
        enemy->script.state.vars.floats[0] = bullet->pos.x;
        enemy->script.state.vars.floats[1] = bullet->pos.y;
        Effect_Spawn(EFFECT_EXPLOSION_LARGE, bullet->pos, 1, COLOR_WHITE);
        M_FreeBullet(bullet);
        break;
    }
}

static void M_CopyBossCircle(ENEMY *const enemy)
{
    const ENEMY *const boss = g_Enemies.bosses[0];
    ASSERT(boss != nullptr);
    enemy->move.start = boss->move.pos;
    enemy->move.circle.radius = boss->move.circle.radius;
    enemy->move.circle.angular_velocity = boss->move.circle.angular_velocity;
}

static void M_FansBackFromBullets(const int32_t arg)
{
    BULLET_SHOOTER shooter;
    BulletShooter_Init(&shooter);
    const bool lunatic = M_Difficulty() == 3;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsLive(bullet) || bullet->sprites.flying.sprite_data == nullptr
            || !(
                (arg == 0 && bullet->color == 6)
                || (arg == 1 && bullet->color == 15)
                || (arg == 2 && bullet->color == 2))) {
            continue;
        }
        shooter.pos = bullet->pos;
        shooter.type = 6;
        shooter.color = 15;
        shooter.angle1 = Angle_AddNormalize(bullet->angle, MATH_PI);
        shooter.angle2 = 0.5235988f;
        shooter.speed1 = bullet->speed * 1.1f;
        if (M_Difficulty() < 3) {
            shooter.columns = 4;
        } else {
            shooter.columns = 2;
            shooter.angle2 = MATH_HALF_PI;
        }
        shooter.rows = 1;
        shooter.flags = BULLET_FLAG_SPAWN_1;
        BulletShooter_SetCommand(
            &shooter, 0,
            (BULLET_COMMAND) { .frames = 130,
                               .flag = BULLET_FLAG_STAY_OUTSIDE });
        // The flags below drop the command's flag again, except for arg 0
        // and on Lunatic, so that the command does not run.
        if (arg == 0) {
            shooter.flags = BULLET_FLAG_STAY_OUTSIDE | BULLET_FLAG_SPAWN_1;
        } else if (arg == 1) {
            shooter.flags = lunatic
                ? BULLET_FLAG_STAY_OUTSIDE | BULLET_FLAG_SPAWN_1
                : BULLET_FLAG_SPAWN_1;
            shooter.color = 2;
        } else if (arg == 2) {
            shooter.flags = BULLET_FLAG_SPAWN_1;
            shooter.color = 10;
        }
        shooter.aim = BULLET_AIM_FAN;
        Bullet_FirePattern(&shooter);
        shooter.angle2 = 1.0471976f;
        if (arg == 0) {
            shooter.flags = BULLET_FLAG_STAY_OUTSIDE;
        } else if (arg == 1) {
            shooter.flags = lunatic ? BULLET_FLAG_STAY_OUTSIDE : 0;
        } else {
            shooter.flags = 0;
        }
        shooter.speed1 = bullet->speed * 0.7f;
        shooter.columns = 2;
        Bullet_FirePattern(&shooter);
        shooter.speed1 = bullet->speed * 0.85f;
        shooter.columns = 1;
        Bullet_FirePattern(&shooter);
        M_FreeBullet(bullet);
    }
}

// Returns whether point, turned by the angle of sin and cos around origin,
// lies in the box of size around center. Only x and y count.
static bool M_IsInTurnedBox(
    const VEC3 point, const VEC3 center, const VEC3 size, const VEC3 origin,
    const float sin, const float cos)
{
    const float dx = point.x - origin.x;
    const float dy = point.y - origin.y;
    const float x = (dx * cos + dy * sin) + origin.x;
    const float y = (dy * cos - dx * sin) + origin.y;
    const float low_x = center.x - size.x / 2.0f;
    const float low_y = center.y - size.y / 2.0f;
    const float high_x = size.x / 2.0f + center.x;
    const float high_y = size.y / 2.0f + center.y;
    return !(x > high_x || x < low_x || y > high_y || y < low_y);
}

// Returns the middle of the beam of laser.
static VEC3 M_LaserMiddle(const LASER *const laser)
{
    const float length = laser->end_offset - laser->start_offset;
    return (VEC3) { length / 2.0f + laser->start_offset + laser->pos.x,
                    laser->pos.y, 0.0f };
}

static void M_TurnAtLaserEdges(const ENEMY *const enemy)
{
    for (int32_t li = 0; li < LASER_COUNT; li++) {
        const LASER *const laser = &g_Bullets.lasers[li];
        // Only lasers 0 and 1 can match.
        if (!laser->active || enemy->callbacks.timer.current % 2 != li
            || laser->state >= LASER_ENDING) {
            continue;
        }
        const VEC3 size = { laser->end_offset - laser->start_offset,
                            laser->width, 0.0f };
        const VEC3 middle = M_LaserMiddle(laser);
        float s;
        float c;
        X87_SinCos(laser->angle, &s, &c);
        for (int32_t n = 0; n < BULLET_COUNT; n++) {
            BULLET *const bullet = &g_Bullets.bullets[n];
            if (!M_IsLive(bullet)
                || bullet->sprites.flying.sprite_data == nullptr
                || !M_IsInTurnedBox(
                    bullet->pos, middle, size, laser->pos, s, c)) {
                continue;
            }
            if (bullet->pattern_state > 0) {
                bullet->pattern_state--;
            }
            if (bullet->pattern_state != 0) {
                continue;
            }
            if (bullet->speed > 0.5f) {
                bullet->speed -= 0.1f;
            }
            const float along = c * bullet->velocity.y + s * bullet->velocity.x;
            bullet->angle = Angle_AddNormalize(
                laser->angle, along >= 0.0f ? MATH_HALF_PI : -MATH_HALF_PI);
            X87_SinCosScale(
                bullet->angle, g_GameSpeed * bullet->speed, &bullet->velocity.x,
                &bullet->velocity.y);
            bullet->pattern_state = 10;
            M_SetLaserTurnSprites(bullet);
        }
    }
}

static void M_DeflectOffLasers(const ENEMY *const enemy)
{
    for (int32_t li = 0; li < LASER_COUNT; li++) {
        const LASER *const laser = &g_Bullets.lasers[li];
        if (!laser->active || enemy->callbacks.timer.current % 3 != li % 3
            || laser->state >= LASER_ENDING) {
            continue;
        }
        const VEC3 size = { laser->end_offset - laser->start_offset,
                            laser->width * 1.5f, 0.0f };
        const VEC3 middle = M_LaserMiddle(laser);
        float s;
        float c;
        X87_SinCos(laser->angle, &s, &c);
        const float nx = -s;
        const float ny = c;
        for (int32_t n = 0; n < BULLET_COUNT; n++) {
            BULLET *const bullet = &g_Bullets.bullets[n];
            if (!M_IsLive(bullet)
                || bullet->sprites.flying.sprite_data == nullptr
                || bullet->pattern_state == li + 1 || bullet->pattern_state < 0
                || !M_IsInTurnedBox(
                    bullet->pos, middle, size, laser->pos, s, c)) {
                continue;
            }
            if (M_Difficulty() < 2) {
                bullet->speed *= Rng_F32(&g_Rng) * 0.3f + 0.7f;
            } else {
                bullet->speed *= Rng_F32(&g_Rng) * 0.4f + 0.8f;
            }
            const float along =
                nx * bullet->velocity.x + ny * bullet->velocity.y;
            if (along >= 0.0f) {
                bullet->velocity.x = nx;
                bullet->velocity.y = ny;
            } else {
                bullet->velocity.x = -nx;
                bullet->velocity.y = -ny;
            }
            M_SetLaserTurnSprites(bullet);
            bullet->angle = X87_Atan2(bullet->velocity.y, bullet->velocity.x);
            // Unlike the turn at a laser's edge, not scaled by the game
            // speed.
            X87_SinCosScale(
                bullet->angle, bullet->speed, &bullet->velocity.x,
                &bullet->velocity.y);
            bullet->pattern_state = M_Difficulty() < 2 ? -1 : li + 1;
        }
    }
}

static void M_StartPetalsFalling(void)
{
    ScreenEffect_Shake(80, 8, 0);
    Effect_StartPetalsFalling();
}

static void M_SlowDown(const int32_t arg)
{
    g_GameSpeed = 1.0f / (float)arg;
    g_Stage.spell_background.sprites[0].pending_interrupt = 2;
    g_Stage.spell_background.sprites[1].pending_interrupt = 2;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        bullet->velocity = Vec3_Scale(bullet->velocity, g_GameSpeed);
        ANM_VM *const vm = &bullet->sprites.flying;
        vm->base_sprite = vm->sprite;
        if (vm->sprite >= M_SLOW_FIRST_SPRITE && vm->sprite <= M_SLOW_SPRITE) {
            Anm_SetSprite(vm, M_SLOW_SPRITE);
        }
    }
}

static void M_EndSlowDown(const int32_t arg)
{
    const float scale = 1.0f / g_GameSpeed;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        bullet->velocity = Vec3_Scale(bullet->velocity, scale);
        ANM_VM *const vm = &bullet->sprites.flying;
        if (vm->sprite >= M_SLOW_FIRST_SPRITE && vm->sprite <= M_SLOW_SPRITE) {
            Anm_SetSprite(vm, vm->base_sprite);
        }
    }
    // The original sets a flag of the app for a speed 1 / arg below 1,
    // which no script asks for.
    ASSERT(arg == 0 || arg == 1);
    g_GameSpeed = 1.0f;
    g_Stage.spell_background.sprites[0].pending_interrupt = 1;
    g_Stage.spell_background.sprites[1].pending_interrupt = 1;
}

// Fires count bullets around bullet, each of one of three small types at
// random in its colour from colors, speeding up from 0.1.
static void M_Burst(
    BULLET_SHOOTER *const shooter, const BULLET *const bullet,
    const int32_t count, const int16_t colors[3], const bool alternate_spawn,
    const int32_t arg)
{
    for (int32_t j = 0; j < count; j++) {
        shooter->pos = bullet->pos;
        shooter->pos.x += Rng_F32(&g_Rng) * 32.0f - 16.0f;
        shooter->pos.y += Rng_F32(&g_Rng) * 32.0f - 16.0f;
        switch (Rng_U16InRange(&g_Rng, 3)) {
        case 0:
            shooter->type = 0;
            shooter->color = colors[0];
            break;
        case 1:
            shooter->type = 3;
            shooter->color = colors[1];
            break;
        case 2:
            shooter->type = 7;
            shooter->color = colors[2];
            break;
        }
        if (arg == 0) {
            shooter->angle1 = Rng_F32(&g_Rng) * M_BURST_SPREAD - MATH_HALF_PI;
        } else {
            shooter->angle1 = Angle_AddNormalize(
                Rng_F32(&g_Rng) * M_BURST_SPREAD, M_QUARTER_PI);
        }
        shooter->speed1 = 0.1f;
        shooter->columns = 1;
        shooter->rows = 1;
        shooter->flags =
            alternate_spawn && (j & 1) != 0 ? BULLET_FLAG_SPAWN_1 : 0;
        shooter->aim = BULLET_AIM_FAN;
        BulletShooter_SetCommand(
            shooter, 0,
            (BULLET_COMMAND) {
                .args = { Rng_F32(&g_Rng) * 0.008f + 0.01f, 0.0f },
                .frames = 100,
                .flag = BULLET_FLAG_CURVE });
        Bullet_FirePattern(shooter);
    }
}

static void M_BurstTallBullets(const ENEMY *const enemy, const int32_t arg)
{
    static const int16_t colors[3] = { 2, 2, 1 };
    BULLET_SHOOTER shooter;
    BulletShooter_Init(&shooter);
    ScreenEffect_Flash(8, 1, COLOR_ARGB(0x50, 0xcf, 0xcf, 0xff));
    const int32_t difficulty = M_Difficulty();
    const int32_t count = difficulty == 0 ? 10
        : difficulty == 1                 ? 18
        : difficulty == 2                 ? 22
                                          : 25;
    const float reach = difficulty < 2 ? 64.0f : 48.0f;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        if (M_Height(bullet) > M_BURST_HEIGHT
            && enemy->move.pos.y - reach < bullet->pos.y
            && enemy->move.pos.y + reach > bullet->pos.y) {
            M_Burst(&shooter, bullet, count, colors, true, arg);
            M_FreeBullet(bullet);
        }
    }
}

static void M_TurnBulletsBelow(const ENEMY *const enemy)
{
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        if (bullet->pattern_state == 0 && bullet->pos.y > enemy->move.pos.y
            && bullet->pos.y < 352.0f
            && enemy->move.pos.x - 16.0f < bullet->pos.x
            && enemy->move.pos.x + 16.0f > bullet->pos.x) {
            Bullet_SetCommand(
                bullet, 0,
                (BULLET_COMMAND) {
                    .args = { -bullet->speed / 180.0f,
                              (i & 1) != 0 ? 0.05235988f : -0.05235988f },
                    .frames = 160,
                    .flag = BULLET_FLAG_CURVE });
            bullet->pattern_state = 1;
        }
    }
}

static void M_AimTurnedBullets(void)
{
    ScreenEffect_Flash(16, 1, COLOR_ARGB(0x50, 0xcf, 0xcf, 0xff));
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE || bullet->pattern_state != 1) {
            continue;
        }
        Bullet_SetCommand(
            bullet, 0,
            (BULLET_COMMAND) {
                .args = { 0.026666667f, Player_AngleFrom(bullet->pos) },
                .frames = 90,
                .flag = BULLET_FLAG_ACCELERATE });
        bullet->commands[1].flag = 0;
        bullet->pattern_state = 2;
    }
}

static bool M_IsUnturnedSprite(const BULLET *const bullet)
{
    const int16_t sprite = bullet->sprites.flying.sprite;
    return sprite >= M_UNTURNED_FIRST_SPRITE
        && sprite <= M_UNTURNED_LAST_SPRITE;
}

static void M_FireBackFromUnturned(const ENEMY *const enemy)
{
    BULLET_SHOOTER shooter;
    BulletShooter_Init(&shooter);
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        const BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE || bullet->pattern_state != 0
            || !M_IsUnturnedSprite(bullet)) {
            continue;
        }
        shooter.pos = bullet->pos;
        shooter.type = 0;
        shooter.color = 6;
        shooter.angle1 = Angle_AddNormalize(bullet->angle, MATH_PI);
        shooter.angle2 = 0.3926991f;
        shooter.speed1 = enemy->script.state.vars.floats[1];
        shooter.columns = 5;
        shooter.rows = 1;
        shooter.flags = BULLET_FLAG_SPAWN_1;
        shooter.aim = BULLET_AIM_FAN;
        Bullet_FirePattern(&shooter);
    }
}

static void M_BulletsToEnemies(const ENEMY *const enemy)
{
    ECL_VARS vars = enemy->script.state.vars;
    const int32_t sub = enemy->script.state.sub + 1;
    float theta = -MATH_PI;
    ScreenEffect_Flash(12, 1, COLOR_ARGB(0x80, 0xcf, 0xcf, 0xff));
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        if (bullet->pattern_state == 0
            && bullet->sprites.flying.sprite == M_UNTURNED_SPRITE) {
            vars.floats[0] = bullet->angle;
            vars.floats[7] = theta;
            theta += M_QUARTER_PI;
            Enemy_SpawnWithVars(sub, bullet->pos, 1, -2, 10, &vars);
            M_FreeBullet(bullet);
        } else if (M_IsUnturnedSprite(bullet)) {
            M_FreeBullet(bullet);
        }
    }
}

static void M_CountUnturned(ENEMY *const enemy)
{
    enemy->script.state.vars.ints[0] = 0;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        const BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state != BULLET_FREE && bullet->pattern_state == 0
            && bullet->sprites.flying.sprite == M_UNTURNED_SPRITE) {
            enemy->script.state.vars.ints[0]++;
        }
    }
}

static void M_PlayBossMusic(void)
{
    Sound_PlayMusic(g_Stage.data.song_paths[2]);
}

static void M_BurstTallBulletsFixed(const ENEMY *const enemy, const int32_t arg)
{
    static const int16_t colors[3] = { 4, 4, 2 };
    BULLET_SHOOTER shooter;
    BulletShooter_Init(&shooter);
    const float reach = M_Difficulty() == 2 ? 128.0f : 180.0f;
    ScreenEffect_Flash(8, 1, COLOR_ARGB(0x50, 0xcf, 0xcf, 0xff));
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        if (M_Height(bullet) > M_BURST_HEIGHT
            && enemy->move.pos.y - reach < bullet->pos.y
            && reach + enemy->move.pos.y > bullet->pos.y) {
            M_Burst(&shooter, bullet, 15, colors, false, arg);
            M_FreeBullet(bullet);
        }
    }
}

// Fires from each tall bullet above y 320 that does not turn: columns slow
// bullets in variant 0, or one fast one that turns at the player once.
static void M_FireFromTallBullets(
    const int32_t variant, const int16_t colors[2], const int16_t color_from,
    const int16_t columns, const int32_t turn_frames, const float turn_speed)
{
    BULLET_SHOOTER shooter;
    BulletShooter_Init(&shooter);
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        const BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE
            || !(
                (bullet->running & BULLET_FLAG_TURN) == 0
                && bullet->pos.y < 320.0f
                && M_Height(bullet) > M_TALL_HEIGHT)) {
            continue;
        }
        shooter.pos = bullet->pos;
        shooter.type = variant != 0 ? 1 : 3;
        shooter.color = bullet->color == color_from ? colors[0] : colors[1];
        shooter.angle1 = Rng_F32(&g_Rng) * MATH_TWO_PI - MATH_PI;
        shooter.angle2 = -MATH_PI;
        shooter.speed1 = variant != 0 ? 1.2f : 0.8f;
        shooter.columns = variant != 0 ? 1 : columns;
        shooter.rows = 1;
        shooter.flags = BULLET_FLAG_SOUND | BULLET_FLAG_SPAWN_3;
        shooter.aim = BULLET_AIM_RING;
        shooter.bullet_sound = 0x19;
        if (variant != 0) {
            BulletShooter_SetCommand(
                &shooter, 0,
                (BULLET_COMMAND) { .args = { 0.0f, turn_speed },
                                   .frames = turn_frames,
                                   .count = 1,
                                   .flag = BULLET_FLAG_TURN_AT_PLAYER });
        }
        Bullet_FirePattern(&shooter);
    }
}

void Ecl_RunPattern(ENEMY *const enemy, const ECL_FUNC func, const int32_t arg)
{
    const int32_t frame = enemy->callbacks.timer.current;
    switch (func) {
    case ECL_FUNC_WARP_TO_BOSS:
        M_WarpToBoss(enemy, arg);
        break;
    case ECL_FUNC_SPIN_BULLETS:
        M_SpinBullets(arg);
        break;
    case ECL_FUNC_FANS_FROM_NEAR_BULLETS:
        M_FansFromNearBullets(enemy, arg);
        break;
    case ECL_FUNC_NOP:
        break;
    case ECL_FUNC_TALL_BULLET_TO_EFFECT:
        M_TallBulletToEffect(enemy);
        break;
    case ECL_FUNC_COPY_BOSS_CIRCLE:
        M_CopyBossCircle(enemy);
        break;
    case ECL_FUNC_FANS_BACK_FROM_BULLETS:
        M_FansBackFromBullets(arg);
        break;
    case ECL_FUNC_TURN_AT_LASER_EDGES:
        M_TurnAtLaserEdges(enemy);
        break;
    case ECL_FUNC_DEFLECT_OFF_LASERS:
        M_DeflectOffLasers(enemy);
        break;
    case ECL_FUNC_START_PETALS_FALLING:
        M_StartPetalsFalling();
        break;
    case ECL_FUNC_SLOW_DOWN:
        M_SlowDown(arg);
        break;
    case ECL_FUNC_END_SLOW_DOWN:
        M_EndSlowDown(arg);
        break;
    case ECL_FUNC_BURST_TALL_BULLETS:
        M_BurstTallBullets(enemy, arg);
        break;
    case ECL_FUNC_TURN_BULLETS_BELOW:
        M_TurnBulletsBelow(enemy);
        break;
    case ECL_FUNC_AIM_TURNED_BULLETS:
        M_AimTurnedBullets();
        break;
    case ECL_FUNC_FLASH:
        ScreenEffect_Flash(arg, 1, COLOR_ARGB(0xd0, 0xcf, 0xcf, 0xff));
        break;
    case ECL_FUNC_FIRE_BACK_FROM_UNTURNED:
        M_FireBackFromUnturned(enemy);
        break;
    case ECL_FUNC_BULLETS_TO_ENEMIES:
        M_BulletsToEnemies(enemy);
        break;
    case ECL_FUNC_COUNT_UNTURNED:
        M_CountUnturned(enemy);
        break;
    case ECL_FUNC_FADE_OUT_MUSIC:
        Sound_FadeOutMusic(3.0f);
        break;
    case ECL_FUNC_PLAY_BOSS_MUSIC:
        M_PlayBossMusic();
        break;
    case ECL_FUNC_BURST_TALL_BULLETS_FIXED:
        M_BurstTallBulletsFixed(enemy, arg);
        break;
    case ECL_FUNC_FIRE_FROM_TALL_BULLETS:
        if (frame % 3 != 0) {
            static const int16_t colors[2] = { 6, 2 };
            M_FireFromTallBullets(frame % 2, colors, 1, 2, 60, 3.1f);
        }
        break;
    case ECL_FUNC_FIRE_FROM_TALL_BULLETS_2:
        if (frame % 3 != 2) {
            static const int16_t colors[2] = { 10, 13 };
            M_FireFromTallBullets(frame % 3, colors, 2, 1, 40, 2.9f);
        }
        break;
    default:
        ASSERT(false);
        break;
    }
}
