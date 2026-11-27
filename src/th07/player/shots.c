#include "th07/anm.h"
#include "th07/effect.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/player/private.h"
#include "th07/sound.h"

#include "core/base/color.h"
#include "core/math/common.h"
#include "core/math/x87.h"
#include "stg/angle.h"
#include "stg/rng.h"
#include "stg/timer.h"

#define M_NO_TARGET -100.0f
#define M_NO_TRAIL -999.0f
#define M_TRAIL_SET -900.0f
#define M_HOMING_FRAMES 40
#define M_FIRE_FRAMES 30
#define M_TRAIL_LASER_FRAMES 999
#define M_LASER_CUT_FRAMES 20
#define M_LASER_IDLE_FRAMES 50
#define M_LASER_FADE_FRAMES 70
#define M_TRAIL_LASER 2
#define M_SCRIPT_HIT_OFFSET 0x20
#define M_SPARK_HITS 8
#define M_BOX_SPARK_HITS 4
#define M_MARISA 1
#define M_SHOT_TYPE_B 1
#define M_EFFECT_SHOT_HIT ((EFFECT_TYPE)(EFFECT_PARTICLE + 1))

static void M_Box(
    const VEC3 pos, const VEC3 size, VEC3 *const min, VEC3 *const max)
{
    min->x = pos.x - size.x * 0.5f;
    min->y = pos.y - size.y * 0.5f;
    max->x = size.x * 0.5f + pos.x;
    max->y = size.y * 0.5f + pos.y;
}

static void M_FireShot(PLAYER_BULLET *const bullet, const SHOT *const shot)
{
    bullet->pos = shot->option == 0 ? g_Player.pos
                                    : g_Player.options.pos[shot->option - 1];
    bullet->pos.x += shot->offset_x;
    bullet->pos.y += shot->offset_y;
    bullet->pos.z = 0.495f;
    bullet->size = (VEC3) { shot->size_x, shot->size_y, 1.0f };
    bullet->angle = shot->angle;
    bullet->speed = shot->speed;
    X87_SinCosScale(
        shot->angle, shot->speed, &bullet->velocity.x, &bullet->velocity.y);
    Timer_Set(&bullet->timer, 0);
    bullet->kind = shot->kind;
    bullet->damage = shot->damage;
    if (shot->sound >= 0) {
        Sound_Queue(shot->sound);
    }
    Anm_StartScript(&bullet->vm, shot->script);
}

static bool M_FirePlain(
    PLAYER_BULLET *const bullet, const int32_t frame, const SHOT *const shot)
{
    if (frame % shot->interval != shot->phase) {
        return false;
    }
    M_FireShot(bullet, shot);
    return true;
}

// Returns whether the laser entry of shot is free while the options are in
// state options. A laser of another shot in the entry is interrupted, and
// the entry freed for the next frame.
static bool M_TakeLaser(const SHOT *const shot, const PLAYER_OPTIONS options)
{
    PLAYER_LASER *const laser = &g_Player.shots.lasers[shot->phase];
    if (laser->bullet != nullptr) {
        if (laser->shot != shot) {
            laser->bullet->vm.pending_interrupt = 1;
            laser->bullet = nullptr;
        }
        return false;
    }
    return g_Player.options.state == options;
}

static void M_StartLaser(
    PLAYER_BULLET *const bullet, const SHOT *const shot, const int32_t frames)
{
    Timer_Set(&g_Player.shots.lasers[shot->phase].timer, frames);
    g_Player.shots.lasers[shot->phase].bullet = bullet;
    bullet->laser = shot->phase;
    bullet->option = shot->option;
    bullet->velocity.z = shot->offset_x;
}

static bool M_FireLaser(PLAYER_BULLET *const bullet, const SHOT *const shot)
{
    if (!M_TakeLaser(shot, PLAYER_OPTIONS_OUT)) {
        return false;
    }
    M_StartLaser(bullet, shot, shot->interval);
    M_FireShot(bullet, shot);
    g_Player.shots.lasers[shot->phase].shot = shot;
    return true;
}

