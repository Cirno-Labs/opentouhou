// SPDX-License-Identifier: GPL-3.0-only

#include "th06/bullet.h"
#include "th06/ascii.h"

#include "th06/angle.h"
#include "th06/anm.h"
#include "th06/chain.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"
#include "th06/scene.h"
#include "th06/sound.h"
#include "th06/timer.h"

#include "core/base/assert.h"
#include "core/base/utils.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#define M_CALC_PRIORITY 11
#define M_DRAW_PRIORITY 9
#define M_ANM_SLOT 6
#define M_ANM_SLOT_EX 7
#define M_FIRST_SPRITE 0x200
#define M_FIRST_SPRITE_EX 0x29a
#define M_FIRST_LASER_SCRIPT 0x209
#define M_LASER_ORIGIN_SPRITE 0x28c
#define M_COLOR_OFFSETS_16 0
#define M_COLOR_OFFSETS_32 16
#define M_PI 3.1415927f
#define M_HALF_PI 1.5707964f
#define M_TWO_PI 6.2831855f
#define M_OUT_OF_BOUNDS_FRAMES 0x100
#define M_START_FAST_FRAMES 16
#define M_LASER_FADE_FRAMES 30
#define M_LASER_GRAZE_INTERVAL 12
#define M_LASER_MAX_START 640.0f
#define M_LASER_ITEM_SPACING 32.0f
#define M_FIRST_BONUS 2000
#define M_BONUS_STEP 10

typedef enum {
    M_SCRIPT_FLYING = 0,
    M_SCRIPT_SPAWNING = 1,
    M_SCRIPT_FADING = 4,
    M_SCRIPT_COUNT = 5,
} M_SCRIPT;

static const int32_t m_TypeScripts[BULLET_TYPE_COUNT][M_SCRIPT_COUNT] = {
    { 512, 526, 527, 528, 523 }, { 513, 529, 530, 531, 524 },
    { 514, 529, 530, 531, 524 }, { 515, 529, 530, 531, 524 },
    { 516, 529, 530, 531, 524 }, { 517, 529, 530, 531, 524 },
    { 518, 532, 532, 532, 525 }, { 519, 532, 532, 532, 525 },
    { 520, 532, 532, 532, 525 }, { 666, 668, 668, 668, 667 },
};

// Holds the sprite offsets of the colors: 16 from M_COLOR_OFFSETS_16 for
// sprites up to 16 high, then 8 from M_COLOR_OFFSETS_32 for sprites up to
// 32 high. A color past the end of its table reads on into the next one,
// and then into the 4 boss timer colors and the 5 extend scores that
// follow. Those offsets reach past every sprite, so they leave the sprite
// unchanged.
static const int32_t m_ColorOffsets[] = {
    0,          1,          1,          1,          1,        2,
    2,          2,          2,          3,          3,        3,
    4,          4,          4,          0,          0,        1,
    1,          2,          2,          3,          4,        0,
    0x00a0d0ff, 0x00a080ff, 0x00e080c0, 0x00ff4040, 10000000, 20000000,
    40000000,   60000000,   1900000000,
};

BULLET_MANAGER g_Bullets;

static BULLET_SPRITES m_Types[BULLET_TYPE_COUNT];
static LASER m_SpareLaser;
static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;

// Returns v / f as D3DX does: multiplied by the reciprocal of f.
static VEC3 M_Div(const VEC3 v, const float f)
{
    return Vec3_Scale(v, 1.0f / f);
}

static void M_SetVelocity(VEC3 *const v, const float angle, const float speed)
{
    X87_SinCosScale(angle, speed, &v->x, &v->y);
}

// Shows sprite plus the offset of color in the table from first. Leaves
// vm unchanged for an index outside every table.
static void M_SetOffsetSprite(
    ANM_VM *const vm, const int32_t sprite, const int32_t first,
    const int32_t color)
{
    const int32_t index = first + color;
    if (index >= 0 && index < (int32_t)ARRAY_SIZE(m_ColorOffsets)) {
        Anm_SetSprite(vm, sprite + m_ColorOffsets[index]);
    }
}

// Shows the sprite of color on vm, a spawn or fade sprite of bullet. The
// height of the flying sprite selects the offsets.
static void M_SetColorSprite(
    const BULLET *const bullet, ANM_VM *const vm, const int32_t color)
{
    const float height = bullet->sprites.flying.sprite_data->height;
    if (height <= 16.0f) {
        M_SetOffsetSprite(vm, vm->sprite, M_COLOR_OFFSETS_16, color);
    } else if (height <= 32.0f) {
        if (bullet->sprites.flying.script_index != 0x207) {
            M_SetOffsetSprite(vm, vm->sprite, M_COLOR_OFFSETS_32, color);
        } else {
            Anm_SetSprite(vm, vm->sprite + 1);
        }
    } else {
        Anm_SetSprite(vm, vm->sprite + color);
    }
}

