#include "th07/effect.h"

#include "th07/game.h"
#include "th07/stage.h"
#include "th07/stubs.h"

#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/rng.h"

#define M_CALC_PRIORITY 11
#define M_DRAW_PRIORITY 9
#define M_FIRST_SLOT 0x11
#define M_SLOT_COUNT 4
#define M_PETAL_SPRITE 0x2d8
#define M_RUN_COUNT (EFFECT_COUNT + EFFECT_RESERVED_COUNT)
#define M_PI 3.1415927f
#define M_TWO_PI 6.2831855f

typedef enum {
    M_LIST_PLAY_AREA,
    M_LIST_PETALS,
    M_LIST_PROJECTED,
    M_LIST_PLAY_AREA_ADD, // draw list 0 with additive blending
    M_LIST_COUNT,
} M_LIST;

typedef struct {
    EFFECT *effects[M_RUN_COUNT];
    int32_t count;
} M_DRAW_LIST;

static const int16_t m_Scripts[] = {
    [EFFECT_EXPLOSION] = 0x2ab,
    [EFFECT_EXPLOSION_SPIN] = 0x2ac,
    [EFFECT_EXPLOSION_LARGE] = 0x2ad,
    [EFFECT_BOMB_SPARK] = 0x2ae,
    [EFFECT_PARTICLE + 0] = 0x2b3,
    [EFFECT_PARTICLE + 1] = 0x2b4,
    [EFFECT_PARTICLE + 2] = 0x2b5,
    [EFFECT_PARTICLE + 3] = 0x2b6,
    [EFFECT_PARTICLE + 4] = 0x2b7,
    [EFFECT_PARTICLE + 5] = 0x2b8,
    [EFFECT_PARTICLE + 6] = 0x2b9,
    [EFFECT_PARTICLE + 7] = 0x2ba,
    [EFFECT_SHOCKWAVE] = 0x2bb,
    [EFFECT_ORBIT + 0] = 0x2bc,
    [EFFECT_ORBIT + 1] = 0x2bc,
    [EFFECT_ORBIT + 2] = 0x2bc,
    [EFFECT_SPELL_BACKGROUND] = 0x2dc,
    [EFFECT_CHARGE] = 0x2af,
    [EFFECT_CHARGE_LONG] = 0x2b0,
    [EFFECT_SPARK] = 0x2bd,
    [EFFECT_PETAL] = 0x2bf,
    [EFFECT_KNIFE_BOMB] = 0x2c3,
    [EFFECT_BURST] = 0x2c0,
    [EFFECT_AT_CAMERA] = 0x304,
    [EFFECT_PLAYER_AURA] = 0x2c2,
    [EFFECT_SPELL_CIRCLE] = 0x2da,
    [EFFECT_PETAL_RISING] = 0x2bf,
    [EFFECT_PETAL_SINKING] = 0x2bf,
    [EFFECT_BORDER] = 0x2db,
    [EFFECT_DIVERGE] = 0x2b2,
    [EFFECT_PETAL_DRIFT] = 0x2bf,
    [EFFECT_PETAL_FALLING] = 0x2bf,
    [EFFECT_BURST + 10] = 0x2c1,
    [EFFECT_CHARGE_ALT] = 0x2b1,
};

static EFFECT m_Effects[M_RUN_COUNT + 1];
static int32_t m_Next;
static M_DRAW_LIST m_Lists[M_LIST_COUNT];
// The original's constructor sets it to 1 and every reset zeroes it, so
// EFFECT_PETAL draws transparent.
static float m_ColorScale[4];
static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;

// Returns v / f as D3DX does: multiplied by the reciprocal of f.
static VEC3 M_Div(const VEC3 v, const float f)
{
    return Vec3_Scale(v, 1.0f / f);
}

static VEC3 M_Neg(const VEC3 v)
{
    return (VEC3) { -v.x, -v.y, -v.z };
}

static float M_RandomAngle(void)
{
    return Rng_F32(&g_Rng) * M_TWO_PI - M_PI;
}

