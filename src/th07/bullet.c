#include "th07/bullet.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/scene.h"
#include "th07/sound.h"
#include "th07/stubs.h"

#include "core/base/assert.h"
#include "core/base/utils.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <math.h>
#include <string.h>

#define M_CALC_PRIORITY 12
#define M_DRAW_PRIORITY 10
#define M_ANM_SLOT 0xb
#define M_ANM_SLOT_COUNT 4
#define M_FIRST_SPRITE 0x200
#define M_OUT_OF_BOUNDS_FRAMES 0x80
#define M_START_FAST_FRAMES 16
#define M_GRAZE_FRAMES 16
#define M_CLEAR_FRAMES 10
#define M_FIRST_BONUS 2000
#define M_BONUS_STEP 20
#define M_LASER_ITEM_SPACING 32.0f

typedef enum {
    M_SCRIPT_FLYING = 0,
    M_SCRIPT_SPAWNING = 1,
    M_SCRIPT_FADING = 4,
    M_SCRIPT_COUNT = 5,
} M_SCRIPT;

static const int32_t m_TypeScripts[BULLET_TYPE_COUNT][M_SCRIPT_COUNT] = {
    { 512, 530, 531, 532, 527 }, { 513, 533, 534, 535, 528 },
    { 514, 533, 534, 535, 528 }, { 515, 533, 534, 535, 528 },
    { 516, 533, 534, 535, 528 }, { 517, 533, 534, 535, 528 },
    { 518, 533, 534, 535, 528 }, { 519, 536, 536, 536, 529 },
    { 520, 536, 536, 536, 529 }, { 521, 536, 536, 536, 529 },
    { 680, 682, 682, 682, 681 },
};

// Hold the sprite offsets of the colors, for sprites up to 16 high and up
// to 32 high.
static const int32_t m_ColorOffsets16[] = {
    0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 0,
};
static const int32_t m_ColorOffsets32[] = {
    0, 1, 1, 2, 2, 3, 4, 0,
};

BULLET_MANAGER g_Bullets;

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

static bool M_KeepsSprites(void)
{
    const SCENE scene = Scene_GetRequested();
    return scene == SCENE_NEXT_STAGE || scene == SCENE_RESTART_STAGE
        || scene == SCENE_NEXT_STAGE_AGAIN;
}

static bool M_IsRunning(const ANM_VM *const vm)
{
    return vm->script != nullptr && vm->pc >= 0;
}

static void M_Free(BULLET *const bullet)
{
    bullet->state = BULLET_FREE;
    Timer_Set(&bullet->timer, 0);
    Timer_Set(&bullet->graze_timer, 0);
}

// Copies src over dst, unless dst runs the same script and the script of
// src has ended, which keeps the sprite of the bullet's last use.
static void M_CopySprite(ANM_VM *const dst, const ANM_VM *const src)
{
    if (dst->script_index != src->script_index || M_IsRunning(src)) {
        *dst = *src;
    }
    Anm_RenewPresentation(dst);
}

// Shows the sprite of src in color on dst, a spawn or fade sprite of
// bullet, unless dst shows src's sprite plus color already. The height of
// the flying sprite selects the offsets.
static void M_SetColorSprite(
    const BULLET *const bullet, ANM_VM *const dst, const ANM_VM *const src,
    const int32_t color)
{
    if (dst->sprite == src->sprite + color) {
        return;
    }
    const float height = bullet->sprites.flying.sprite_data->height;
    if (height <= 16.0f) {
        ASSERT(color >= 0 && color < (int32_t)ARRAY_SIZE(m_ColorOffsets16));
        Anm_SetSprite(dst, src->sprite + m_ColorOffsets16[color]);
    } else if (height <= 32.0f) {
        ASSERT(color >= 0 && color < (int32_t)ARRAY_SIZE(m_ColorOffsets32));
        Anm_SetSprite(dst, src->sprite + m_ColorOffsets32[color]);
    } else {
        Anm_SetSprite(dst, src->sprite + color);
    }
}

