#include "th06/effect.h"

#include "th06/chain.h"
#include "th06/game.h"
#include "th06/game_manager.h"

#include "core/math/x87.h"

#include "stg/rng.h"

#define M_ANM_SLOT 11
#define M_FIRST_SPRITE 0x2b3
#define M_CALC_PRIORITY 10
#define M_DRAW_PRIORITY 8
#define M_PI 3.1415927f
#define M_TWO_PI 6.2831855f

typedef struct {
    int32_t script;
    bool (*update)(EFFECT *effect);
} M_TYPE;

static EFFECT m_Effects[EFFECT_COUNT + 1];
static int32_t m_Next;
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

static bool M_IsFirstFrame(const EFFECT *const effect)
{
    return effect->timer.current == 0 && Timer_HasTicked(&effect->timer);
}

static bool M_Move(EFFECT *const effect)
{
    effect->pos = Vec3_Add(effect->pos, effect->velocity);
    effect->velocity = Vec3_Add(effect->velocity, effect->acceleration);
    return true;
}

// Picks a random drift on the first frame that slows to a stop over 19
// frames, as particles do.
static bool M_Drift(EFFECT *const effect)
{
    if (M_IsFirstFrame(effect)) {
        effect->velocity.x = (Rng_F32(&g_Rng) * 256.0f - 128.0f) / 12.0f;
        effect->velocity.y = (Rng_F32(&g_Rng) * 256.0f - 128.0f) / 12.0f;
        effect->velocity.z = 0.0f;
        effect->acceleration = M_Div(M_Neg(effect->velocity), 19.0f);
    }
    return M_Move(effect);
}

// Picks a faster random drift on the first frame that slows to a stop
// over 20 frames, as bomb sparks do.
static bool M_DriftFast(EFFECT *const effect)
{
    if (M_IsFirstFrame(effect)) {
        effect->velocity.x = (Rng_F32(&g_Rng) * 256.0f - 128.0f) * 4.0f / 33.0f;
        effect->velocity.y = (Rng_F32(&g_Rng) * 256.0f - 128.0f) * 4.0f / 33.0f;
        effect->velocity.z = 0.0f;
        effect->acceleration = M_Div(M_Neg(effect->velocity), 20.0f);
    }
    return M_Move(effect);
}