static bool M_FireTrailLaser(
    PLAYER_BULLET *const bullet, const SHOT *const shot)
{
    if (!M_TakeLaser(shot, PLAYER_OPTIONS_IN)) {
        return false;
    }
    M_StartLaser(bullet, shot, M_TRAIL_LASER_FRAMES);
    bullet->trail_length = shot->interval;
    M_FireShot(bullet, shot);
    for (int32_t i = PLAYER_TRAIL_COUNT - 1; i >= 0; i--) {
        bullet->trail[i].x = M_NO_TRAIL;
    }
    bullet->pos.x = M_NO_TRAIL;
    g_Player.shots.lasers[shot->phase].shot = shot;
    return true;
}

static void M_Turn(
    PLAYER_BULLET *const bullet, const float angle, const float speed)
{
    X87_SinCosScale(angle, speed, &bullet->velocity.x, &bullet->velocity.y);
    bullet->angle = angle;
}

static bool M_FireHoming(
    PLAYER_BULLET *const bullet, const int32_t frame, const SHOT *const shot)
{
    if (frame % shot->interval != shot->phase) {
        return false;
    }
    M_FireShot(bullet, shot);
    const VEC3 target = g_Player.aim_target;
    if (target.x > M_NO_TARGET) {
        const float dx = target.x - bullet->pos.x;
        const float dy = target.y - bullet->pos.y;
        const float angle =
            Angle_AddNormalize(X87_Atan2(dy, dx), shot->angle + MATH_HALF_PI);
        M_Turn(bullet, angle, shot->speed * 1.5f);
    }
    return true;
}

static bool M_FireAimed(
    PLAYER_BULLET *const bullet, const int32_t frame, const SHOT *const shot)
{
    if (frame % shot->interval != shot->phase) {
        return false;
    }
    M_FireShot(bullet, shot);
    const float angle = Angle_AddNormalize(
        g_Player.shots.aim_angle, shot->angle + MATH_HALF_PI);
    M_Turn(bullet, angle, shot->speed);
    return true;
}

static bool M_Fire(
    PLAYER_BULLET *const bullet, const int32_t frame, const SHOT *const shot)
{
    switch (shot->fire) {
    case SHOT_FIRE_LASER:
        return M_FireLaser(bullet, shot);
    case SHOT_FIRE_TRAIL_LASER:
        return M_FireTrailLaser(bullet, shot);
    case SHOT_FIRE_HOMING:
        return M_FireHoming(bullet, frame, shot);
    case SHOT_FIRE_AIMED:
        return M_FireAimed(bullet, frame, shot);
    default:
        return M_FirePlain(bullet, frame, shot);
    }
}

static void M_Home(
    PLAYER_BULLET *const bullet, const float max_speed, const float step)
{
    if (bullet->state != PLAYER_BULLET_FLYING) {
        return;
    }
    const VEC3 target = g_Player.homing_target;
    float x;
    float y;
    float len;
    if (target.x > M_NO_TARGET && bullet->timer.current < M_HOMING_FRAMES
        && Timer_HasTicked(&bullet->timer)) {
        x = target.x - bullet->pos.x;
        y = target.y - bullet->pos.y;
        len = X87_Sqrt(x * x + y * y) / (bullet->speed / 4.0f);
        if (len < 1.0f) {
            len = 1.0f;
        }
        x = x / len + bullet->velocity.x;
        y = y / len + bullet->velocity.y;
        len = X87_Sqrt(x * x + y * y);
        bullet->speed = len > max_speed ? max_speed : len;
        if (bullet->speed < 1.0f) {
            bullet->speed = 1.0f;
        }
        bullet->velocity.x = x * bullet->speed / len;
        bullet->velocity.y = y * bullet->speed / len;
    } else if (bullet->speed < max_speed) {
        bullet->speed += step;
        x = bullet->velocity.x;
        y = bullet->velocity.y;
        len = X87_Sqrt(x * x + y * y);
        bullet->velocity.x = x * bullet->speed / len;
        bullet->velocity.y = y * bullet->speed / len;
    }
}