static BULLET *M_FindFree(void)
{
    BULLET *bullet = &g_Bullets.bullets[g_Bullets.next_bullet];
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        g_Bullets.next_bullet++;
        if (g_Bullets.next_bullet >= BULLET_COUNT) {
            g_Bullets.next_bullet = 0;
        }
        if (bullet->state == BULLET_FREE) {
            return bullet;
        }
        bullet++;
        if (g_Bullets.next_bullet == 0) {
            bullet = g_Bullets.bullets;
        }
    }
    return nullptr;
}

// Returns the angle of the bullet at column col and row row, and sets
// *speed to its speed.
static float M_Aim(
    const BULLET_SHOOTER *const shooter, const int32_t col, const int32_t row,
    const float aim_angle, float *const speed)
{
    float angle = 0.0f;
    switch (shooter->aim) {
    case BULLET_AIM_FAN_AT_PLAYER:
    case BULLET_AIM_FAN:
        if ((shooter->columns & 1) != 0) {
            angle += (float)((col + 1) / 2) * shooter->angle2;
        } else {
            angle +=
                (float)(col / 2) * shooter->angle2 + shooter->angle2 * 0.5f;
        }
        if ((col & 1) != 0) {
            angle *= -1.0f;
        }
        if (shooter->aim == BULLET_AIM_FAN_AT_PLAYER) {
            angle += aim_angle;
        }
        angle += shooter->angle1;
        break;
    case BULLET_AIM_RING_AT_PLAYER:
        angle += aim_angle;
        [[fallthrough]];
    case BULLET_AIM_RING:
        angle += (float)col * M_TWO_PI / (float)shooter->columns;
        angle += (float)row * shooter->angle2 + shooter->angle1;
        break;
    case BULLET_AIM_RING_OFFSET_AT_PLAYER:
        angle += aim_angle;
        [[fallthrough]];
    case BULLET_AIM_RING_OFFSET:
        angle += M_PI / (float)shooter->columns;
        angle += (float)col * M_TWO_PI / (float)shooter->columns;
        angle += shooter->angle1;
        break;
    case BULLET_AIM_RANDOM_ANGLE:
        angle = Rng_F32(&g_Rng) * (shooter->angle1 - shooter->angle2)
            + shooter->angle2;
        break;
    case BULLET_AIM_RING_RANDOM_SPEED:
        *speed = Rng_F32(&g_Rng) * (shooter->speed1 - shooter->speed2)
            + shooter->speed2;
        angle += (float)col * M_TWO_PI / (float)shooter->columns;
        angle += (float)row * shooter->angle2 + shooter->angle1;
        break;
    case BULLET_AIM_RANDOM:
        angle = Rng_F32(&g_Rng) * (shooter->angle1 - shooter->angle2)
            + shooter->angle2;
        *speed = Rng_F32(&g_Rng) * (shooter->speed1 - shooter->speed2)
            + shooter->speed2;
        break;
    default:
        break;
    }
    return angle;
}

static void M_SetFlags(
    BULLET *const bullet, const BULLET_SHOOTER *const shooter,
    const float angle, const float speed)
{
    if ((bullet->flags & BULLET_FLAG_ACCELERATE) != 0) {
        // Takes the angle before it is normalized.
        const float accel_angle =
            shooter->ex_floats[1] <= -999.0f ? angle : shooter->ex_floats[1];
        M_SetVelocity(
            &bullet->change.accel, accel_angle, shooter->ex_floats[0]);
        bullet->change.frames =
            shooter->ex_ints[0] > 0 ? shooter->ex_ints[0] : 99999;
        bullet->change.accel.z = 0.0f;
    } else if ((bullet->flags & BULLET_FLAG_CURVE) != 0) {
        bullet->change.speed = shooter->ex_floats[0];
        bullet->change.angle = shooter->ex_floats[1];
        bullet->change.frames = shooter->ex_ints[0];
    }
    if ((bullet->flags
         & (BULLET_FLAG_TURN | BULLET_FLAG_TURN_AT_PLAYER
            | BULLET_FLAG_TURN_TO))
        != 0) {
        bullet->turn.angle = shooter->ex_floats[0];
        bullet->turn.speed =
            shooter->ex_floats[1] >= 0.0f ? shooter->ex_floats[1] : speed;
        bullet->turn.interval = shooter->ex_ints[0];
        bullet->turn.max = shooter->ex_ints[1];
        bullet->turn.count = 0;
    }
    if ((bullet->flags & (BULLET_FLAG_BOUNCE | BULLET_FLAG_BOUNCE_NO_BOTTOM))
        != 0) {
        bullet->turn.speed =
            shooter->ex_floats[0] >= 0.0f ? shooter->ex_floats[0] : speed;
        bullet->turn.max = shooter->ex_ints[0];
        bullet->turn.count = 0;
    }
}