static BULLET *M_FindFree(void)
{
    int32_t index = g_Bullets.next_bullet;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        if (g_Bullets.bullets[index].state == BULLET_FREE) {
            return &g_Bullets.bullets[index];
        }
        index = (index + 1) % BULLET_COUNT;
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
        angle += (float)col * MATH_TWO_PI / (float)shooter->columns;
        angle += (float)row * shooter->angle2 + shooter->angle1;
        break;
    case BULLET_AIM_RING_OFFSET_AT_PLAYER:
        angle += aim_angle;
        [[fallthrough]];
    case BULLET_AIM_RING_OFFSET:
        angle += MATH_PI / (float)shooter->columns;
        angle += (float)col * MATH_TWO_PI / (float)shooter->columns;
        angle += shooter->angle1;
        break;
    case BULLET_AIM_RANDOM_ANGLE:
        angle = Rng_F32(&g_Rng) * (shooter->angle1 - shooter->angle2)
            + shooter->angle2;
        break;
    case BULLET_AIM_RING_RANDOM_SPEED:
        *speed = Rng_F32(&g_Rng) * (shooter->speed1 - shooter->speed2)
            + shooter->speed2;
        angle += (float)col * MATH_TWO_PI / (float)shooter->columns;
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

static void M_PlaySound(const BULLET *const bullet)
{
    if (bullet->sound >= 0) {
        Sound_Queue(bullet->sound);
    }
}

// Starts the commands of bullet from next_command on, up to the first
// that starts a behaviour or has to wait.
static void M_RunCommands(BULLET *const bullet)
{
    for (; bullet->next_command < BULLET_COMMAND_COUNT;
         bullet->next_command++) {
        const BULLET_COMMAND *const command =
            &bullet->commands[bullet->next_command];
        if (command->flag == 0
            || (!command->concurrent && bullet->running != 0)) {
            return;
        }
        if ((bullet->flags & command->flag) == 0) {
            continue;
        }
        switch (command->flag) {
        case BULLET_FLAG_START_FAST:
            bullet->running |= BULLET_FLAG_START_FAST;
            Timer_Set(&bullet->start_fast.timer, 0);
            break;
        case BULLET_FLAG_ACCELERATE: {
            bullet->running |= BULLET_FLAG_ACCELERATE;
            const float angle =
                command->args[1] > -990.0f ? command->args[1] : bullet->angle;
            Timer_Set(&bullet->accelerate.timer, 0);
            bullet->accelerate.frames = command->frames;
            M_SetVelocity(
                &bullet->accelerate.accel, angle,
                g_GameSpeed * command->args[0]);
            if (bullet->next_command != 0) {
                M_PlaySound(bullet);
            }
            break;
        }
        case BULLET_FLAG_CURVE:
            bullet->running |= BULLET_FLAG_CURVE;
            bullet->curve.speed = command->args[0];
            bullet->curve.angle = command->args[1];
            Timer_Set(&bullet->curve.timer, 0);
            bullet->curve.frames = command->frames;
            if (bullet->next_command != 0) {
                M_PlaySound(bullet);
            }
            break;
        case BULLET_FLAG_TURN:
        case BULLET_FLAG_TURN_AT_PLAYER:
        case BULLET_FLAG_TURN_TO:
            bullet->running |= (uint16_t)command->flag;
            bullet->turn.angle = command->args[0];
            bullet->turn.speed =
                command->args[1] > -999.0f ? command->args[1] : bullet->speed;
            Timer_Set(&bullet->turn.timer, 0);
            bullet->turn.frames = command->frames;
            bullet->turn.max = command->count;
            bullet->turn.count = 0;
            break;
        case BULLET_FLAG_BOUNCE:
        case BULLET_FLAG_BOUNCE_NO_BOTTOM:
            bullet->running |= (uint16_t)command->flag;
            bullet->bounce.speed =
                command->args[0] >= 0.0f ? command->args[0] : bullet->speed;
            bullet->bounce.max = command->frames;
            bullet->bounce.count = 0;
            break;
        case BULLET_FLAG_STAY_OUTSIDE:
            bullet->keep_outside_frames = command->frames;
            continue;
        default:
            break;
        }
        bullet->next_command++;
        return;
    }
}

// Returns whether a bullet was free for it.
static bool M_Spawn(
    const BULLET_SHOOTER *const shooter, const BULLET_SPRITES *const type,
    const int32_t col, const int32_t row, const float aim_angle)
{
    BULLET *const bullet = M_FindFree();
    if (bullet == nullptr) {
        return false;
    }
    float speed = shooter->speed1;
    if (shooter->rows > 1) {
        speed = shooter->speed1
            - (shooter->speed1 - shooter->speed2) * (float)row
                / (float)shooter->rows;
    }
    const float angle = M_Aim(shooter, col, row, aim_angle, &speed);
    bullet->state = BULLET_FLYING;
    bullet->grazed = false;
    Timer_Set(&bullet->timer, 0);
    Timer_Set(&bullet->graze_timer, 0);
    bullet->speed = speed;
    bullet->angle = Angle_AddNormalize(angle, 0.0f);
    bullet->pos = shooter->pos;
    bullet->pos.z = 0.1f;
    // Takes the angle before it is normalized.
    M_SetVelocity(&bullet->velocity, angle, speed * g_GameSpeed);
    bullet->color = shooter->color;
    bullet->pattern_state = 0;
    M_CopySprite(&bullet->sprites.flying, &type->flying);
    M_CopySprite(&bullet->sprites.fading, &type->fading);
    bullet->sprites.hitbox_size = type->hitbox_size;
    bullet->sprites.size_class = type->size_class;
    bullet->sound = shooter->bullet_sound;
    bullet->keep_outside_frames = 0;
    if (bullet->sprites.flying.sprite != type->flying.sprite + shooter->color) {
        Anm_SetSprite(
            &bullet->sprites.flying, type->flying.sprite + shooter->color);
    }
    M_SetColorSprite(
        bullet, &bullet->sprites.fading, &type->fading, shooter->color);
    static const BULLET_FLAG spawn_flags[BULLET_SPAWN_SPRITE_COUNT] = {
        BULLET_FLAG_SPAWN_1,
        BULLET_FLAG_SPAWN_2,
        BULLET_FLAG_SPAWN_3,
    };
    for (int32_t i = 0; i < BULLET_SPAWN_SPRITE_COUNT; i++) {
        if ((shooter->flags & spawn_flags[i]) != 0) {
            ANM_VM *const vm = &bullet->sprites.spawning[i];
            M_CopySprite(vm, &type->spawning[i]);
            M_SetColorSprite(bullet, vm, &type->spawning[i], shooter->color);
            bullet->state = (BULLET_STATE)(BULLET_SPAWNING_1 + i);
            bullet->pos =
                Vec3_Sub(bullet->pos, Vec3_Scale(bullet->velocity, 4.0f));
            break;
        }
    }
    for (int32_t i = 0; i < BULLET_COMMAND_COUNT; i++) {
        bullet->commands[i] = shooter->commands[i];
    }
    bullet->flags = (uint16_t)shooter->flags;
    bullet->running = 0;
    bullet->next_command = 0;
    M_RunCommands(bullet);
    // Fades only after the commands ran.
    if (g_Bullets.clear_frames != 0
        && (bullet->flags & BULLET_FLAG_RESIST_CLEAR) == 0) {
        bullet->state = BULLET_FADING;
    }
    g_Bullets.next_bullet =
        (int32_t)(bullet - g_Bullets.bullets + 1) % BULLET_COUNT;
    return true;
}

static void M_StartFast(BULLET *const bullet)
{
    if (bullet->start_fast.timer.current <= M_START_FAST_FRAMES) {
        const float extra =
            5.0f - Timer_AsFloat(&bullet->start_fast.timer) * 5.0f / 16.0f;
        M_SetVelocity(
            &bullet->velocity, bullet->angle,
            (extra + bullet->speed) * g_GameSpeed);
    } else {
        bullet->running ^= BULLET_FLAG_START_FAST;
    }
    Timer_TickAt(&bullet->start_fast.timer, g_GameSpeed);
}

static void M_Accelerate(BULLET *const bullet)
{
    if (bullet->accelerate.timer.current >= bullet->accelerate.frames) {
        bullet->running &= (uint16_t)~BULLET_FLAG_ACCELERATE;
    } else {
        bullet->velocity = Vec3_Add(
            bullet->velocity,
            Vec3_Scale(bullet->accelerate.accel, g_GameSpeed));
        if (fabsf(bullet->velocity.x) > 0.0001f
            || fabsf(bullet->velocity.y) > 0.0001f) {
            bullet->angle = X87_Atan2(bullet->velocity.y, bullet->velocity.x);
        }
    }
    Timer_TickAt(&bullet->accelerate.timer, g_GameSpeed);
}

static void M_Curve(BULLET *const bullet)
{
    if (bullet->curve.timer.current >= bullet->curve.frames) {
        bullet->running &= (uint16_t)~BULLET_FLAG_CURVE;
    } else {
        bullet->angle = Angle_AddNormalize(
            bullet->angle, g_GameSpeed * bullet->curve.angle);
        bullet->speed = g_GameSpeed * bullet->curve.speed + bullet->speed;
        M_SetVelocity(
            &bullet->velocity, bullet->angle, g_GameSpeed * bullet->speed);
    }
    Timer_TickAt(&bullet->curve.timer, g_GameSpeed);
}

// Runs the turn of flag. The turns share their state, so each running
// turn ticks the same timer.
static void M_Turn(BULLET *const bullet, const BULLET_FLAG flag)
{
    float speed;
    if (bullet->turn.timer.current >= bullet->turn.frames) {
        M_PlaySound(bullet);
        bullet->turn.count++;
        if (bullet->turn.count >= bullet->turn.max) {
            bullet->running &= (uint16_t)~flag;
        }
        switch (flag) {
        case BULLET_FLAG_TURN:
            bullet->angle += bullet->turn.angle;
            break;
        case BULLET_FLAG_TURN_TO:
            bullet->angle = bullet->turn.angle;
            break;
        default:
            bullet->angle = Angle_AddNormalize(
                Player_AngleFrom(bullet->pos), bullet->turn.angle);
            break;
        }
        bullet->speed = bullet->turn.speed;
        speed = bullet->speed;
        Timer_Set(&bullet->turn.timer, 0);
    } else {
        speed = bullet->speed
            - Timer_AsFloat(&bullet->turn.timer) * bullet->speed
                / (float)bullet->turn.frames;
    }
    M_SetVelocity(&bullet->velocity, bullet->angle, speed * g_GameSpeed);
    Timer_TickAt(&bullet->turn.timer, g_GameSpeed);
}

static void M_Bounce(BULLET *const bullet)
{
    const ANM_SPRITE *const sprite = bullet->sprites.flying.sprite_data;
    if (GameManager_IsInPlayArea(
            bullet->pos.x, bullet->pos.y, sprite->width, sprite->height)) {
        return;
    }
    M_PlaySound(bullet);
    if (bullet->pos.x < 0.0f || bullet->pos.x >= 384.0f) {
        bullet->angle = -bullet->angle - MATH_PI;
        bullet->angle = Angle_AddNormalize(bullet->angle, 0.0f);
    }
    if (bullet->pos.y < 0.0f
        || (bullet->pos.y >= 448.0f
            && (bullet->running & BULLET_FLAG_BOUNCE) != 0)) {
        bullet->angle = -bullet->angle;
    }
    bullet->speed = bullet->bounce.speed;
    M_SetVelocity(
        &bullet->velocity, bullet->angle, bullet->speed * g_GameSpeed);
    bullet->bounce.count++;
    if (bullet->bounce.count >= bullet->bounce.max) {
        bullet->running &=
            (uint16_t)~(BULLET_FLAG_BOUNCE | BULLET_FLAG_BOUNCE_NO_BOTTOM);
    }
}

static void M_RunBehaviours(BULLET *const bullet)
{
    if ((bullet->running & BULLET_FLAG_START_FAST) != 0) {
        M_StartFast(bullet);
    }
    if ((bullet->running & BULLET_FLAG_ACCELERATE) != 0) {
        M_Accelerate(bullet);
    }
    if ((bullet->running & BULLET_FLAG_CURVE) != 0) {
        M_Curve(bullet);
    }
    if ((bullet->running & BULLET_FLAG_TURN) != 0) {
        M_Turn(bullet, BULLET_FLAG_TURN);
    }
    if ((bullet->running & BULLET_FLAG_TURN_TO) != 0) {
        M_Turn(bullet, BULLET_FLAG_TURN_TO);
    }
    if ((bullet->running & BULLET_FLAG_TURN_AT_PLAYER) != 0) {
        M_Turn(bullet, BULLET_FLAG_TURN_AT_PLAYER);
    }
    if ((bullet->running & (BULLET_FLAG_BOUNCE | BULLET_FLAG_BOUNCE_NO_BOTTOM))
        != 0) {
        M_Bounce(bullet);
    }
}

// Returns false when the bullet left the play area for good.
static bool M_TestBounds(BULLET *const bullet)
{
    const uint16_t turns = BULLET_FLAG_TURN | BULLET_FLAG_TURN_AT_PLAYER
        | BULLET_FLAG_TURN_TO | BULLET_FLAG_BOUNCE
        | BULLET_FLAG_BOUNCE_NO_BOTTOM;
    const ANM_SPRITE *const sprite = bullet->sprites.flying.sprite_data;
    if (GameManager_IsInPlayArea(
            bullet->pos.x, bullet->pos.y, sprite->width, sprite->height)) {
        bullet->out_of_bounds_frames = 0;
        return true;
    }
    if ((bullet->running & turns) != 0) {
        bullet->out_of_bounds_frames++;
        return bullet->out_of_bounds_frames < M_OUT_OF_BOUNDS_FRAMES;
    }
    if (bullet->out_of_bounds_frames == 0) {
        return false;
    }
    bullet->out_of_bounds_frames--;
    return true;
}

static void M_Clear(BULLET *const bullet)
{
    bullet->state = BULLET_FADING;
    Item_Spawn(bullet->pos, g_Player.clear_item, ITEM_HOMING);
}

static void M_TestPlayer(BULLET *const bullet)
{
    const bool resists = (bullet->flags & BULLET_FLAG_RESIST_CLEAR) != 0;
    if (!bullet->grazed && bullet->graze_timer.current >= M_GRAZE_FRAMES) {
        const PLAYER_CONTACT graze =
            Player_CheckGraze(bullet->pos, bullet->sprites.hitbox_size);
        if (graze == PLAYER_CONTACT_CLEARED && !resists) {
            M_Clear(bullet);
        }
        if (graze != PLAYER_CONTACT_BOX) {
            return;
        }
        bullet->grazed = true;
    }
    const PLAYER_CONTACT hit =
        Player_CheckHit(bullet->pos, bullet->sprites.hitbox_size);
    if (hit == PLAYER_CONTACT_BOX) {
        bullet->state = BULLET_FADING;
    } else if (hit == PLAYER_CONTACT_CLEARED && !resists) {
        M_Clear(bullet);
    }
}

// Moves a flying bullet. Returns false when it left the play area for
// good, which frees it.
static bool M_Fly(BULLET *const bullet)
{
    M_RunCommands(bullet);
    if (bullet->running != 0) {
        M_RunBehaviours(bullet);
    }
    if (bullet->keep_outside_frames != 0) {
        bullet->keep_outside_frames--;
    }
    bullet->pos = Vec3_Add(bullet->pos, bullet->velocity);
    if (bullet->keep_outside_frames == 0 && !M_TestBounds(bullet)) {
        return false;
    }
    M_TestPlayer(bullet);
    if (M_IsRunning(&bullet->sprites.flying)) {
        Anm_Step(&bullet->sprites.flying);
    }
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
        Timer_SubAt(&bullet->graze_timer, 1, g_GameSpeed);
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

static CHAIN_STATUS M_Update(void *const arg)
{
    Item_Update();
    g_Bullets.count = 0;
    for (int32_t i = 0; i < BULLET_SIZE_CLASS_COUNT; i++) {
        g_Bullets.draw_lists[i] = nullptr;
    }
    // Walks bullet 0 first, then the rest from the last down.
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet =
            &g_Bullets.bullets[i == 0 ? 0 : BULLET_COUNT - i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        g_Bullets.count++;
        if (!M_UpdateBullet(bullet)) {
            M_Free(bullet);
            continue;
        }
        Timer_TickAt(&bullet->timer, g_GameSpeed);
        Timer_TickAt(&bullet->graze_timer, g_GameSpeed);
        BULLET **const list = &g_Bullets.draw_lists[bullet->sprites.size_class];
        bullet->next_drawn = *list;
        *list = bullet;
    }
    if (g_Bullets.clear_frames != 0) {
        g_Bullets.clear_frames--;
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    return CHAIN_CONTINUE;
}

static void M_SetHitbox(BULLET_SPRITES *const type, const int32_t script)
{
    const float height = type->flying.sprite_data->height;
    float size;
    if (height <= 8.0f) {
        size = 4.0f;
        type->size_class = 5;
    } else if (height <= 16.0f) {
        switch (script) {
        case 0x202:
        case 0x204:
        case 0x205:
        case 0x206:
            size = 4.0f;
            type->size_class = 4;
            break;
        default:
            size = 6.0f;
            type->size_class = 3;
            break;
        }
    } else if (height <= 32.0f) {
        switch (script) {
        case 0x208:
            size = 5.0f;
            type->size_class = 1;
            break;
        case 0x209:
            size = 8.0f;
            type->size_class = 2;
            break;
        default:
            size = 10.0f;
            type->size_class = 2;
            break;
        }
    } else {
        size = 24.0f;
        type->size_class = 0;
    }
    type->hitbox_size.x = size;
    type->hitbox_size.y = size;
}

static RESULT M_Added(void *const arg)
{
    if (!M_KeepsSprites()) {
        MUST(Anm_Load(M_ANM_SLOT, "data/etama.anm", M_FIRST_SPRITE));
    }
    for (int32_t i = 0; i < BULLET_TYPE_COUNT; i++) {
        BULLET_SPRITES *const type = &g_Bullets.types[i];
        const int32_t *const scripts = m_TypeScripts[i];
        Anm_StartScript(&type->flying, scripts[M_SCRIPT_FLYING]);
        for (int32_t j = 0; j < BULLET_SPAWN_SPRITE_COUNT; j++) {
            Anm_StartScript(&type->spawning[j], scripts[M_SCRIPT_SPAWNING + j]);
        }
        Anm_StartScript(&type->fading, scripts[M_SCRIPT_FADING]);
        type->flying.z_write_off = true;
        for (int32_t j = 0; j < BULLET_SPAWN_SPRITE_COUNT; j++) {
            type->spawning[j].z_write_off = true;
        }
        type->fading.z_write_off = true;
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
    if (!M_KeepsSprites()) {
        for (int32_t i = 0; i < M_ANM_SLOT_COUNT; i++) {
            Anm_Release(M_ANM_SLOT + i);
        }
    }
}

// Clears g_Bullets with memset: a compound literal of its size goes through
// the stack, which overflows Windows' 1 MB.
static void M_Reset(void)
{
    memset(&g_Bullets, 0, sizeof(g_Bullets));
    g_Bullets.clear_item = ITEM_STAR;
}

// Ends laser, which is not ending yet. Drops a clear item at its
// position with at_origin, and every 32 pixels along it with along.
static void M_EndLaser(
    LASER *const laser, const bool at_origin, const bool along)
{
    laser->state = LASER_ENDING;
    Timer_Set(&laser->timer, 0);
    laser->width = laser->shown_width;
    if (at_origin) {
        Item_Spawn(laser->pos, g_Bullets.clear_item, ITEM_HOMING);
    }
    if (!along) {
        return;
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
        Item_Spawn(pos, g_Bullets.clear_item, ITEM_HOMING);
    }
}

// Clears every bullet that is not fading, and ends every laser, except
// those with LASER_FLAG_RESIST_CLEAR unless all_lasers.
static void M_ClearAll(const bool to_items, const bool all_lasers)
{
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE || bullet->state == BULLET_FADING) {
            continue;
        }
        if (to_items) {
            Item_Spawn(bullet->pos, g_Bullets.clear_item, ITEM_HOMING);
            *bullet = (BULLET) {};
        } else {
            bullet->state = BULLET_FADING;
        }
    }
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        LASER *const laser = &g_Bullets.lasers[i];
        if (!laser->active
            || ((laser->flags & LASER_FLAG_RESIST_CLEAR) != 0 && !all_lasers)) {
            continue;
        }
        if (laser->state < LASER_ENDING) {
            M_EndLaser(laser, false, to_items);
        }
        laser->end_hit_frames = 0;
    }
    g_Bullets.clear_frames = M_CLEAR_FRAMES;
}

RESULT Bullet_Register(void)
{
    M_Reset();
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
    memset(&g_Bullets, 0, sizeof(g_Bullets));
}

void Bullet_FirePattern(const BULLET_SHOOTER *const shooter)
{
    if (g_Bullets.count >= BULLET_COUNT) {
        return;
    }
    ASSERT(shooter->type >= 0 && shooter->type < BULLET_TYPE_COUNT);
    const BULLET_SPRITES *const type = &g_Bullets.types[shooter->type];
    const float aim_angle = Player_AngleFrom(shooter->pos);
    bool spawning = true;
    for (int32_t row = 0; spawning && row < shooter->rows; row++) {
        for (int32_t col = 0; spawning && col < shooter->columns; col++) {
            spawning = M_Spawn(shooter, type, col, row, aim_angle);
        }
    }
    if ((shooter->flags & BULLET_FLAG_SOUND) != 0) {
        Sound_Queue(shooter->sound);
    }
}

void Bullet_ClearAll(void)
{
    M_ClearAll(false, false);
}

void Bullet_ClearAllAndKeptLasers(void)
{
    M_ClearAll(false, true);
}

void Bullet_ClearAllToItems(void)
{
    M_ClearAll(true, false);
}

int32_t Bullet_ClearAllWithBonus(const int32_t max_bonus, const bool to_items)
{
    int32_t total = 0;
    int32_t score = M_FIRST_BONUS;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        Item_Spawn(bullet->pos, g_Bullets.clear_item, ITEM_HOMING);
        AsciiText_AddPopup(
            bullet->pos, score, score >= max_bonus ? 0xffffff00u : 0xffffffffu);
        total += score;
        score += M_BONUS_STEP;
        if (score > max_bonus) {
            score = max_bonus;
        }
        bullet->state = BULLET_FADING;
    }
    for (int32_t i = 0; i < LASER_COUNT; i++) {
        LASER *const laser = &g_Bullets.lasers[i];
        if (!laser->active) {
            continue;
        }
        if (laser->state < LASER_ENDING) {
            M_EndLaser(laser, to_items, to_items);
        }
        laser->end_hit_frames = 0;
    }
    g_Bullets.clear_frames = M_CLEAR_FRAMES;
    return total;
}