// Cuts the laser of bullet short during a dialogue or a bomb, and returns
// whether its time ran out, which frees the bullet.
static bool M_RunLaser(PLAYER_BULLET *const bullet)
{
    PLAYER_LASER *const laser = &g_Player.shots.lasers[bullet->laser];
    if (laser->bullet != bullet && bullet->vm.stopped) {
        bullet->vm.pending_interrupt = 1;
    }
    if ((Gui_IsMsgRunning() || g_Player.bomb.running)
        && laser->timer.current > M_LASER_CUT_FRAMES) {
        Timer_Set(&laser->timer, M_LASER_CUT_FRAMES);
    }
    // The entry is freed even when another bullet holds it.
    if (laser->timer.current <= 0) {
        Timer_Set(&laser->timer, 0);
        laser->bullet = nullptr;
        bullet->state = PLAYER_BULLET_FREE;
        return true;
    }
    if (laser->timer.current <= M_LASER_FADE_FRAMES && bullet->vm.stopped) {
        bullet->vm.pending_interrupt = 1;
    }
    return false;
}

// Keeps a laser on its option, reaching to the top of the play area. The
// laser then still moves by its velocity in the frame.
static bool M_HoldLaser(PLAYER_BULLET *const bullet)
{
    if (M_RunLaser(bullet)) {
        return true;
    }
    bullet->pos = g_Player.options.pos[bullet->option - 1];
    bullet->pos.x += bullet->velocity.z;
    bullet->pos.z = 0.44f;
    if (g_Player.state.value == PLAYER_DYING) {
        return true;
    }
    bullet->vm.scale_y = bullet->pos.y / 14.0f;
    bullet->size.y = bullet->pos.y;
    bullet->pos.y /= 2.0f;
    return false;
}

// Keeps a trail laser on the player, reaching to the top of the play area,
// and sets a damage box at each point of its trail.
static bool M_HoldTrailLaser(PLAYER_BULLET *const bullet)
{
    if (M_RunLaser(bullet)) {
        return true;
    }
    for (int32_t i = 0; i < bullet->trail_length; i++) {
        if (bullet->trail[i].x >= M_TRAIL_SET) {
            PLAYER_DAMAGE_BOX *const box =
                &g_Player.damage_boxes[PLAYER_LASER_BOX_FIRST + i];
            box->pos = bullet->trail[i];
            box->damage = 1;
            box->size = bullet->size;
        }
    }
    for (int32_t i = PLAYER_TRAIL_COUNT - 1; i > 0; i--) {
        bullet->trail[i] = bullet->trail[i - 1];
    }
    bullet->trail[0] = bullet->pos;
    if (g_Player.state.value == PLAYER_DYING) {
        return true;
    }
    bullet->pos = g_Player.pos;
    bullet->pos.x += bullet->velocity.z;
    bullet->pos.z = 0.44f;
    bullet->vm.scale_y = (bullet->pos.y + 64.0f) / 14.0f;
    bullet->size.y = g_Player.pos.y + 64.0f;
    bullet->pos.y = bullet->pos.y / 2.0f - 32.0f;
    return false;
}

// Returns whether the bullet is freed.
static bool M_Move(PLAYER_BULLET *const bullet)
{
    switch (bullet->shot->move) {
    case SHOT_MOVE_HOMING:
        M_Home(bullet, 10.0f, 0.33333334f);
        return false;
    case SHOT_MOVE_HOMING_FAST:
        M_Home(bullet, 18.0f, 0.6f);
        return false;
    case SHOT_MOVE_RISING:
        if (bullet->state == PLAYER_BULLET_FLYING) {
            bullet->velocity.y -= Rng_F32(&g_Rng) * 0.1f + 0.27f;
        }
        return false;
    case SHOT_MOVE_LASER:
        return M_HoldLaser(bullet);
    case SHOT_MOVE_TRAIL_LASER:
        return M_HoldTrailLaser(bullet);
    default:
        return false;
    }
}

static void M_SetVelocity(
    PLAYER_BULLET *const bullet, const float angle, const float speed)
{
    X87_SinCosScale(angle, speed, &bullet->velocity.x, &bullet->velocity.y);
}