// Returns whether a bullet was free for it.
static bool M_Spawn(
    const BULLET_SHOOTER *const shooter, const int32_t col, const int32_t row,
    const float aim_angle)
{
    BULLET *const bullet = M_FindFree();
    if (bullet == nullptr) {
        return false;
    }
    ASSERT(shooter->type >= 0 && shooter->type < BULLET_TYPE_COUNT);
    const BULLET_SPRITES *const type = &m_Types[shooter->type];
    float speed = shooter->speed1
        - (shooter->speed1 - shooter->speed2) * (float)row
            / (float)shooter->rows;
    const float angle = M_Aim(shooter, col, row, aim_angle, &speed);
    bullet->state = BULLET_FLYING;
    bullet->speed = speed;
    bullet->angle = Angle_AddNormalize(angle, 0.0f);
    bullet->pos = shooter->pos;
    bullet->pos.z = 0.1f;
    M_SetVelocity(&bullet->velocity, bullet->angle, speed);
    bullet->flags = (uint16_t)shooter->flags;
    bullet->color = shooter->color;
    bullet->sprites.flying = type->flying;
    bullet->sprites.fading = type->fading;
    Anm_RenewPresentation(&bullet->sprites.flying);
    Anm_RenewPresentation(&bullet->sprites.fading);
    bullet->sprites.hitbox_size = type->hitbox_size;
    static const BULLET_FLAG spawn_flags[BULLET_SPAWN_SPRITE_COUNT] = {
        BULLET_FLAG_SPAWN_1,
        BULLET_FLAG_SPAWN_2,
        BULLET_FLAG_SPAWN_3,
    };
    for (int32_t i = 0; i < BULLET_SPAWN_SPRITE_COUNT; i++) {
        if ((bullet->flags & spawn_flags[i]) != 0) {
            ANM_VM *const vm = &bullet->sprites.spawning[i];
            *vm = type->spawning[i];
            Anm_RenewPresentation(vm);
            M_SetColorSprite(bullet, vm, shooter->color);
            bullet->state = (BULLET_STATE)(BULLET_SPAWNING_1 + i);
            break;
        }
    }
    Anm_SetSprite(
        &bullet->sprites.flying,
        bullet->sprites.flying.sprite + shooter->color);
    M_SetColorSprite(bullet, &bullet->sprites.fading, shooter->color);
    M_SetFlags(bullet, shooter, angle, speed);
    return true;
}

// Returns the speed of a turning bullet between two turns: it falls from
// speed towards 0 over the interval.
static float M_TurnSpeed(const BULLET *const bullet)
{
    return bullet->speed
        - ((float)bullet->timer.current
           - (float)(bullet->turn.interval * bullet->turn.count))
        * bullet->speed / (float)bullet->turn.interval;
}

// Runs the turn of flag. Between two turns, slows the bullet and returns
// false. At a turn, counts it and returns true without moving the bullet.
static bool M_Turn(BULLET *const bullet, const BULLET_FLAG flag)
{
    if (bullet->timer.current
        < bullet->turn.interval * (bullet->turn.count + 1)) {
        M_SetVelocity(&bullet->velocity, bullet->angle, M_TurnSpeed(bullet));
        return false;
    }
    bullet->turn.count++;
    if (bullet->turn.count >= bullet->turn.max) {
        bullet->flags &= (uint16_t)~flag;
    }
    return true;
}

static void M_EndTurn(BULLET *const bullet)
{
    bullet->speed = bullet->turn.speed;
    M_SetVelocity(&bullet->velocity, bullet->angle, bullet->speed);
}