void Bullet_ClearCircleToItems(const VEC3 pos, const float radius)
{
    const float radius_sq = radius * radius;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE || bullet->state == BULLET_FADING) {
            continue;
        }
        const VEC3 v = Vec3_Sub(bullet->pos, pos);
        if (Vec3_Dot(v, v) > radius_sq) {
            continue;
        }
        Item_Spawn(bullet->pos, ITEM_STAR, ITEM_HOMING);
        *bullet = (BULLET) {};
    }
}

void Bullet_StopAll(void)
{
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (bullet->state == BULLET_FREE) {
            continue;
        }
        bullet->velocity = (VEC3) {};
        bullet->speed = 0.0f;
        bullet->color = 0;
        Anm_SetSprite(
            &bullet->sprites.flying,
            bullet->sprites.flying.base_sprite + bullet->color);
    }
}

int32_t Bullet_Count(void)
{
    return g_Bullets.count;
}

void BulletShooter_Init(BULLET_SHOOTER *const shooter)
{
    *shooter = (BULLET_SHOOTER) {};
    shooter->bullet_sound = -1;
}

void BulletShooter_SetCommand(
    BULLET_SHOOTER *const shooter, const int32_t index,
    const BULLET_COMMAND command)
{
    ASSERT(index >= 0 && index < BULLET_COMMAND_COUNT);
    shooter->commands[index] = command;
    shooter->flags |= (int32_t)command.flag;
}

void Bullet_SetCommand(
    BULLET *const bullet, const int32_t index, const BULLET_COMMAND command)
{
    ASSERT(index >= 0 && index < BULLET_COMMAND_COUNT);
    bullet->commands[index] = command;
    bullet->flags |= (uint16_t)command.flag;
    bullet->next_command = 0;
}