static VEC3 M_CameraCenter(void)
{
    const STAGE_CAMERA *const camera = &g_Stage.camera.current;
    return Vec3_Add(M_Div(camera->at, 2.0f), camera->eye);
}

static void M_InitDrift(EFFECT *const effect)
{
    effect->step.x = (Rng_F32(&g_Rng) * 256.0f - 128.0f) / 12.0f;
    effect->step.y = (Rng_F32(&g_Rng) * 256.0f - 128.0f) / 12.0f;
    effect->step.z = 0.0f;
    effect->acceleration = M_Div(M_Neg(effect->step), 19.0f);
    effect->step = Vec3_Scale(effect->step, g_GameSpeed);
    effect->acceleration = Vec3_Scale(effect->acceleration, g_GameSpeed);
}

static void M_InitDriftFast(EFFECT *const effect)
{
    effect->step.x = (Rng_F32(&g_Rng) * 256.0f - 128.0f) * 4.0f / 33.0f;
    effect->step.y = (Rng_F32(&g_Rng) * 256.0f - 128.0f) * 4.0f / 33.0f;
    effect->step.z = 0.0f;
    effect->acceleration = M_Div(M_Neg(effect->step), 20.0f);
    effect->step = Vec3_Scale(effect->step, g_GameSpeed);
    effect->acceleration = Vec3_Scale(effect->acceleration, g_GameSpeed);
}

static void M_InitConverge(EFFECT *const effect)
{
    effect->origin = effect->pos;
    effect->origin.z = 0.0f;
    const float angle = M_RandomAngle();
    effect->axis = (VEC3) { X87_Cos(angle), X87_Sin(angle), 0.0f };
}

static void M_InitDiverge(EFFECT *const effect)
{
    const float angle = effect->velocity.x > -990.0f
        ? Angle_AddNormalize(effect->velocity.x, 0.0f)
        : M_RandomAngle();
    effect->origin = effect->pos;
    effect->origin.z = 0.0f;
    effect->axis = (VEC3) { X87_Cos(angle), X87_Sin(angle), 0.0f };
    effect->axis = Vec3_Scale(effect->axis, Rng_F32(&g_Rng) * 1.5f + 1.0f);
}

static void M_SetPetalSprite(EFFECT *const effect)
{
    Anm_SetSprite(&effect->vm, M_PETAL_SPRITE);
    effect->vm.color |= 0xffffffu;
}

// Shows the petal sprite as often as the cherry is full. The cherry counts
// from its offset, but the percentage divides by the whole maximum.
static void M_RollPetalSprite(EFFECT *const effect)
{
    int32_t percent = g_GameManager.cherry.value - g_GameManager.cherry.offset;
    percent = percent * 100 / g_GameManager.cherry.max;
    if ((uint32_t)percent >= Rng_U32InRange(&g_Rng, 100)) {
        M_SetPetalSprite(effect);
    }
}

static void M_RollPetalSpin(EFFECT *const effect)
{
    effect->vm.rotation.z = M_RandomAngle();
    effect->vm.rotation.x = Rng_F32(&g_Rng) * 0.06283186f - 0.031415928f;
}

// Keeps acceleration.z of the effect the slot held before, as the original.
static void M_InitPetal(EFFECT *const effect)
{
    const STAGE_CAMERA *const camera = &g_Stage.camera.current;
    const VEC3 back = M_Neg(camera->at);
    effect->true_pos = Vec3_Add(camera->at, camera->eye);
    effect->true_pos.x += Rng_F32(&g_Rng) * 120.0f - 60.0f + back.x / 2.0f;
    effect->true_pos.y += Rng_F32(&g_Rng) * 200.0f - 100.0f + back.y / 2.0f;
    effect->true_pos.z += Rng_F32(&g_Rng) * 100.0f - 100.0f + back.z / 2.0f;
    effect->step.x = Rng_F32(&g_Rng) * 0.06f - 0.03f + effect->velocity.x;
    effect->step.y = Rng_F32(&g_Rng) * 0.06f - 0.03f + effect->velocity.y;
    effect->step.z = Rng_F32(&g_Rng) * 0.1f + 0.03f + effect->velocity.z;
    effect->acceleration.x = Rng_F32(&g_Rng) * 0.0002f - 0.0001f;
    effect->acceleration.y = Rng_F32(&g_Rng) * 0.0002f - 0.0001f;
    effect->step = Vec3_Scale(effect->step, g_GameSpeed);
    effect->acceleration = Vec3_Scale(effect->acceleration, g_GameSpeed);
    effect->draw_list = 1;
    effect->vm.rotation.z = M_RandomAngle();
    effect->vm.rotation.x = Rng_F32(&g_Rng) * 0.031415928f - 0.015707964f;
    M_RollPetalSprite(effect);
}