static void M_Bounce(BULLET *const bullet, const BULLET_FLAG flag)
{
    const ANM_SPRITE *const sprite = bullet->sprites.flying.sprite_data;
    if (GameManager_IsInPlayArea(
            bullet->pos.x, bullet->pos.y, sprite->width, sprite->height)) {
        return;
    }
    if (bullet->pos.x < 0.0f || bullet->pos.x >= 384.0f) {
        bullet->angle = -bullet->angle - M_PI;
        bullet->angle = Angle_AddNormalize(bullet->angle, 0.0f);
    }
    if (bullet->pos.y < 0.0f
        || (flag == BULLET_FLAG_BOUNCE && bullet->pos.y >= 448.0f)) {
        bullet->angle = -bullet->angle;
    }
    M_EndTurn(bullet);
    bullet->turn.count++;
    if (bullet->turn.count >= bullet->turn.max) {
        bullet->flags &= (uint16_t)~flag;
    }
}

static void M_ApplyChange(BULLET *const bullet)
{
    if ((bullet->flags & BULLET_FLAG_START_FAST) != 0) {
        if (bullet->timer.current <= M_START_FAST_FRAMES) {
            const float extra =
                5.0f - (float)bullet->timer.current * 5.0f / 16.0f;
            M_SetVelocity(
                &bullet->velocity, bullet->angle, extra + bullet->speed);
        } else {
            bullet->flags ^= BULLET_FLAG_START_FAST;
        }
    } else if ((bullet->flags & BULLET_FLAG_ACCELERATE) != 0) {
        if (bullet->timer.current >= bullet->change.frames) {
            bullet->flags &= (uint16_t)~BULLET_FLAG_ACCELERATE;
        } else {
            bullet->velocity = Vec3_Add(bullet->velocity, bullet->change.accel);
            bullet->angle = X87_Atan2(bullet->velocity.y, bullet->velocity.x);
        }
    } else if ((bullet->flags & BULLET_FLAG_CURVE) != 0) {
        if (bullet->timer.current >= bullet->change.frames) {
            bullet->flags &= (uint16_t)~BULLET_FLAG_CURVE;
        } else {
            bullet->angle =
                Angle_AddNormalize(bullet->angle, bullet->change.angle);
            bullet->speed = bullet->change.speed + bullet->speed;
            M_SetVelocity(&bullet->velocity, bullet->angle, bullet->speed);
        }
    }
}

// Leaves the angle of a turn unnormalized, except for a bounce off a side.
static void M_ApplyTurn(BULLET *const bullet)
{
    if ((bullet->flags & BULLET_FLAG_TURN) != 0) {
        if (M_Turn(bullet, BULLET_FLAG_TURN)) {
            bullet->angle += bullet->turn.angle;
            M_EndTurn(bullet);
        }
    } else if ((bullet->flags & BULLET_FLAG_TURN_TO) != 0) {
        if (M_Turn(bullet, BULLET_FLAG_TURN_TO)) {
            bullet->angle = bullet->turn.angle;
            M_EndTurn(bullet);
        }
    } else if ((bullet->flags & BULLET_FLAG_TURN_AT_PLAYER) != 0) {
        if (M_Turn(bullet, BULLET_FLAG_TURN_AT_PLAYER)) {
            bullet->angle = Player_AngleFrom(bullet->pos) + bullet->turn.angle;
            M_EndTurn(bullet);
        }
    } else if ((bullet->flags & BULLET_FLAG_BOUNCE) != 0) {
        M_Bounce(bullet, BULLET_FLAG_BOUNCE);
    } else if ((bullet->flags & BULLET_FLAG_BOUNCE_NO_BOTTOM) != 0) {
        M_Bounce(bullet, BULLET_FLAG_BOUNCE_NO_BOTTOM);
    }
}

static void M_TestPlayer(BULLET *const bullet)
{
    if (!bullet->grazed) {
        const PLAYER_CONTACT graze =
            Player_CheckGraze(bullet->pos, bullet->sprites.hitbox_size);
        if (graze == PLAYER_CONTACT_CLEARED) {
            bullet->state = BULLET_FADING;
            Item_Spawn(bullet->pos, ITEM_STAR, ITEM_HOMING);
            return;
        }
        if (graze != PLAYER_CONTACT_BOX) {
            return;
        }
        bullet->grazed = true;
    }
    const PLAYER_CONTACT hit =
        Player_CheckHit(bullet->pos, bullet->sprites.hitbox_size);
    if (hit != PLAYER_CONTACT_NONE) {
        bullet->state = BULLET_FADING;
        if (hit == PLAYER_CONTACT_CLEARED) {
            Item_Spawn(bullet->pos, ITEM_STAR, ITEM_HOMING);
        }
    }
}