// Grows a shot on its first hit and sends it upwards at angle. Its angle
// stays the one it was fired at.
static void M_Linger(PLAYER_BULLET *const bullet, const float angle)
{
    switch (bullet->vm.script_index) {
    case 0x441:
        bullet->size.x = 32.0f;
        bullet->size.y = 32.0f;
        M_SetVelocity(bullet, angle, 4.0f);
        break;
    case 0x442:
        bullet->size.x = 42.0f;
        bullet->size.y = 42.0f;
        M_SetVelocity(bullet, angle, 4.0f);
        break;
    case 0x443:
        bullet->size.x = 48.0f;
        bullet->size.y = 48.0f;
        M_SetVelocity(bullet, angle, 4.0f);
        break;
    case 0x444:
        bullet->size.x = 56.0f;
        bullet->size.y = 56.0f;
        M_SetVelocity(bullet, angle, 4.0f);
        break;
    case 0x445:
        bullet->size.x = 48.0f;
        bullet->size.y = 48.0f;
        M_SetVelocity(bullet, angle, 6.0f);
        break;
    case 0x446:
        bullet->size.x = 64.0f;
        bullet->size.y = 64.0f;
        M_SetVelocity(bullet, angle, 6.0f);
        break;
    case 0x447:
        bullet->size.x = 80.0f;
        bullet->size.y = 80.0f;
        M_SetVelocity(bullet, angle, 6.0f);
        break;
    case 0x448:
        bullet->size.x = 96.0f;
        bullet->size.y = 96.0f;
        M_SetVelocity(bullet, angle, 6.0f);
        break;
    default:
        break;
    }
}

// Returns whether the hit is skipped.
static bool M_Hit(PLAYER_BULLET *const bullet, const VEC3 pos)
{
    switch (bullet->shot->hit) {
    case SHOT_HIT_LINGER:
        if (bullet->state == PLAYER_BULLET_HIT) {
            if (bullet->timer.current % 2 != 0) {
                return true;
            }
            bullet->damage = (int16_t)(bullet->damage / 3);
            if (bullet->damage == 0) {
                bullet->damage = 1;
            }
            bullet->velocity.x *= 0.88f;
            bullet->velocity.y *= 0.88f;
        } else {
            M_Linger(bullet, Rng_F32(&g_Rng) * 1.5707964f - 2.3561945f);
        }
        if (bullet->timer.current % 6 == 0) {
            Effect_Spawn(M_EFFECT_SHOT_HIT, pos, 1, COLOR_WHITE);
        }
        return false;
    case SHOT_HIT_SPARK:
        g_Player.shots.hit_counter++;
        if (g_Player.shots.hit_counter % M_SPARK_HITS == 0) {
            VEC3 at = pos;
            at.x = bullet->pos.x;
            Effect_Spawn(M_EFFECT_SHOT_HIT, at, 1, COLOR_WHITE);
        }
        return false;
    default:
        return false;
    }
}

static bool M_IsLaser(const SHOT_KIND kind)
{
    return kind == SHOT_KIND_LASER || kind == SHOT_KIND_TRAIL_LASER;
}

static void M_DrawTrail(PLAYER_BULLET *const bullet, const ANM_DRAW *const draw)
{
    const int32_t alpha = (int32_t)(bullet->vm.color >> 24);
    ANM_DRAW part = *draw;
    for (int32_t i = 0; i < bullet->trail_length; i++) {
        if (bullet->trail[i].x == M_NO_TRAIL) {
            break;
        }
        bullet->vm.pos = bullet->trail[i];
        const uint8_t faded =
            (uint8_t)(alpha - alpha * i / bullet->trail_length);
        bullet->vm.color =
            (bullet->vm.color & 0x00ffffffu) | ((uint32_t)faded << 24);
        bullet->vm.pos.x += g_GameManager.play_area.x;
        bullet->vm.pos.y += g_GameManager.play_area.y;
        part.part = (uint32_t)i + 1;
        Anm_Draw(&bullet->vm, &part);
    }
    bullet->vm.color =
        (bullet->vm.color & 0x00ffffffu) | ((uint32_t)alpha << 24);
}