// Starts a petal that circles the camera's view at velocity.x frames a
// radian.
static void M_InitPetalCircling(EFFECT *const effect, const bool rising)
{
    effect->true_pos.x = Rng_F32(&g_Rng) * 160.0f - 80.0f;
    effect->true_pos.y = Rng_F32(&g_Rng) * 160.0f - 80.0f;
    effect->true_pos.z = Rng_F32(&g_Rng) * 100.0f - 50.0f;
    effect->step.x = -effect->true_pos.y / effect->velocity.x;
    effect->step.y = effect->true_pos.x / effect->velocity.x;
    effect->step.z = rising ? Rng_F32(&g_Rng) * 0.1f + 0.09f
                            : -(Rng_F32(&g_Rng) * 0.2f) - 0.06f;
    effect->true_pos = Vec3_Add(effect->true_pos, M_CameraCenter());
    effect->step = Vec3_Scale(effect->step, g_GameSpeed);
    effect->draw_list = 1;
    M_RollPetalSpin(effect);
    if (rising) {
        M_RollPetalSprite(effect);
    } else {
        M_SetPetalSprite(effect);
    }
    effect->acceleration = (VEC3) {};
}

// The drift's step is not scaled by the game speed, as the original.
static void M_InitPetalDrift(EFFECT *const effect)
{
    effect->true_pos.x = Rng_F32(&g_Rng) * 160.0f - 80.0f;
    effect->true_pos.y = Rng_F32(&g_Rng) * 160.0f - 80.0f;
    effect->true_pos.z = Rng_F32(&g_Rng) * 100.0f - 100.0f;
    effect->step.x = Rng_F32(&g_Rng) * 0.06f - 0.03f + effect->velocity.x;
    effect->step.y = Rng_F32(&g_Rng) * 0.06f - 0.03f + effect->velocity.y;
    effect->step.z = Rng_F32(&g_Rng) * 0.02f + 0.01f + effect->velocity.z;
    effect->true_pos = Vec3_Add(effect->true_pos, M_CameraCenter());
    effect->draw_list = 1;
    M_RollPetalSpin(effect);
    M_SetPetalSprite(effect);
    effect->acceleration = (VEC3) {};
}

static void M_InitPetalFalling(EFFECT *const effect)
{
    effect->true_pos.x = Rng_F32(&g_Rng) * 160.0f - 80.0f;
    effect->true_pos.y = Rng_F32(&g_Rng) * 160.0f - 80.0f;
    effect->true_pos.z = Rng_F32(&g_Rng) * 200.0f - 0.0f;
    effect->step.x = Rng_F32(&g_Rng) * 0.06f - 0.03f + effect->velocity.x;
    effect->step.y = Rng_F32(&g_Rng) * 0.06f - 0.03f + effect->velocity.y;
    effect->step.z = -(Rng_F32(&g_Rng) * 0.1f) + effect->velocity.z;
    effect->true_pos = Vec3_Add(effect->true_pos, M_CameraCenter());
    effect->step = Vec3_Scale(effect->step, g_GameSpeed);
    effect->draw_list = 1;
    M_RollPetalSpin(effect);
    M_SetPetalSprite(effect);
    effect->vm.rotation_speed.z *= 2.0f;
    effect->acceleration = (VEC3) { 0.0f, 0.0f, -0.015f };
}