// Moves a flying bullet. Returns false when it left the play area for
// good, which frees it.
static bool M_Fly(BULLET *const bullet)
{
    const uint16_t turns = BULLET_FLAG_TURN | BULLET_FLAG_TURN_AT_PLAYER
        | BULLET_FLAG_TURN_TO | BULLET_FLAG_BOUNCE
        | BULLET_FLAG_BOUNCE_NO_BOTTOM;
    if (bullet->flags != 0) {
        M_ApplyChange(bullet);
        M_ApplyTurn(bullet);
    }
    bullet->pos = Vec3_Add(bullet->pos, bullet->velocity);
    const ANM_SPRITE *const sprite = bullet->sprites.flying.sprite_data;
    if (!GameManager_IsInPlayArea(
            bullet->pos.x, bullet->pos.y, sprite->width, sprite->height)) {
        if ((bullet->flags & turns) == 0 && bullet->out_of_bounds_frames == 0) {
            return false;
        }
        bullet->out_of_bounds_frames++;
        if (bullet->out_of_bounds_frames >= M_OUT_OF_BOUNDS_FRAMES) {
            return false;
        }
    } else {
        bullet->out_of_bounds_frames = 0;
    }
    M_TestPlayer(bullet);
    Anm_Step(&bullet->sprites.flying);
    return true;
}

// Runs bullet for this frame. Returns false when it is freed.
static bool M_UpdateBullet(BULLET *const bullet)
{
    static const float spawn_divisors[BULLET_SPAWN_SPRITE_COUNT] = {
        2.0f,
        2.5f,
        3.0f,
    };
    switch (bullet->state) {
    case BULLET_SPAWNING_1:
    case BULLET_SPAWNING_2:
    case BULLET_SPAWNING_3: {
        const int32_t i = bullet->state - BULLET_SPAWNING_1;
        bullet->pos =
            Vec3_Add(bullet->pos, M_Div(bullet->velocity, spawn_divisors[i]));
        if (!Anm_Step(&bullet->sprites.spawning[i])) {
            return true;
        }
        bullet->state = BULLET_FLYING;
        Timer_Set(&bullet->timer, 0);
        return M_Fly(bullet);
    }
    case BULLET_FLYING:
        return M_Fly(bullet);
    case BULLET_FADING:
        bullet->pos = Vec3_Add(bullet->pos, M_Div(bullet->velocity, 2.0f));
        return !Anm_Step(&bullet->sprites.fading);
    default:
        return true;
    }
}

// Tests laser against the player: the box of size centred on pos, in the
// frame of the laser.
static void M_HitLaser(
    const LASER *const laser, const VEC3 pos, const VEC3 size)
{
    Player_CheckLaserHit(
        pos, size, laser->pos, laser->angle,
        laser->timer.current % M_LASER_GRAZE_INTERVAL == 0);
}

// Sets the alpha of the beam from the timer over start_frames. The ending
// uses start_frames too, so the beam fades in again as it ends.
static void M_FadeLaser(LASER *const laser)
{
    int32_t alpha = X87_Ftol(
        (float)laser->timer.current * 255.0f / (float)laser->start_frames);
    if (alpha > 0xff) {
        alpha = 0xff;
    }
    laser->beam.color = (uint32_t)alpha << 24;
}