// A shot that does not fire leaves its bullet for the next shot.
static void M_FireShots(const int32_t frame)
{
    const SHOT_TYPE *const shot_type = g_Player.move.focused
        ? &g_Player.shot_type_focused
        : &g_Player.shot_type;
    // ShotType_Read makes sure that a table goes past full power.
    const int32_t power = X87_Ftol(g_GameManager.player.power);
    int32_t level = 0;
    while (level < shot_type->table_count - 1
           && power >= shot_type->tables[level].max_power) {
        level++;
    }
    const SHOT_TABLE *const table = &shot_type->tables[level];
    int32_t index = 0;
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        PLAYER_BULLET *const bullet = &g_Player.shots.bullets[i];
        if (bullet->state != PLAYER_BULLET_FREE) {
            continue;
        }
        for (;;) {
            const SHOT *const shot = &table->shots[index];
            const bool fired = M_Fire(bullet, frame, shot);
            if (fired) {
                bullet->vm.z_write_off = true;
                bullet->state = PLAYER_BULLET_FLYING;
                bullet->shot = shot;
            }
            index++;
            if (index >= table->count) {
                return;
            }
            if (fired) {
                break;
            }
        }
    }
}

void Player_StartFiring(void)
{
    if (g_Player.shots.fire_timer.current < 0) {
        Timer_Set(&g_Player.shots.fire_timer, 0);
    }
}

void Player_UpdateFiring(void)
{
    TIMER *const timer = &g_Player.shots.fire_timer;
    if (timer->current < 0) {
        return;
    }
    const bool master_spark = g_Player.bomb.running
        && g_GameManager.run.character == M_MARISA
        && g_GameManager.run.shot_type == M_SHOT_TYPE_B;
    if (Timer_HasTicked(timer) && !master_spark) {
        M_FireShots(timer->current);
    }
    Timer_TickAt(timer, g_GameSpeed);
    if (timer->current >= M_FIRE_FRAMES || g_Player.state.value == PLAYER_DYING
        || g_Player.state.value == PLAYER_RESPAWNING) {
        Timer_Set(timer, -1);
    }
}

void Player_UpdateShots(void)
{
    PLAYER_LASER *const lasers = g_Player.shots.lasers;
    if (g_Player.options.state != PLAYER_OPTIONS_IN
        && lasers[M_TRAIL_LASER].bullet != nullptr) {
        lasers[M_TRAIL_LASER].bullet->state = PLAYER_BULLET_FREE;
        lasers[M_TRAIL_LASER].bullet = nullptr;
    }
    if (g_Player.options.state != PLAYER_OPTIONS_OUT) {
        for (int32_t i = 0; i < M_TRAIL_LASER; i++) {
            if (lasers[i].bullet != nullptr) {
                lasers[i].bullet->vm.pending_interrupt = 1;
                lasers[i].bullet = nullptr;
            }
        }
    }
    if (g_Player.state.value == PLAYER_DYING) {
        for (int32_t i = 0; i < PLAYER_LASER_COUNT; i++) {
            if (lasers[i].bullet != nullptr) {
                lasers[i].bullet->state = PLAYER_BULLET_FREE;
                lasers[i].bullet = nullptr;
            }
        }
    }
    for (int32_t i = 0; i < PLAYER_LASER_COUNT; i++) {
        if (lasers[i].bullet == nullptr) {
            continue;
        }
        if (lasers[i].timer.current > 0
            && lasers[i].timer.current < M_TRAIL_LASER_FRAMES) {
            Timer_SubAt(&lasers[i].timer, 1, g_GameSpeed);
        }
        if (g_Player.shots.fire_timer.current < 0
            && lasers[i].timer.current > M_LASER_IDLE_FRAMES) {
            Timer_Set(&lasers[i].timer, M_LASER_IDLE_FRAMES);
        }
        if (lasers[i].timer.current == 0) {
            lasers[i].bullet = nullptr;
        }
    }
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        PLAYER_BULLET *const bullet = &g_Player.shots.bullets[i];
        if (bullet->state == PLAYER_BULLET_FREE) {
            continue;
        }
        if (M_Move(bullet)) {
            bullet->state = PLAYER_BULLET_FREE;
            continue;
        }
        bullet->pos.x += g_GameSpeed * bullet->velocity.x;
        bullet->pos.y += g_GameSpeed * bullet->velocity.y;
        // A shot freed here still runs its script and timer this frame.
        if (!M_IsLaser(bullet->kind)
            && !GameManager_IsInPlayArea(
                bullet->pos.x, bullet->pos.y, bullet->vm.sprite_data->width,
                bullet->vm.sprite_data->height)) {
            bullet->state = PLAYER_BULLET_FREE;
        }
        if (Anm_Step(&bullet->vm)) {
            bullet->state = PLAYER_BULLET_FREE;
        }
        Timer_TickAt(&bullet->timer, g_GameSpeed);
    }
}