static void M_Init(EFFECT *const effect)
{
    switch (effect->type) {
    case EFFECT_BOMB_SPARK:
        M_InitDriftFast(effect);
        break;
    case EFFECT_PARTICLE + 0:
    case EFFECT_PARTICLE + 1:
    case EFFECT_PARTICLE + 2:
    case EFFECT_PARTICLE + 3:
    case EFFECT_PARTICLE + 4:
    case EFFECT_PARTICLE + 5:
    case EFFECT_PARTICLE + 6:
    case EFFECT_PARTICLE + 7:
        M_InitDrift(effect);
        break;
    case EFFECT_ORBIT + 0:
    case EFFECT_ORBIT + 1:
    case EFFECT_ORBIT + 2:
        effect->draw_list = 2;
        break;
    case EFFECT_CHARGE:
    case EFFECT_CHARGE_LONG:
    case EFFECT_DIVERGE:
    case EFFECT_CHARGE_ALT:
        M_InitConverge(effect);
        break;
    case EFFECT_BURST:
    case EFFECT_BURST + 10:
        M_InitDiverge(effect);
        break;
    case EFFECT_PETAL:
        M_InitPetal(effect);
        break;
    case EFFECT_PETAL_RISING:
        M_InitPetalCircling(effect, true);
        break;
    case EFFECT_PETAL_SINKING:
        M_InitPetalCircling(effect, false);
        break;
    case EFFECT_PETAL_DRIFT:
        M_InitPetalDrift(effect);
        break;
    case EFFECT_PETAL_FALLING:
        M_InitPetalFalling(effect);
        break;
    default:
        break;
    }
}

static void M_Move(EFFECT *const effect)
{
    effect->pos = Vec3_Add(effect->pos, effect->step);
    effect->step = Vec3_Add(effect->step, effect->acceleration);
}

// Circles origin at radius about axis, at angle, flattened in z. Once
// fade_out is set, fades out and grows over 16 frames, then ends.
static bool M_Orbit(EFFECT *const effect)
{
    const VEC3 n = Vec3_Normalize(effect->axis);
    const float s = X87_Sin(effect->angle);
    const float c = X87_Cos(effect->angle);
    effect->rotation = (VEC4) { n.x * s, n.y * s, n.z * s, c };
    const MAT4 m = Mat4_RotationQuaternion(effect->rotation);
    // The cross product of n and (0, 0, 1), written out as the original
    // does.
    VEC3 v = {
        n.y * 1.0f - n.z * 0.0f,
        n.z * 0.0f - n.x * 1.0f,
        n.x * 0.0f - n.y * 0.0f,
    };
    if (!(Vec3_Dot(v, v) < 0.00001f)) {
        v = Vec3_Normalize(v);
    }
    v = Vec3_Scale(v, effect->radius);
    v = Vec3_TransformCoord(v, &m);
    v.z *= 6.0f;
    effect->pos = Vec3_Add(effect->origin, v);
    if (effect->fade_out) {
        effect->fade_frames++;
        if (effect->fade_frames >= 16) {
            return false;
        }
        const float alpha = 1.0f - (float)effect->fade_frames / 16.0f;
        effect->vm.color = (effect->vm.color & 0xffffffu)
            | ((uint32_t)X87_Ftol(alpha * 255.0f) << 24);
        effect->vm.scale_y = 2.0f - alpha;
        effect->vm.scale_x = effect->vm.scale_y;
    }
    return true;
}

// Places the effect distance along axis from origin.
static void M_Along(EFFECT *const effect, const float distance)
{
    effect->pos = Vec3_Add(Vec3_Scale(effect->axis, distance), effect->origin);
}

// Ends a petal that leaves the stage camera's view or reaches z 0.
static bool M_Petal(EFFECT *const effect)
{
    const STAGE_CAMERA *const camera = &g_Stage.camera.current;
    effect->step = Vec3_Add(effect->step, effect->acceleration);
    effect->true_pos = Vec3_Add(effect->true_pos, effect->step);
    effect->pos = effect->true_pos;
    const VEC3 v = Vec3_Normalize(Vec3_Sub(effect->pos, camera->eye));
    if (Vec3_Dot(camera->dir, v) < 0.94f) {
        return false;
    }
    effect->vm.rotation.z =
        Angle_AddNormalize(effect->vm.rotation.z, effect->vm.rotation.x);
    effect->vm.rotation_changed = true;
    return effect->pos.z < 0.0f;
}