// Runs laser for this frame. Returns false when it ended, which skips the
// rest of its frame.
static bool M_UpdateLaser(LASER *const laser)
{
    laser->end_offset = laser->speed + laser->end_offset;
    if (laser->end_offset - laser->start_offset > laser->max_length) {
        laser->start_offset = laser->end_offset - laser->max_length;
    }
    if (laser->start_offset < 0.0f) {
        laser->start_offset = 0.0f;
    }
    VEC3 size = {
        .x = laser->end_offset - laser->start_offset,
        .y = laser->width / 2.0f,
    };
    const VEC3 center = {
        .x = (laser->end_offset - laser->start_offset) / 2.0f
            + laser->start_offset + laser->pos.x,
        .y = laser->pos.y,
    };
    const ANM_SPRITE *const sprite = laser->beam.sprite_data;
    if (sprite != nullptr) {
        laser->beam.scale_x = laser->width / sprite->width;
        laser->beam.scale_y =
            (laser->end_offset - laser->start_offset) / sprite->height;
    }
    laser->beam.rotation.z = M_HALF_PI - laser->angle;
    switch (laser->state) {
    case LASER_STARTING:
        if ((laser->flags & BULLET_FLAG_START_FAST) != 0) {
            M_FadeLaser(laser);
        } else {
            const int32_t fade_frames =
                laser->start_frames > M_LASER_FADE_FRAMES ? M_LASER_FADE_FRAMES
                                                          : laser->start_frames;
            const float width =
                laser->start_frames - fade_frames < laser->timer.current
                ? (float)laser->timer.current * laser->width
                    / (float)laser->start_frames
                : 1.2f;
            laser->beam.scale_x = width / 16.0f;
            size.x = width / 2.0f;
        }
        if (laser->timer.current >= laser->hit_start_frame) {
            M_HitLaser(laser, center, size);
        }
        if (laser->timer.current < laser->start_frames) {
            break;
        }
        Timer_Set(&laser->timer, 0);
        laser->state = LASER_FIRING;
        // Tests the player a second time in the frame that starts firing.
        [[fallthrough]];
    case LASER_FIRING:
        M_HitLaser(laser, center, size);
        if (laser->timer.current < laser->duration_frames) {
            break;
        }
        Timer_Set(&laser->timer, 0);
        laser->state = LASER_ENDING;
        if (laser->end_frames == 0) {
            laser->active = false;
            return false;
        }
        [[fallthrough]];
    case LASER_ENDING:
        if ((laser->flags & BULLET_FLAG_START_FAST) != 0) {
            M_FadeLaser(laser);
        } else if (laser->end_frames > 0) {
            const float width = laser->width
                - (float)laser->timer.current * laser->width
                    / (float)laser->end_frames;
            laser->beam.scale_x = width / 16.0f;
            size.x = width / 2.0f;
        }
        if (laser->timer.current < laser->end_hit_frames) {
            M_HitLaser(laser, center, size);
        }
        if (laser->timer.current < laser->end_frames) {
            break;
        }
        laser->active = false;
        return false;
    }
    if (laser->start_offset >= M_LASER_MAX_START) {
        laser->active = false;
    }
    return true;
}

static CHAIN_STATUS M_Update(void *const arg)
{
    if (g_GameManager.mode.frozen) {
        return CHAIN_CONTINUE;
    }
    Item_Update();
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        if (!M_UpdateBullet(bullet)) {
            *bullet = (BULLET) {};
            continue;
        }
        Timer_Tick(&bullet->timer);
    }
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        LASER *const laser = &g_Bullets.lasers[i];
        if (!laser->active || !M_UpdateLaser(laser)) {
            continue;
        }
        Timer_Tick(&laser->timer);
        Anm_Step(&laser->beam);
    }
    return CHAIN_CONTINUE;
}

static void M_DrawBullet(BULLET *const bullet, const ANM_DRAW *const draw)
{
    ANM_VM *vm = &bullet->sprites.flying;
    if (bullet->state >= BULLET_SPAWNING_1
        && bullet->state <= BULLET_SPAWNING_3) {
        vm = &bullet->sprites.spawning[bullet->state - BULLET_SPAWNING_1];
    } else if (bullet->state == BULLET_FADING) {
        vm = &bullet->sprites.fading;
    }
    vm->pos = (VEC3) { bullet->pos.x, bullet->pos.y, 0.0f };
    vm->color = (vm->color & 0xff000000u) | 0xffffffu;
    if (vm->auto_rotate != 0) {
        vm->rotation.z = 1.5707964f - bullet->angle;
    }
    Anm_Draw3DRounded(vm, draw);
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    ANM_DRAW draw = GameManager_GetDrawContext();
    draw.interpolate = true;
    draw.depth_test = GFX_DEPTH_ALWAYS;
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        LASER *const laser = &g_Bullets.lasers[i];
        if (!laser->active) {
            continue;
        }
        float sine, cosine;
        X87_SinCos(laser->angle, &sine, &cosine);
        const float length = (laser->end_offset - laser->start_offset) / 2.0f
            + laser->start_offset;
        laser->beam.pos = (VEC3) { cosine * length + laser->pos.x,
                                   sine * length + laser->pos.y, 0.0f };
        Anm_Draw3D(&laser->beam, &draw);
        if (laser->start_offset < 16.0f || laser->speed == 0.0f) {
            laser->origin.pos =
                (VEC3) { cosine * laser->start_offset + laser->pos.x,
                         sine * laser->start_offset + laser->pos.y, 0.0f };
            laser->origin.color = (laser->beam.color & 0xffffffu) | 0xff000000u;
            laser->origin.color_op = true;
            laser->origin.scale_x =
                laser->width / 10.0f * ((16.0f - laser->start_offset) / 16.0f);
            laser->origin.scale_y = laser->origin.scale_x;
            if (laser->origin.scale_y < 0.0f) {
                laser->origin.scale_x = laser->width / 10.0f;
                laser->origin.scale_y = laser->origin.scale_x;
            }
            Anm_Draw3D(&laser->origin, &draw);
        }
    }
    Item_Draw();
    for (int32_t pass = 0; pass < 4; pass++) {
        for (int32_t i = 0; i < BULLET_COUNT; i++) {
            BULLET *const bullet = &g_Bullets.bullets[i];
            const ANM_SPRITE *const sprite = bullet->sprites.flying.sprite_data;
            if (bullet->state == BULLET_FREE || sprite == nullptr) {
                continue;
            }
            const int16_t script = bullet->sprites.flying.script_index;
            const bool special = script == 0x201 || script == 0x203;
            if ((pass == 0 && sprite->height > 16.0f)
                || (pass == 1 && sprite->height == 16.0f && special)
                || (pass == 2 && sprite->height == 16.0f && !special)
                || (pass == 3 && sprite->height == 8.0f)) {
                M_DrawBullet(bullet, &draw);
            }
        }
    }
    return CHAIN_CONTINUE;
}