void Player_DrawShots(
    const PLAYER_BULLET_STATE state, const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        PLAYER_BULLET *const bullet = &g_Player.shots.bullets[i];
        if (bullet->state != state) {
            continue;
        }
        if (bullet->vm.auto_rotate != 0) {
            bullet->vm.rotation.z =
                Angle_AddNormalize(bullet->angle, MATH_HALF_PI);
            bullet->vm.rotation_changed = true;
        }
        bullet->vm.pos.x = g_GameManager.play_area.x + bullet->pos.x;
        bullet->vm.pos.y = g_GameManager.play_area.y + bullet->pos.y;
        bullet->vm.pos.z = 0.4f;
        Anm_Draw(&bullet->vm, draw);
        if (state == PLAYER_BULLET_FLYING
            && bullet->shot->draw == SHOT_DRAW_TRAIL) {
            M_DrawTrail(bullet, draw);
        }
    }
}

int32_t Player_CalcDamage(
    const VEC3 pos, const VEC3 size, bool *const hit_by_bomb)
{
    if (!Timer_HasTicked(&g_Player.state.timer)) {
        return 0;
    }
    int32_t damage = 0;
    VEC3 enemy_min;
    VEC3 enemy_max;
    M_Box(pos, size, &enemy_min, &enemy_max);
    *hit_by_bomb = false;
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        PLAYER_BULLET *const bullet = &g_Player.shots.bullets[i];
        if (bullet->state == PLAYER_BULLET_FREE
            || (bullet->state != PLAYER_BULLET_FLYING
                && bullet->kind != SHOT_KIND_PIERCING)) {
            continue;
        }
        VEC3 min;
        VEC3 max;
        M_Box(bullet->pos, bullet->size, &min, &max);
        if (min.y > enemy_max.y || min.x > enemy_max.x || max.y < enemy_min.y
            || max.x < enemy_min.x) {
            continue;
        }
        if (M_IsLaser(bullet->kind) && bullet->timer.current % 2 != 0) {
            continue;
        }
        if (M_Hit(bullet, pos)) {
            continue;
        }
        if (!g_Player.bomb.running) {
            damage += bullet->damage;
        } else {
            damage += bullet->damage / 3 != 0 ? bullet->damage / 3 : 1;
        }
        if (M_IsLaser(bullet->kind)) {
            continue;
        }
        if (bullet->state == PLAYER_BULLET_FLYING) {
            Anm_StartScript(
                &bullet->vm, bullet->vm.script_index + M_SCRIPT_HIT_OFFSET);
            Effect_Spawn(M_EFFECT_SHOT_HIT, bullet->pos, 1, COLOR_WHITE);
            bullet->pos.z = 0.1f;
        }
        bullet->state = PLAYER_BULLET_HIT;
        if (bullet->kind != SHOT_KIND_PIERCING) {
            bullet->velocity.x /= 8.0f;
            bullet->velocity.y /= 8.0f;
        }
    }
    for (int32_t i = 0; i < PLAYER_DAMAGE_BOX_COUNT; i++) {
        PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[i];
        if (box->size.x <= 0.0f) {
            continue;
        }
        VEC3 min;
        VEC3 max;
        M_Box(box->pos, box->size, &min, &max);
        if (min.x > enemy_max.x || max.x < enemy_min.x || min.y > enemy_max.y
            || max.y < enemy_min.y) {
            continue;
        }
        damage += box->damage;
        box->damage_dealt += box->damage;
        g_Player.shots.hit_counter++;
        if (g_Player.shots.hit_counter % M_BOX_SPARK_HITS == 0) {
            Effect_Spawn(
                i < PLAYER_LASER_BOX_FIRST ? EFFECT_BOMB_SPARK
                                           : M_EFFECT_SHOT_HIT,
                pos, 1, COLOR_WHITE);
        }
        if (g_Player.bomb.running) {
            *hit_by_bomb = true;
        }
    }
    return damage;
}