// Returns false when the effect ends.
static bool M_UpdateEffect(EFFECT *const effect)
{
    const float time = Timer_AsFloat(&effect->timer);
    switch (effect->type) {
    case EFFECT_BOMB_SPARK:
    case EFFECT_PARTICLE + 0:
    case EFFECT_PARTICLE + 1:
    case EFFECT_PARTICLE + 2:
    case EFFECT_PARTICLE + 3:
    case EFFECT_PARTICLE + 4:
    case EFFECT_PARTICLE + 5:
    case EFFECT_PARTICLE + 6:
    case EFFECT_PARTICLE + 7:
        M_Move(effect);
        return true;
    case EFFECT_ORBIT + 0:
    case EFFECT_ORBIT + 1:
    case EFFECT_ORBIT + 2:
        return M_Orbit(effect);
    case EFFECT_CHARGE:
    case EFFECT_CHARGE_ALT:
        M_Along(effect, 256.0f - time * 256.0f / 60.0f);
        effect->pos.z = 0.0f;
        return true;
    case EFFECT_CHARGE_LONG:
        M_Along(effect, 256.0f - time * 256.0f / 240.0f);
        return true;
    case EFFECT_DIVERGE:
        M_Along(effect, time * 256.0f / 30.0f);
        return true;
    case EFFECT_BURST:
    case EFFECT_BURST + 10: {
        float f = time / 90.0f;
        f = 1.0f - (1.0f - f) * (1.0f - f);
        effect->pos = Vec3_Add(
            Vec3_Scale(Vec3_Scale(effect->axis, f), 128.0f), effect->origin);
        effect->pos.z = 0.0f;
        return true;
    }
    case EFFECT_AT_CAMERA:
        effect->true_pos =
            Vec3_Add(g_Stage.camera.current.at, g_Stage.camera.current.eye);
        effect->pos = effect->true_pos;
        effect->pos.z = 0.0f;
        effect->draw_list = 3;
        return true;
    case EFFECT_PLAYER_AURA:
        if (effect->vm.script == nullptr || effect->vm.pc < 0) {
            return false;
        }
        effect->pos = g_Player.pos;
        return true;
    case EFFECT_PETAL:
    case EFFECT_PETAL_RISING:
    case EFFECT_PETAL_SINKING:
    case EFFECT_PETAL_DRIFT:
    case EFFECT_PETAL_FALLING:
        return M_Petal(effect);
    default:
        return true;
    }
}

static M_LIST M_ListOf(const EFFECT *const effect)
{
    switch (effect->draw_list) {
    case 0:
        return effect->vm.blend_add ? M_LIST_PLAY_AREA_ADD : M_LIST_PLAY_AREA;
    case 1:
    case 3:
        return M_LIST_PETALS;
    default:
        return M_LIST_PROJECTED;
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    for (int32_t i = 0; i < M_LIST_COUNT; i++) {
        m_Lists[i].count = 0;
    }
    for (int32_t i = 0; i < M_RUN_COUNT; i++) {
        EFFECT *const effect = &m_Effects[i];
        if (!effect->active) {
            continue;
        }
        if (!M_UpdateEffect(effect) || Anm_Step(&effect->vm)) {
            effect->active = false;
            continue;
        }
        Timer_TickAt(&effect->timer, g_GameSpeed);
        M_DRAW_LIST *const list = &m_Lists[M_ListOf(effect)];
        list->effects[list->count++] = effect;
    }
    return CHAIN_CONTINUE;
}

static ANM_DRAW M_DrawContext(void)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view =
        Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
    draw.interpolate = true;
    return draw;
}