static void M_SetHitbox(BULLET_SPRITES *const type, const int32_t script)
{
    const float height = type->flying.sprite_data->height;
    float size;
    if (height <= 8.0f) {
        size = 4.0f;
    } else if (height <= 16.0f) {
        switch (script) {
        case 0x202:
        case 0x205:
            size = 4.0f;
            break;
        case 0x204:
            size = 5.0f;
            break;
        default:
            size = 6.0f;
            break;
        }
    } else if (height <= 32.0f) {
        switch (script) {
        case 0x207:
            size = 11.0f;
            break;
        case 0x208:
            size = 9.0f;
            break;
        default:
            size = 16.0f;
            break;
        }
    } else {
        size = 32.0f;
    }
    type->hitbox_size.x = size;
    type->hitbox_size.y = size;
}

static RESULT M_Added(void *const arg)
{
    if (Scene_GetRequested() != SCENE_NEXT_STAGE) {
        MUST(Anm_Load(M_ANM_SLOT, "data/etama3.anm", M_FIRST_SPRITE));
        MUST(Anm_Load(M_ANM_SLOT_EX, "data/etama4.anm", M_FIRST_SPRITE_EX));
    }
    for (int32_t i = 0; i < BULLET_TYPE_COUNT; i++) {
        BULLET_SPRITES *const type = &m_Types[i];
        const int32_t *const scripts = m_TypeScripts[i];
        Anm_StartScript(&type->flying, scripts[M_SCRIPT_FLYING]);
        for (int32_t j = 0; j < BULLET_SPAWN_SPRITE_COUNT; j++) {
            Anm_StartScript(&type->spawning[j], scripts[M_SCRIPT_SPAWNING + j]);
        }
        Anm_StartScript(&type->fading, scripts[M_SCRIPT_FADING]);
        type->flying.base_sprite = type->flying.sprite;
        FAIL_IF(
            type->flying.sprite_data == nullptr,
            "bullet script %d shows no sprite", scripts[M_SCRIPT_FLYING]);
        M_SetHitbox(type, scripts[M_SCRIPT_FLYING]);
    }
    Item_Reset();
    return OK;
}

static void M_Deleted(void *const arg)
{
    if (Scene_GetRequested() != SCENE_NEXT_STAGE) {
        Anm_Release(M_ANM_SLOT);
        Anm_Release(M_ANM_SLOT_EX);
    }
}