// Circles origin at radius about axis, at angle, flattened in z. Once
// fade_out is set, fades out and grows over 16 frames, then ends.
static bool M_Orbit(EFFECT *const effect)
{
    VEC3 n = Vec3_Normalize(effect->axis);
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

// Picks a random direction on the first frame, then comes in along it
// from 256 pixels away to where it started over frames frames.
static bool M_Converge(EFFECT *const effect, const float frames)
{
    if (M_IsFirstFrame(effect)) {
        effect->origin = effect->pos;
        const float angle = Rng_F32InRange(&g_Rng, M_TWO_PI) - M_PI;
        effect->axis = (VEC3) { X87_Cos(angle), X87_Sin(angle), 0.0f };
    }
    const float distance =
        256.0f - (float)effect->timer.current * 256.0f / frames;
    effect->pos = Vec3_Add(Vec3_Scale(effect->axis, distance), effect->origin);
    return true;
}

static bool M_ConvergeFast(EFFECT *const effect)
{
    return M_Converge(effect, 60.0f);
}

static bool M_ConvergeSlow(EFFECT *const effect)
{
    return M_Converge(effect, 240.0f);
}

static const M_TYPE m_Types[] = {
    [EFFECT_EXPLOSION] = { 669, nullptr },
    [EFFECT_EXPLOSION_SPIN] = { 670, nullptr },
    [EFFECT_EXPLOSION_LARGE] = { 671, nullptr },
    [EFFECT_BOMB_SPARK] = { 672, M_DriftFast },
    [EFFECT_PARTICLE + 0] = { 675, M_Drift },
    [EFFECT_PARTICLE + 1] = { 676, M_Drift },
    [EFFECT_PARTICLE + 2] = { 677, M_Drift },
    [EFFECT_PARTICLE + 3] = { 678, M_Drift },
    [EFFECT_SMALL_PARTICLE + 0] = { 679, M_Drift },
    [EFFECT_SMALL_PARTICLE + 1] = { 680, M_Drift },
    [EFFECT_SMALL_PARTICLE + 2] = { 681, M_Drift },
    [EFFECT_SMALL_PARTICLE + 3] = { 682, M_Drift },
    [EFFECT_SHOCKWAVE] = { 683, nullptr },
    [EFFECT_ORBIT + 0] = { 684, M_Orbit },
    [EFFECT_ORBIT + 1] = { 684, M_Orbit },
    [EFFECT_ORBIT + 2] = { 684, M_Orbit },
    [EFFECT_SPELL_BACKGROUND] = { 691, nullptr },
    [EFFECT_CHARGE] = { 673, M_ConvergeFast },
    [EFFECT_CHARGE_LONG] = { 674, M_ConvergeSlow },
    [EFFECT_SPARK] = { 685, M_Move },
};

static CHAIN_STATUS M_Update(void *const arg)
{
    for (int32_t i = 0; i < EFFECT_COUNT; i++) {
        EFFECT *const effect = &m_Effects[i];
        if (!effect->active) {
            continue;
        }
        const M_TYPE *const type = &m_Types[effect->type];
        if (type->update != nullptr && !type->update(effect)) {
            effect->active = false;
        }
        if (Anm_Step(&effect->vm)) {
            effect->active = false;
        }
        Timer_Tick(&effect->timer);
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    ANM_DRAW draw = GameManager_GetDrawContext();
    draw.interpolate = true;
    for (int32_t i = 0; i < EFFECT_COUNT; i++) {
        if (m_Effects[i].active) {
            m_Effects[i].vm.pos = m_Effects[i].pos;
            Anm_Draw3D(&m_Effects[i].vm, &draw);
        }
    }
    return CHAIN_CONTINUE;
}

static void M_Reset(void)
{
    for (int32_t i = 0; i <= EFFECT_COUNT; i++) {
        m_Effects[i] = (EFFECT) {};
    }
    m_Next = 0;
}

static RESULT M_Added(void *const arg)
{
    static const char *const paths[] = {
        "data/eff01.anm", "data/eff01.anm", "data/eff02.anm", "data/eff03.anm",
        "data/eff04.anm", "data/eff05.anm", "data/eff05.anm", "data/eff04.anm",
    };
    M_Reset();
    const int32_t stage = g_GameManager.run.stage;
    if (stage < 0 || stage > 7) {
        return OK;
    }
    return Anm_Load(M_ANM_SLOT, paths[stage], M_FIRST_SPRITE);
}

static void M_Deleted(void *const arg)
{
    Anm_Release(M_ANM_SLOT);
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
    const EFFECT_TYPE type, const VEC3 pos, int32_t count, const uint32_t color)
{
    EFFECT *effect = &m_Effects[m_Next];
    int32_t tries;
    for (tries = 0; tries < EFFECT_COUNT; tries++) {
        m_Next++;
        if (m_Next >= EFFECT_COUNT) {
            m_Next = 0;
        }
        if (!effect->active) {
            effect->active = true;
            effect->type = (int8_t)type;
            effect->pos = pos;
            Anm_StartScript(&effect->vm, m_Types[type].script);
            effect->vm.color = color;
            Timer_Set(&effect->timer, 0);
            effect->fade_out = false;
            effect->fade_frames = 0;
            count--;
            if (count == 0) {
                break;
            }
        }
        effect = m_Next == 0 ? m_Effects : effect + 1;
    }
    return tries >= EFFECT_COUNT ? &m_Effects[EFFECT_COUNT] : effect;
}