static void M_DrawInPlayArea(
    const M_DRAW_LIST *const list, const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < list->count; i++) {
        EFFECT *const effect = list->effects[i];
        effect->vm.pos = effect->pos;
        effect->vm.pos.x = g_GameManager.play_area.x + effect->vm.pos.x;
        effect->vm.pos.y = g_GameManager.play_area.y + effect->vm.pos.y;
        Anm_Draw(&effect->vm, draw);
    }
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    const ANM_DRAW draw = M_DrawContext();
    M_DrawInPlayArea(&m_Lists[M_LIST_PLAY_AREA], &draw);
    const M_DRAW_LIST *const projected = &m_Lists[M_LIST_PROJECTED];
    for (int32_t i = 0; i < projected->count; i++) {
        EFFECT *const effect = projected->effects[i];
        effect->vm.pos = effect->pos;
        Anm_DrawProjected(&effect->vm, &draw);
    }
    M_DrawInPlayArea(&m_Lists[M_LIST_PLAY_AREA_ADD], &draw);
    return CHAIN_CONTINUE;
}

static uint8_t M_ScaleChannel(
    const uint32_t color, const int32_t shift, const float scale)
{
    const int32_t t = X87_Ftol((float)(uint8_t)(color >> shift) * scale);
    return (uint8_t)(t > 0xff ? 0xff : t);
}

static uint32_t M_ScaleColor(const uint32_t color)
{
    return (uint32_t)M_ScaleChannel(color, 16, m_ColorScale[0]) << 16
        | (uint32_t)M_ScaleChannel(color, 8, m_ColorScale[1]) << 8
        | (uint32_t)M_ScaleChannel(color, 0, m_ColorScale[2])
        | (uint32_t)M_ScaleChannel(color, 24, m_ColorScale[3]) << 24;
}

static void M_Reset(void)
{
    for (int32_t i = 0; i <= M_RUN_COUNT; i++) {
        m_Effects[i] = (EFFECT) {};
    }
    m_Next = 0;
    for (int32_t i = 0; i < M_LIST_COUNT; i++) {
        m_Lists[i].count = 0;
    }
    for (int32_t i = 0; i < 4; i++) {
        m_ColorScale[i] = 0.0f;
    }
}

static RESULT M_Added(void *const arg)
{
    M_Reset();
    g_Stage.spell_background.offset = 0;
    switch (g_GameManager.run.stage) {
    case 0:
    case 1:
        g_Stage.spell_background.count = 1;
        return Anm_Load(0x11, "data/eff01.anm", 0x2dc);
    case 2:
        g_Stage.spell_background.count = 1;
        return Anm_Load(0x11, "data/eff02.anm", 0x2dc);
    case 3:
        g_Stage.spell_background.count = 1;
        return Anm_Load(0x11, "data/eff03.anm", 0x2dc);
    case 4:
        g_Stage.spell_background.count = 2;
        MUST(Anm_Load(0x11, "data/eff04.anm", 0x2dc));
        return Anm_Load(0x12, "data/eff04b.anm", 0x2dd);
    case 5:
        g_Stage.spell_background.count = 2;
        return Anm_Load(0x11, "data/eff05.anm", 0x2dc);
    case 6:
        g_Stage.spell_background.count = 2;
        MUST(Anm_Load(0x11, "data/eff05.anm", 0x2dc));
        return Anm_Load(0x13, "data/eff06.anm", 0x2de);
    case 7:
        g_Stage.spell_background.count = 1;
        MUST(Anm_Load(0x11, "data/eff02.anm", 0x2dc));
        return Anm_Load(0x12, "data/eff07.anm", 0x2dd);
    case 8:
        g_Stage.spell_background.count = 2;
        MUST(Anm_Load(0x11, "data/eff07.anm", 0x2dc));
        return Anm_Load(0x13, "data/eff08.anm", 0x2de);
    default:
        return OK;
    }
}

static void M_Deleted(void *const arg)
{
    for (int32_t i = 0; i < M_SLOT_COUNT; i++) {
        Anm_Release(M_FIRST_SLOT + i);
    }
}