// Clears every bullet and laser. With bonus, scores the bullets as
// Bullet_ClearAllWithBonus does and returns the total.
static int32_t M_ClearAll(
    const bool to_items, const bool bonus, const int32_t max_bonus)
{
    int32_t total = 0;
    int32_t score = M_FIRST_BONUS;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bonus) {
            // Scores a bullet that already fades too, and gives the first
            // bullet the full first bonus even above max_bonus.
            if (bullet->state == BULLET_FREE) {
                continue;
            }
            if (to_items) {
                Item_Spawn(bullet->pos, ITEM_STAR, ITEM_HOMING);
            }
            AsciiText_AddPopup(
                bullet->pos, score,
                score >= max_bonus ? 0xffffff00u : 0xffffffffu);
            total += score;
            score += M_BONUS_STEP;
            if (score > max_bonus) {
                score = max_bonus;
            }
            bullet->state = BULLET_FADING;
            continue;
        }
        if (bullet->state == BULLET_FREE || bullet->state == BULLET_FADING) {
            continue;
        }
        if (to_items) {
            Item_Spawn(bullet->pos, ITEM_STAR, ITEM_HOMING);
            *bullet = (BULLET) {};
        } else {
            bullet->state = BULLET_FADING;
        }
    }
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        LASER *const laser = &g_Bullets.lasers[i];
        if (!laser->active) {
            continue;
        }
        if (laser->state < LASER_ENDING) {
            laser->state = LASER_ENDING;
            Timer_Set(&laser->timer, 0);
            if (to_items) {
                if (bonus) {
                    Item_Spawn(laser->pos, ITEM_STAR, ITEM_HOMING);
                }
                float s;
                float c;
                X87_SinCos(laser->angle, &s, &c);
                for (float t = laser->start_offset; laser->end_offset > t;
                     t += M_LASER_ITEM_SPACING) {
                    const VEC3 pos = {
                        c * t + laser->pos.x,
                        s * t + laser->pos.y,
                        0.0f,
                    };
                    Item_Spawn(pos, ITEM_STAR, ITEM_HOMING);
                }
            }
        }
        laser->end_hit_frames = 0;
    }
    return total;
}

RESULT Bullet_Register(void)
{
    g_Bullets = (BULLET_MANAGER) {};
    for (int32_t i = 0; i < BULLET_TYPE_COUNT; i++) {
        m_Types[i] = (BULLET_SPRITES) {};
    }
    m_SpareLaser = (LASER) {};
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, nullptr);
    return Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY);
}

void Bullet_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
}

void Bullet_FirePattern(const BULLET_SHOOTER *const shooter)
{
    const float aim_angle = Player_AngleFrom(shooter->pos);
    bool spawning = true;
    for (int32_t row = 0; spawning && row < shooter->rows; row++) {
        for (int32_t col = 0; spawning && col < shooter->columns; col++) {
            spawning = M_Spawn(shooter, col, row, aim_angle);
        }
    }
    if ((shooter->flags & BULLET_FLAG_SOUND) != 0) {
        Sound_Queue(shooter->sound);
    }
}

LASER *Bullet_SpawnLaser(const BULLET_SHOOTER *const shooter)
{
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        LASER *const laser = &g_Bullets.lasers[i];
        if (laser->active) {
            continue;
        }
        Anm_StartScript(&laser->beam, shooter->type + M_FIRST_LASER_SCRIPT);
        Anm_SetSprite(&laser->beam, laser->beam.sprite + shooter->color);
        Anm_InitVm(&laser->origin);
        M_SetOffsetSprite(
            &laser->origin, M_LASER_ORIGIN_SPRITE, M_COLOR_OFFSETS_16,
            shooter->color);
        laser->origin.blend_add = true;
        laser->pos = shooter->pos;
        laser->active = true;
        laser->angle = shooter->angle1;
        if (shooter->aim == BULLET_AIM_FAN_AT_PLAYER) {
            laser->angle += Player_AngleFrom(shooter->pos);
        }
        laser->flags = (uint16_t)shooter->flags;
        Timer_Set(&laser->timer, 0);
        laser->start_offset = shooter->ex_floats[0];
        laser->end_offset = shooter->ex_floats[1];
        laser->max_length = shooter->ex_floats[2];
        laser->width = shooter->ex_floats[3];
        laser->speed = shooter->speed1;
        laser->start_frames = shooter->ex_ints[0];
        laser->duration_frames = shooter->ex_ints[1];
        laser->end_frames = shooter->ex_ints[2];
        laser->hit_start_frame = shooter->ex_ints[3];
        laser->end_hit_frames = (int32_t)shooter->laser_end_hit_frames;
        laser->state = laser->start_frames == 0 ? LASER_FIRING : LASER_STARTING;
        return laser;
    }
    return &m_SpareLaser;
}

void Bullet_ClearAll(void)
{
    M_ClearAll(false, false, 0);
}

void Bullet_ClearAllToItems(void)
{
    M_ClearAll(true, false, 0);
}

void Bullet_ClearAllWithBonus(const int32_t max_bonus, const bool to_items)
{
    const int32_t total = M_ClearAll(to_items, true, max_bonus);
    g_GameManager.score.value += (uint32_t)total;
    if (total != 0) {
        Gui_ShowBonusPopup(total);
    }
}