// Starts effect as type, with the order of the original's spawn functions.
static void M_Start(
    EFFECT *const effect, const EFFECT_TYPE type, const VEC3 pos,
    const VEC3 velocity, const bool z_write_off, const uint32_t color)
{
    effect->draw_list = 0;
    effect->active = true;
    effect->type = (int8_t)type;
    effect->pos = pos;
    Anm_StartScript(&effect->vm, m_Scripts[type]);
    if (z_write_off) {
        effect->vm.z_write_off = true;
    }
    effect->vm.color = color;
    Timer_Set(&effect->timer, 0);
    effect->fade_out = false;
    effect->fade_frames = 0;
    effect->velocity = velocity;
    M_Init(effect);
}

static EFFECT *M_Spawn(
    const EFFECT_TYPE type, const VEC3 pos, const VEC3 velocity,
    const bool z_write_off, int32_t count, const uint32_t color)
{
    EFFECT *effect = &m_Effects[m_Next];
    int32_t tries;
    for (tries = 0; tries < EFFECT_COUNT; tries++) {
        m_Next++;
        if (m_Next >= EFFECT_COUNT) {
            m_Next = 0;
        }
        if (!effect->active) {
            M_Start(effect, type, pos, velocity, z_write_off, color);
            count--;
            if (count == 0) {
                break;
            }
        }
        effect = m_Next == 0 ? m_Effects : effect + 1;
    }
    return tries >= EFFECT_COUNT ? &m_Effects[M_RUN_COUNT] : effect;
}

RESULT Effect_Register(void)
{
    M_Reset();
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, nullptr);
    return Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY);
}

void Effect_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
}

EFFECT *Effect_Spawn(
    const EFFECT_TYPE type, const VEC3 pos, const int32_t count,
    const uint32_t color)
{
    return M_Spawn(type, pos, (VEC3) {}, true, count, color);
}

EFFECT *Effect_SpawnMoving(
    const EFFECT_TYPE type, const VEC3 pos, const VEC3 velocity,
    const int32_t count, const uint32_t color)
{
    return M_Spawn(type, pos, velocity, false, count, color);
}

EFFECT *Effect_SpawnReserved(
    const EFFECT_TYPE type, const VEC3 pos, const int32_t slot,
    const uint32_t color)
{
    EFFECT *const effect = &m_Effects[EFFECT_COUNT + slot];
    M_Start(effect, type, pos, (VEC3) {}, true, color);
    return effect;
}

// Moves inactive effects of those types too, as the original.
void Effect_OffsetPetals(const VEC3 offset)
{
    for (int32_t i = 0; i < EFFECT_COUNT; i++) {
        EFFECT *const effect = &m_Effects[i];
        if (effect->type == EFFECT_PETAL
            || effect->type == EFFECT_PETAL_FALLING) {
            effect->true_pos = Vec3_Add(effect->true_pos, offset);
        }
    }
}

void Effect_StartPetalsFalling(void)
{
    for (int32_t i = 0; i < EFFECT_COUNT; i++) {
        if (m_Effects[i].type == EFFECT_PETAL_DRIFT) {
            m_Effects[i].acceleration.z = -0.01f;
        }
    }
}

void Effect_DrawPetals(void)
{
    // Quality 1 draws none either: the original stops at the first odd
    // petal count, which is that of the first petal.
    if (g_EffectsQuality <= 1) {
        return;
    }
    const ANM_DRAW draw = M_DrawContext();
    const M_DRAW_LIST *const list = &m_Lists[M_LIST_PETALS];
    for (int32_t i = 0; i < list->count; i++) {
        EFFECT *const effect = list->effects[i];
        const uint32_t color = effect->vm.color;
        if (effect->type == EFFECT_PETAL) {
            effect->vm.color = M_ScaleColor(color);
        }
        effect->vm.pos = effect->pos;
        if (effect->draw_list == 1) {
            Anm_DrawProjected(&effect->vm, &draw);
        } else {
            Anm_DrawProjected3D(&effect->vm, &draw);
        }
        effect->vm.color = color;
    }
}
