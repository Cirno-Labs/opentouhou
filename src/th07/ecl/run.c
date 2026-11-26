#include "th07/ecl/run.h"

#include "th07/ecl/patterns.h"
#include "th07/ecl/private.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/bullet.h"
#include "th07/effect.h"
#include "th07/enemy.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/item.h"
#include "th07/player.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/screens/result/records.h"

#include "stg/translation.h"

#include "core/base/assert.h"
#include "core/base/color.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/rng.h"
#include "stg/timer.h"

#include <string.h>

#define M_MAX_DEPTH 15
#define M_FIRST_SCRIPT 0x900
#define M_SPELL_BACKGROUND_SCRIPT 0x2dc
#define M_INTERP_HERMITE 7
#define M_ORBIT_EFFECT 0xd
#define M_TURN_SPEED 0.01f
#define M_HALF_PI 1.5707964f
#define M_QUARTER_PI 0.7853982f
#define M_THREE_QUARTER_PI 2.3561945f
#define M_PI_F 3.1415927f
#define M_TWO_PI 6.2831855f
#define M_CLEAR_BONUS 8000
#define M_SPELL_CIRCLE_SCALE 0.125f
#define M_HIDDEN_SPELL_CARDS 0x76
#define M_STAGE_EXTRA 7
#define M_SOUND_SPELL_END 0xf
#define M_POWER_FULL 0x80
#define M_DROP_SPREAD 128.0f
#define M_BAR_BOSSES 4 // the bosses whose removal hides the boss bar

typedef enum {
    M_STEP_NEXT,
    M_STEP_JUMP,
    M_STEP_RESTART, // after a call, a return or an interrupt
    M_STEP_DELETE,
} M_STEP;

static const int32_t m_SpellCardBonuses[ECL_SPELL_CARD_COUNT] = {
    2000000, 2000000, 2200000, 2200000, 2400000, 2400000, 2400000, 2400000,
    2400000, 2400000, 2600000, 2600000, 2600000, 2600000, 2600000, 2600000,
    2600000, 2600000, 2600000, 2600000, 2600000, 2600000, 2600000, 2600000,
    2600000, 2600000, 3000000, 3000000, 3000000, 3000000, 3000000, 3000000,
    3000000, 3000000, 3000000, 3000000, 3000000, 3000000, 3000000, 3000000,
    3000000, 3000000, 3000000, 3000000, 3500000, 3500000, 3500000, 3500000,
    3500000, 3500000, 3500000, 3500000, 3500000, 3500000, 3500000, 3500000,
    3500000, 3500000, 3500000, 3500000, 3500000, 3500000, 3500000, 3500000,
    3500000, 3500000, 3500000, 3500000, 4000000, 4000000, 4000000, 4000000,
    4000000, 4000000, 4000000, 4000000, 4000000, 4000000, 4000000, 4000000,
    4000000, 4000000, 4000000, 4000000, 4000000, 4000000, 4000000, 4000000,
    5000000, 5000000, 5000000, 5000000, 5000000, 5000000, 5000000, 5000000,
    5000000, 5000000, 5000000, 5000000, 5000000, 5000000, 5000000, 5000000,
    5000000, 5000000, 5000000, 5000000, 5000000, 5000000, 5000000, 5000000,
    3000000, 3000000, 3000000, 3000000, 6000000, 6000000, 7000000, 7000000,
    7000000, 7000000, 7000000, 7000000, 7000000, 7000000, 4000000, 7000000,
    7000000, 7000000, 8000000, 8000000, 8000000, 8000000, 8000000, 8000000,
    8000000, 8000000, 4000000, 8000000, 4000000,
};

static const uint32_t m_OrbitColors[0x1c] = {
    COLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0),
    COLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0),
    COLOR_WHITE,
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xff),
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xff),
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xff),
    COLOR_ARGB(0xff, 0xe0, 0xe0, 0xff),
    COLOR_ARGB(0xff, 0xe0, 0xe0, 0xff),
    COLOR_ARGB(0xff, 0xe0, 0xe0, 0xff),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xff),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xff),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xff),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xe0, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xff, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xe0),
    COLOR_ARGB(0xff, 0xff, 0xe0, 0xe0),
    COLOR_WHITE,
};

// Hold the shared variables 10037 to 10044. They last across loads.
static int32_t m_SharedInts[4];
static float m_SharedFloats[4];

static int32_t M_Wrap(const int64_t value)
{
    return (int32_t)(uint32_t)(uint64_t)value;
}

static ECL_VARS *M_Vars(ENEMY *const enemy)
{
    return &enemy->script.state.vars;
}

static float M_Length(const VEC3 v)
{
    return X87_Sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Returns the value of operand id: a variable, truncated when it holds a
// float, or another value from 10000 on; any other id is its own value.
static int32_t M_GetInt(ENEMY *const enemy, const int32_t id)
{
    const ECL_VARS *const vars = M_Vars(enemy);
    if (id >= 10000 && id <= 10003) {
        return vars->ints[id - 10000];
    }
    if (id >= 10012 && id <= 10015) {
        return vars->ints[4 + id - 10012];
    }
    if (id >= 10029 && id <= 10032) {
        return vars->ints[8 + id - 10029];
    }
    if (id >= 10004 && id <= 10011) {
        return X87_Ftol(vars->floats[id - 10004]);
    }
    if (id >= 10033 && id <= 10036) {
        return X87_Ftol(vars->floats[10 + id - 10033]);
    }
    if (id >= 10037 && id <= 10040) {
        return m_SharedInts[id - 10037];
    }
    if (id >= 10041 && id <= 10044) {
        return X87_Ftol(m_SharedFloats[id - 10041]);
    }
    if (id >= 10066 && id <= 10069) {
        return enemy->callbacks.life[id - 10066];
    }
    switch (id) {
    case 10016:
        return g_GameManager.run.difficulty;
    case 10017:
        return g_GameManager.rank.value;
    case 10025:
        return enemy->callbacks.timer.current;
    case 10027:
        return enemy->life;
    case 10028:
        return g_GameManager.run.shot_index;
    case 10072:
    case 10073:
        return X87_Ftol(vars->floats[8 + id - 10072]);
    case 10018:
        return X87_Ftol(enemy->move.pos.x);
    case 10019:
        return X87_Ftol(enemy->move.pos.y);
    case 10020:
        return X87_Ftol(enemy->move.pos.z);
    case 10021:
        return X87_Ftol(g_Player.pos.x);
    case 10022:
        return X87_Ftol(g_Player.pos.y);
    case 10023:
        return X87_Ftol(g_Player.pos.z);
    case 10050:
        return X87_Ftol(enemy->move.start.x);
    case 10051:
        return X87_Ftol(enemy->move.start.y);
    case 10052:
        return X87_Ftol(enemy->move.start.z);
    case 10063:
        return X87_Ftol(enemy->move.step.x);
    case 10064:
        return X87_Ftol(enemy->move.step.y);
    case 10065:
        return X87_Ftol(enemy->move.step.z);
    case 10045:
        return X87_Ftol(enemy->move.angle);
    case 10046:
        return X87_Ftol(enemy->move.angular_velocity);
    case 10047:
        return X87_Ftol(enemy->move.speed);
    case 10048:
        return X87_Ftol(enemy->move.acceleration);
    case 10049:
        return X87_Ftol(enemy->move.circle.radius);
    case 10053:
        return X87_Ftol(enemy->move.circle.angle);
    case 10054:
        return X87_Ftol(enemy->move.circle.angular_velocity);
    case 10055:
        return (int32_t)Rng_U32(&g_Rng);
    case 10056:
        return M_Wrap(
            (int64_t)Rng_U32InRange(&g_Rng, (uint32_t)vars->ints[8])
            + vars->ints[9]);
    case 10061:
        return enemy->last_damage;
    case 10062:
        return enemy->boss_index;
    case 10070:
        return enemy->death.item;
    case 10071:
        return enemy->score;
    case 10024:
        return X87_Ftol(Player_AngleFrom(enemy->move.pos));
    case 10026:
        return X87_Ftol(M_Length(Vec3_Sub(g_Player.pos, enemy->move.pos)));
    default:
        return id;
    }
}

// Returns the int variable that operand names, or operand itself when it
// names none or when bit is not negative and clear in mask. This table
// names fewer variables than M_GetInt reads.
static int32_t *M_ResolveInt(
    ENEMY *const enemy, int32_t *const operand, const uint16_t mask,
    const int32_t bit)
{
    if (bit >= 0 && (mask & (1u << bit)) == 0) {
        return operand;
    }
    ECL_VARS *const vars = M_Vars(enemy);
    const int32_t id = *operand;
    if (id >= 10000 && id <= 10003) {
        return &vars->ints[id - 10000];
    }
    if (id >= 10029 && id <= 10032) {
        return &vars->ints[8 + id - 10029];
    }
    if (id >= 10012 && id <= 10015) {
        return &vars->ints[4 + id - 10012];
    }
    if (id >= 10037 && id <= 10040) {
        return &m_SharedInts[id - 10037];
    }
    if (id == 10016) {
        return &g_GameManager.run.difficulty;
    }
    if (id == 10017) {
        return &g_GameManager.rank.value;
    }
    if (id == 10025) {
        return &enemy->callbacks.timer.current;
    }
    if (id == 10027) {
        return &enemy->life;
    }
    if (id == 10070) {
        return &enemy->death.item;
    }
    if (id == 10071) {
        return &enemy->score;
    }
    return operand;
}

// Returns the value of float operand id as M_GetInt does, with its own
// list of ids; 10055, 10056 and 10060 draw random floats.
static float M_GetFloat(ENEMY *const enemy, const float id)
{
    const ECL_VARS *const vars = M_Vars(enemy);
    const int32_t n = X87_Ftol(id);
    if (n >= 10000 && n <= 10003) {
        return (float)vars->ints[n - 10000];
    }
    if (n >= 10029 && n <= 10032) {
        return (float)vars->ints[8 + n - 10029];
    }
    if (n >= 10012 && n <= 10015) {
        return (float)vars->ints[4 + n - 10012];
    }
    if (n >= 10037 && n <= 10040) {
        return (float)m_SharedInts[n - 10037];
    }
    if (n >= 10041 && n <= 10044) {
        return m_SharedFloats[n - 10041];
    }
    if (n >= 10004 && n <= 10011) {
        return vars->floats[n - 10004];
    }
    if (n >= 10033 && n <= 10036) {
        return vars->floats[10 + n - 10033];
    }
    if (n >= 10066 && n <= 10069) {
        return (float)enemy->callbacks.life[n - 10066];
    }
    switch (n) {
    case 10016:
        return (float)g_GameManager.run.difficulty;
    case 10017:
        return (float)g_GameManager.rank.value;
    case 10025:
        return (float)enemy->callbacks.timer.current;
    case 10027:
        return (float)enemy->life;
    case 10028:
        return (float)g_GameManager.run.shot_index;
    case 10070:
        return (float)enemy->death.item;
    case 10071:
        return (float)enemy->score;
    case 10018:
        return enemy->move.pos.x;
    case 10019:
        return enemy->move.pos.y;
    case 10020:
        return enemy->move.pos.z;
    case 10021:
        return g_Player.pos.x;
    case 10022:
        return g_Player.pos.y;
    case 10023:
        return g_Player.pos.z;
    case 10072:
    case 10073:
        return vars->floats[8 + n - 10072];
    case 10050:
        return enemy->move.start.x;
    case 10051:
        return enemy->move.start.y;
    case 10052:
        return enemy->move.start.z;
    case 10057:
        return enemy->move.delta.x;
    case 10058:
        return enemy->move.delta.y;
    case 10059:
        return enemy->move.delta.z;
    case 10063:
        return enemy->move.step.x;
    case 10064:
        return enemy->move.step.y;
    case 10065:
        return enemy->move.step.z;
    case 10024:
        return Player_AngleFrom(enemy->move.pos);
    case 10045:
        return enemy->move.angle;
    case 10046:
        return enemy->move.angular_velocity;
    case 10047:
        return enemy->move.speed;
    case 10048:
        return enemy->move.acceleration;
    case 10049:
        return enemy->move.circle.radius;
    case 10053:
        return enemy->move.circle.angle;
    case 10054:
        return enemy->move.circle.angular_velocity;
    case 10055:
        return Rng_F32(&g_Rng);
    case 10056:
        return Rng_F32InRange(&g_Rng, vars->floats[10]) + vars->floats[11];
    case 10060:
        return Rng_F32(&g_Rng) * M_TWO_PI - M_PI_F;
    case 10062:
        return (float)enemy->boss_index;
    case 10061:
        return (float)enemy->last_damage;
    case 10026:
        return M_Length(Vec3_Sub(g_Player.pos, enemy->move.pos));
    default:
        return id;
    }
}

// Returns the float variable that operand names, as M_ResolveInt does.
static float *M_ResolveFloat(
    ENEMY *const enemy, float *const operand, const uint16_t mask,
    const int32_t bit)
{
    if (bit >= 0 && (mask & (1u << bit)) == 0) {
        return operand;
    }
    ECL_VARS *const vars = M_Vars(enemy);
    const int32_t n = X87_Ftol(*operand);
    if (n >= 10004 && n <= 10011) {
        return &vars->floats[n - 10004];
    }
    if (n >= 10033 && n <= 10036) {
        return &vars->floats[10 + n - 10033];
    }
    if (n >= 10041 && n <= 10044) {
        return &m_SharedFloats[n - 10041];
    }
    switch (n) {
    case 10018:
        return &enemy->move.pos.x;
    case 10019:
        return &enemy->move.pos.y;
    case 10020:
        return &enemy->move.pos.z;
    case 10021:
        return &g_Player.pos.x;
    case 10022:
        return &g_Player.pos.y;
    case 10023:
        return &g_Player.pos.z;
    case 10072:
    case 10073:
        return &vars->floats[8 + n - 10072];
    case 10050:
        return &enemy->move.start.x;
    case 10051:
        return &enemy->move.start.y;
    case 10052:
        return &enemy->move.start.z;
    case 10057:
        return &enemy->move.delta.x;
    case 10058:
        return &enemy->move.delta.y;
    case 10059:
        return &enemy->move.delta.z;
    case 10045:
        return &enemy->move.angle;
    case 10046:
        return &enemy->move.angular_velocity;
    case 10047:
        return &enemy->move.speed;
    case 10048:
        return &enemy->move.acceleration;
    case 10049:
        return &enemy->move.circle.radius;
    case 10053:
        return &enemy->move.circle.angle;
    case 10054:
        return &enemy->move.circle.angular_velocity;
    default:
        return operand;
    }
}

static bool M_IsVar(const ECL_INSTR *const instr, const uint16_t bit)
{
    return (instr->param_mask & bit) != 0;
}

// Returns value, or the operand it names when bit is set in the mask of
// instr.
static int32_t M_IntOp(
    ENEMY *const enemy, const ECL_INSTR *const instr, const int32_t value,
    const uint16_t bit)
{
    return M_IsVar(instr, bit) ? M_GetInt(enemy, value) : value;
}

static float M_FloatOp(
    ENEMY *const enemy, const ECL_INSTR *const instr, const float value,
    const uint16_t bit)
{
    return M_IsVar(instr, bit) ? M_GetFloat(enemy, value) : value;
}

static int32_t M_IntArg(
    ENEMY *const enemy, const ECL_INSTR *const instr, const int32_t n)
{
    return M_IntOp(enemy, instr, instr->args[n].i, (uint16_t)(1u << n));
}

static float M_FloatArg(
    ENEMY *const enemy, const ECL_INSTR *const instr, const int32_t n)
{
    return M_FloatOp(enemy, instr, instr->args[n].f, (uint16_t)(1u << n));
}

static int32_t *M_IntVar(
    ENEMY *const enemy, ECL_INSTR *const instr, const int32_t n)
{
    return M_ResolveInt(enemy, &instr->args[n].i, instr->param_mask, n);
}

static float *M_FloatVar(
    ENEMY *const enemy, ECL_INSTR *const instr, const int32_t n)
{
    return M_ResolveFloat(enemy, &instr->args[n].f, instr->param_mask, n);
}

static void M_SetInt(
    ENEMY *const enemy, ECL_INSTR *const instr, const int32_t n,
    const int32_t value)
{
    *M_IntVar(enemy, instr, n) = value;
}

static void M_SetFloat(
    ENEMY *const enemy, ECL_INSTR *const instr, const int32_t n,
    const float value)
{
    *M_FloatVar(enemy, instr, n) = value;
}

static int16_t M_ArgShort(const ECL_INSTR *const instr, const int32_t index)
{
    const uint32_t word = (uint32_t)instr->args[index / 2].i;
    return (int16_t)(uint16_t)(word >> (16 * (index % 2)));
}

static uint8_t M_ArgByte(const ECL_INSTR *const instr, const int32_t index)
{
    const uint32_t word = (uint32_t)instr->args[index / 4].i;
    return (uint8_t)(word >> (8 * (index % 4)));
}

static int32_t M_RankLerp(const int32_t low, const int32_t high)
{
    return g_GameManager.rank.value * (high - low) / 32 + low;
}

static float M_RankLerpFloat(const float low, const float high)
{
    return (float)g_GameManager.rank.value * (high - low) / 32.0f + low;
}

static ENEMY *M_Boss(const int32_t index)
{
    ASSERT(index >= 0 && index < ENEMY_BOSS_COUNT);
    return g_Enemies.bosses[index];
}

static LASER **M_Laser(ENEMY *const enemy, const int32_t index)
{
    ASSERT(index >= 0 && index < ENEMY_LASER_COUNT);
    return &enemy->shots.lasers[index];
}

// Returns the slot of the call stack at depth. A depth of -1 is the
// running state itself, which the stack follows in memory in the
// original; a return at depth 0 reaches it.
static ECL_STATE *M_StackSlot(ENEMY *const enemy, const int32_t depth)
{
    ASSERT(depth >= -1 && depth < ENEMY_STACK_DEPTH);
    return depth < 0 ? &enemy->script.state : &enemy->script.stack[depth];
}

static void M_SetBossMarker(const int32_t index, const int16_t interrupt)
{
    // The original also writes the markers of bosses 4 to 7, past its
    // four.
    if (index < ASCII_BOSS_MARKER_COUNT) {
        g_AsciiText.boss_markers[index].pending_interrupt = interrupt;
    }
}

static float M_Ease(const int32_t ease, float t)
{
    switch (ease) {
    case ENEMY_EASE_IN:
        return t * t;
    case ENEMY_EASE_IN3:
        return t * t * t;
    case ENEMY_EASE_IN4:
        return t * t * t * t;
    case ENEMY_EASE_OUT:
        t = 1.0f - t;
        return 1.0f - t * t;
    case ENEMY_EASE_OUT3:
        t = 1.0f - t;
        return 1.0f - t * t * t;
    case ENEMY_EASE_OUT4:
        t = 1.0f - t;
        return 1.0f - t * t * t * t;
    default:
        return t;
    }
}

static void M_MoveAngle(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    const float angle = Angle_AddNormalize(M_FloatArg(enemy, instr, 2), 0.0f);
    // Each product rounds once: the trig result stays at extended
    // precision into the first multiply.
    float x;
    float y;
    float speed = M_FloatArg(enemy, instr, 3);
    float frames = (float)M_IntArg(enemy, instr, 0);
    X87_SinCosScale(angle, speed, &x, &y);
    enemy->move.delta.x = x * frames;
    speed = M_FloatArg(enemy, instr, 3);
    frames = (float)M_IntArg(enemy, instr, 0);
    X87_SinCosScale(angle, speed, &x, &y);
    enemy->move.delta.y = y * frames;
    enemy->move.delta.z = 0.0f;
    enemy->move.start = enemy->move.pos;
    enemy->move.duration = M_IntArg(enemy, instr, 0);
    Timer_Set(&enemy->move.timer, enemy->move.duration);
    enemy->flags.ease = (ENEMY_EASE)(M_IntArg(enemy, instr, 1) & 7);
    enemy->flags.move = ENEMY_MOVE_TO;
    if (enemy->flags.mirrored) {
        enemy->move.delta.x = -enemy->move.delta.x;
    }
}

static void M_MoveTo(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    const VEC3 target = {
        M_FloatArg(enemy, instr, 2),
        M_FloatArg(enemy, instr, 3),
        M_FloatArg(enemy, instr, 4),
    };
    enemy->move.delta = Vec3_Sub(target, enemy->move.pos);
    enemy->move.start = enemy->move.pos;
    enemy->move.duration = M_IntArg(enemy, instr, 0);
    Timer_Set(&enemy->move.timer, enemy->move.duration);
    enemy->flags.ease = (ENEMY_EASE)(M_IntArg(enemy, instr, 1) & 7);
    enemy->flags.move = ENEMY_MOVE_TO;
    enemy->move.velocity = (VEC3) {};
    if (enemy->flags.mirrored) {
        enemy->move.delta.x = -enemy->move.delta.x;
    }
}

static void M_SetMoveTimer(
    ENEMY *const enemy, const ECL_INSTR *const instr, const ENEMY_MOVE move)
{
    enemy->flags.move = move;
    enemy->move.duration = M_IntArg(enemy, instr, 0);
    Timer_Set(&enemy->move.timer, enemy->move.duration);
}

static float M_RandomAngleInBounds(const ENEMY *const enemy)
{
    float angle;
    if (g_Player.pos.x < enemy->move.pos.x) {
        angle = Angle_AddNormalize(
            Rng_F32InRange(&g_Rng, M_HALF_PI) + M_THREE_QUARTER_PI, 0.0f);
    } else {
        angle = Rng_F32InRange(&g_Rng, M_HALF_PI) - M_QUARTER_PI;
    }
    if (enemy->move.bounds_min.x + 96.0f > enemy->move.pos.x) {
        if (angle > M_HALF_PI) {
            angle = M_PI_F - angle;
        } else if (angle < -M_HALF_PI) {
            angle = -M_PI_F - angle;
        }
    }
    if (enemy->move.bounds_max.x - 96.0f < enemy->move.pos.x) {
        if (angle < M_HALF_PI && angle >= 0.0f) {
            // The original mirrors the angle the enemy moves at, not the
            // one drawn.
            angle = M_PI_F - enemy->move.angle;
        } else if (angle > -M_HALF_PI && angle <= 0.0f) {
            angle = -M_PI_F - angle;
        }
    }
    if (enemy->move.bounds_min.y + 48.0f > enemy->move.pos.y && angle < 0.0f) {
        angle = -angle;
    }
    if (enemy->move.bounds_max.y - 48.0f < enemy->move.pos.y && angle > 0.0f) {
        angle = -angle;
    }
    return angle;
}

static void M_Fire(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    if (enemy->life <= 0) {
        return;
    }
    BULLET_SHOOTER *const shooter = &enemy->shots.shooter;
    shooter->type = (int16_t)M_IntOp(enemy, instr, M_ArgShort(instr, 0), 1);
    shooter->aim = (BULLET_AIM)(instr->opcode - ECL_OP_FIRE_FAN_AT_PLAYER);
    shooter->columns = (int16_t)M_IntOp(enemy, instr, instr->args[1].i, 4);
    shooter->rows = (int16_t)M_IntOp(enemy, instr, instr->args[2].i, 8);
    shooter->pos = Vec3_Add(enemy->move.pos, enemy->shots.offset);
    shooter->angle1 = M_FloatOp(enemy, instr, instr->args[5].f, 0x40);
    shooter->speed1 = M_FloatOp(enemy, instr, instr->args[3].f, 0x10);
    shooter->angle2 = M_FloatOp(enemy, instr, instr->args[6].f, 0x80);
    shooter->speed2 = M_FloatOp(enemy, instr, instr->args[4].f, 0x20);
    if (g_Enemies.spell_card.state == ENEMY_SPELL_NONE) {
        shooter->columns = (int16_t)(shooter->columns
                                     + M_RankLerp(
                                         enemy->shots.rank_columns_low,
                                         enemy->shots.rank_columns_high));
        if (shooter->columns <= 0) {
            shooter->columns = 1;
        }
        shooter->rows = (int16_t)(shooter->rows
                                  + M_RankLerp(
                                      enemy->shots.rank_rows_low,
                                      enemy->shots.rank_rows_high));
        if (shooter->rows <= 0) {
            shooter->rows = 1;
        }
        if (shooter->speed1 != 0.0f) {
            shooter->speed1 += M_RankLerpFloat(
                enemy->shots.rank_speed_low, enemy->shots.rank_speed_high);
            if (shooter->speed1 < 0.3f) {
                shooter->speed1 = 0.3f;
            }
        }
        shooter->speed2 +=
            M_RankLerpFloat(
                enemy->shots.rank_speed_low, enemy->shots.rank_speed_high)
            / 2.0f;
        if (shooter->speed2 < 0.3f) {
            shooter->speed2 = 0.3f;
        }
    }
    shooter->flags = instr->args[7].i;
    shooter->color = (int16_t)M_IntOp(enemy, instr, M_ArgShort(instr, 1), 2);
    if (!enemy->flags.hold_fire) {
        Bullet_FirePattern(shooter);
    }
}

static void M_SpawnLaser(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    BULLET_SHOOTER *const shooter = &enemy->shots.laser_shooter;
    shooter->pos = Vec3_Add(enemy->move.pos, enemy->shots.offset);
    shooter->type = M_ArgShort(instr, 0);
    shooter->color = (int16_t)M_IntOp(enemy, instr, M_ArgShort(instr, 1), 2);
    shooter->angle1 = M_FloatOp(enemy, instr, instr->args[1].f, 4);
    shooter->speed1 = M_FloatOp(enemy, instr, instr->args[2].f, 8);
    shooter->laser.start_offset =
        M_FloatOp(enemy, instr, instr->args[3].f, 0x10);
    shooter->laser.end_offset = M_FloatOp(enemy, instr, instr->args[4].f, 0x20);
    shooter->laser.max_length = M_FloatOp(enemy, instr, instr->args[5].f, 0x40);
    shooter->laser.width = instr->args[6].f;
    shooter->laser.start_frames = instr->args[7].i;
    shooter->laser.firing_frames = instr->args[8].i;
    shooter->laser.end_frames = instr->args[9].i;
    shooter->laser.hit_start_frame = instr->args[10].i;
    shooter->laser.end_hit_frames = instr->args[11].i;
    shooter->flags = instr->args[12].i;
    shooter->aim = instr->opcode == ECL_OP_LASER_AT_PLAYER
        ? BULLET_AIM_FAN_AT_PLAYER
        : BULLET_AIM_FAN;
    *M_Laser(enemy, enemy->shots.laser_index) = Bullet_SpawnLaser(shooter);
}

static void M_SetInterp(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    for (int32_t i = 0; i < ECL_INTERP_COUNT; i++) {
        ECL_INTERP *const interp = &enemy->script.state.interps[i];
        if (interp->active && interp->var != instr->args[0].f) {
            continue;
        }
        Timer_Set(&interp->timer, 0);
        interp->var = instr->args[0].f;
        interp->duration = M_IntArg(enemy, instr, 1);
        interp->kind = M_IntArg(enemy, instr, 2);
        interp->ease = M_IntArg(enemy, instr, 3);
        ASSERT(interp->kind >= 0 && interp->kind <= M_INTERP_HERMITE);
        interp->active = true;
        interp->from = M_FloatArg(enemy, instr, 4);
        interp->to = M_FloatArg(enemy, instr, 5);
        interp->tangent_from = M_FloatArg(enemy, instr, 6);
        interp->tangent_to = M_FloatArg(enemy, instr, 7);
        return;
    }
}

// Moves the variable of interp to t along its curve. The original
// resolves the ends and the tangents as operands again each frame.
static void M_RunInterp(
    ENEMY *const enemy, ECL_INTERP *const interp, const float t)
{
    const float start = M_GetFloat(enemy, interp->from);
    const float end = M_GetFloat(enemy, interp->to);
    float value;
    if (interp->kind == M_INTERP_HERMITE) {
        const float slope_in = M_GetFloat(enemy, interp->tangent_from);
        const float slope_out = M_GetFloat(enemy, interp->tangent_to);
        const float h_start = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        const float h_end = t * t * (3.0f - 2.0f * t);
        const float w_in = (1.0f - t) * (1.0f - t) * t;
        const float w_out = (t - 1.0f) * t * t;
        value =
            h_start * start + h_end * end + w_in * slope_in + w_out * slope_out;
    } else {
        value = (end - start) * t + start;
    }
    *M_ResolveFloat(enemy, &interp->var, 0, -1) = value;
}

static void M_RunInterps(ENEMY *const enemy)
{
    bool moved = false;
    const VEC3 saved = enemy->move.pos;
    for (int32_t i = 0; i < ECL_INTERP_COUNT; i++) {
        ECL_INTERP *const interp = &enemy->script.state.interps[i];
        if (!interp->active) {
            continue;
        }
        Timer_TickAt(&interp->timer, g_GameSpeed);
        if (interp->timer.current >= interp->duration) {
            Timer_Set(&interp->timer, interp->duration);
        }
        const float t = M_Ease(
            interp->ease,
            Timer_AsFloat(&interp->timer) / (float)interp->duration);
        M_RunInterp(enemy, interp, t);
        if (interp->timer.current >= interp->duration) {
            interp->active = false;
        }
        if (interp->var == 10018.0f || interp->var == 10019.0f
            || interp->var == 10020.0f) {
            moved = true;
        }
    }
    if (moved) {
        enemy->move.velocity.x = enemy->move.pos.x - saved.x;
        enemy->move.velocity.y = enemy->move.pos.y - saved.y;
        enemy->move.angle =
            X87_Atan2(enemy->move.velocity.y, enemy->move.velocity.x);
        enemy->move.pos = saved;
    }
}

static void M_StartSpellCard(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    // The name is translated only where it is drawn. The record of the
    // card keeps the ECL's name, because the score file checks its bytes.
    Gui_ShowSpellCardName(
        M_ArgShort(instr, 0),
        Translation_GetSpellName(M_ArgShort(instr, 1), instr->text));
    Bullet_ClearAllToItems();
    g_Stage.spell_card.state = 1;
    g_Stage.spell_card.frames = 0;
    for (int32_t i = 0; i < g_Stage.spell_background.count; i++) {
        Anm_StartScript(
            &g_Stage.spell_background.sprites[i],
            i + g_Stage.spell_background.offset + M_SPELL_BACKGROUND_SCRIPT);
    }
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.index = (uint16_t)M_ArgShort(instr, 1);
    g_Enemies.spell_card.bonus = m_SpellCardBonuses[g_Enemies.spell_card.index];
    g_Enemies.spell_card.graze_bonus = 0;
    const int32_t seconds = enemy->callbacks.time / 60 + 10;
    ASSERT(seconds != 0);
    g_Enemies.spell_card.bonus_step = g_Enemies.spell_card.bonus / seconds;
    Timer_Set(&g_Enemies.spell_card.bonus_timer, 0);
    enemy->shots.rank_speed_low = -0.5f;
    enemy->shots.rank_speed_high = 0.5f;
    enemy->shots.rank_columns_low = 0;
    enemy->shots.rank_columns_high = 0;
    enemy->shots.rank_rows_low = 0;
    enemy->shots.rank_rows_high = 0;
    EFFECT *const circle = Effect_SpawnReserved(
        EFFECT_SPELL_CIRCLE, enemy->move.pos, 1, COLOR_WHITE);
    enemy->orbits.effects[ENEMY_SPELL_CIRCLE] = circle;
    ANM_VM *const vm = &circle->vm;
    Timer_Set(&vm->interp_time[ANM_INTERP_SCALE], 0);
    vm->interp_length[ANM_INTERP_SCALE] = enemy->callbacks.time;
    vm->interp_ease[ANM_INTERP_SCALE] = ANM_EASE_LINEAR;
    vm->scale_from = (VEC2) { vm->scale_x, vm->scale_y };
    vm->scale_to = (VEC2) { M_SPELL_CIRCLE_SCALE, M_SPELL_CIRCLE_SCALE };
    circle->pos = enemy->move.pos;
    enemy->flags.spell_circle_detached = false;
    if (!g_GameManager.mode.in_replay) {
        ResultScreen_AddSpellCardAttempt(
            g_Enemies.spell_card.index, instr->text);
    }
}

static void M_EndSpellCard(void)
{
    if (g_Enemies.spell_card.state != ENEMY_SPELL_NONE) {
        Gui_EndSpellCardName();
        if (g_Enemies.spell_card.state == ENEMY_SPELL_RUNNING) {
            int32_t bonus = Bullet_ClearAllWithBonus(M_CLEAR_BONUS, true);
            bonus = Enemy_ClearForBonus(M_CLEAR_BONUS, bonus);
            if (bonus != 0) {
                GameManager_AddScore(bonus);
                Gui_ShowBonusPopup(bonus);
            }
            if (g_Enemies.spell_card.capturable) {
                bonus = g_Enemies.spell_card.bonus
                    + g_Enemies.spell_card.graze_bonus;
                Gui_ShowSpellCardBonus(bonus);
                GameManager_AddScore(bonus);
                if (!g_GameManager.mode.in_replay) {
                    ResultScreen_AddSpellCardCapture(
                        g_Enemies.spell_card.index, bonus);
                }
                g_GameManager.stats.spell_cards_captured++;
            }
        }
        g_Enemies.spell_card.state = ENEMY_SPELL_NONE;
        for (int32_t i = 0; i < ENEMY_BOSS_COUNT; i++) {
            ENEMY *const boss = g_Enemies.bosses[i];
            if (boss != nullptr
                && boss->orbits.effects[ENEMY_SPELL_CIRCLE] != nullptr) {
                boss->orbits.effects[ENEMY_SPELL_CIRCLE]->active = false;
                boss->orbits.effects[ENEMY_SPELL_CIRCLE] = nullptr;
            }
        }
        Sound_Queue(M_SOUND_SPELL_END);
    }
    g_Stage.spell_card.state = 0;
}

static void M_SetBoss(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    if (M_IntArg(enemy, instr, 0) >= 0) {
        const int32_t index = M_IntArg(enemy, instr, 0);
        ASSERT(index < ENEMY_BOSS_COUNT);
        g_Enemies.bosses[index] = enemy;
        g_Gui.boss.present = true;
        g_Gui.boss.health = 1.0f;
        enemy->flags.boss = true;
        enemy->boss_index = (uint8_t)M_IntArg(enemy, instr, 0);
        M_SetBossMarker(enemy->boss_index, 1);
    } else {
        if (enemy->boss_index < M_BAR_BOSSES) {
            g_Gui.boss.present = false;
        }
        ASSERT(enemy->boss_index < ENEMY_BOSS_COUNT);
        g_Enemies.bosses[enemy->boss_index] = nullptr;
        enemy->flags.boss = false;
        M_SetBossMarker(enemy->boss_index, 2);
        Enemy_ReleaseOrbits(enemy);
    }
}

static void M_AddOrbit(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    const int32_t color = instr->args[0].i;
    ASSERT(color >= 0 && color < (int32_t)ARRAY_SIZE(m_OrbitColors));
    ASSERT(enemy->orbits.count >= 0 && enemy->orbits.count < ENEMY_ORBIT_COUNT);
    EFFECT *const effect = Effect_Spawn(
        (EFFECT_TYPE)M_ORBIT_EFFECT, enemy->move.pos, 1, m_OrbitColors[color]);
    enemy->orbits.effects[enemy->orbits.count] = effect;
    effect->axis =
        (VEC3) { instr->args[1].f, instr->args[2].f, instr->args[3].f };
    enemy->orbits.radius = instr->args[4].f;
    enemy->orbits.count++;
}

static void M_DropItems(
    ENEMY *const enemy, const ECL_INSTR *const instr, const bool power)
{
    const int32_t count = M_IntArg(enemy, instr, 0);
    for (int32_t i = 0; i < count; i++) {
        VEC3 pos = enemy->move.pos;
        pos.x += Rng_F32InRange(&g_Rng, M_DROP_SPREAD) - 64.0f;
        pos.y += Rng_F32InRange(&g_Rng, M_DROP_SPREAD) - 64.0f;
        if (power && X87_Ftol(g_GameManager.player.power) < M_POWER_FULL) {
            Item_Spawn(pos, i == 0 ? ITEM_BIG_POWER : ITEM_POWER, ITEM_FALLING);
        } else {
            Item_Spawn(pos, ITEM_POINT, ITEM_FALLING);
        }
    }
}

static void M_SpawnChild(
    ENEMY *const enemy, const ECL_INSTR *const instr, const bool relative)
{
    if (enemy->life <= 0) {
        return;
    }
    VEC3 pos = {
        M_FloatOp(enemy, instr, instr->args[1].f, 2),
        M_FloatOp(enemy, instr, instr->args[2].f, 4),
        M_FloatOp(enemy, instr, instr->args[3].f, 8),
    };
    if (relative) {
        pos = Vec3_Add(pos, enemy->move.pos);
    }
    // The arguments evaluate in the order of the decompilation.
    const int32_t life = M_IntArg(enemy, instr, 4);
    const int32_t item = M_IntArg(enemy, instr, 5);
    const int32_t score = M_IntArg(enemy, instr, 6);
    Enemy_SpawnWithVars(
        instr->args[0].i, pos, life, item, score, &enemy->script.state.vars);
}

// Saves the state to resume at next, when the enemy keeps a call stack,
// and starts the pending interrupt. Unlike a call, an interrupt counts its
// depth also without a call stack.
static void M_Interrupt(ENEMY *const enemy, const int32_t next)
{
    ECL_STATE *const state = &enemy->script.state;
    state->pc = next;
    if (!enemy->flags.no_call_stack) {
        *M_StackSlot(enemy, enemy->script.depth) = *state;
    }
    const int32_t index = enemy->script.pending_interrupt;
    ASSERT(index >= 0 && index < ENEMY_INTERRUPT_COUNT);
    Ecl_StartSub(state, (int16_t)enemy->script.interrupts[index]);
    if (enemy->script.depth < M_MAX_DEPTH) {
        enemy->script.depth++;
    }
    enemy->script.pending_interrupt = -1;
}

static void M_Call(ENEMY *const enemy, const int32_t next, const int16_t sub)
{
    ECL_STATE *const state = &enemy->script.state;
    state->pc = next;
    if (!enemy->flags.no_call_stack) {
        *M_StackSlot(enemy, enemy->script.depth) = *state;
    }
    Ecl_StartSub(state, sub);
    memcpy(&state->vars.ints[8], m_SharedInts, sizeof(m_SharedInts));
    memcpy(&state->vars.floats[10], m_SharedFloats, sizeof(m_SharedFloats));
    if (!enemy->flags.no_call_stack && enemy->script.depth < M_MAX_DEPTH) {
        enemy->script.depth++;
    }
}

// Returns from a call or an interrupt. The depth goes down also without a
// call stack.
static void M_Return(ENEMY *const enemy)
{
    ECL_STATE *const state = &enemy->script.state;
    enemy->script.depth--;
    if (state->in_timed_call) {
        enemy->script.periodic.vars = state->vars;
        state->in_timed_call = false;
    }
    *state = *M_StackSlot(enemy, enemy->script.depth);
}

// Starts the periodic call when it is due. Returns whether it started.
static bool M_StartPeriodic(ENEMY *const enemy)
{
    if (enemy->script.periodic.sub < 0) {
        return false;
    }
    Timer_TickAt(&enemy->script.periodic.timer, g_GameSpeed);
    if (enemy->script.periodic.timer.current
        < enemy->script.periodic.interval.current) {
        return false;
    }
    ECL_STATE *const state = &enemy->script.state;
    Timer_Set(&enemy->script.periodic.timer, 0);
    *M_StackSlot(enemy, enemy->script.depth) = *state;
    state->vars = enemy->script.periodic.vars;
    Ecl_StartSub(state, (int16_t)enemy->script.periodic.sub);
    if (enemy->script.depth < M_MAX_DEPTH) {
        enemy->script.depth++;
    }
    state->in_timed_call = true;
    return true;
}

static bool M_CompareInts(
    const int16_t opcode, const int32_t a, const int32_t b)
{
    switch (opcode) {
    case ECL_OP_JUMP_EQUAL_INT:
        return a == b;
    case ECL_OP_JUMP_NOT_EQUAL_INT:
        return a != b;
    case ECL_OP_JUMP_LESS_INT:
        return a < b;
    case ECL_OP_JUMP_LESS_EQUAL_INT:
        return a <= b;
    case ECL_OP_JUMP_GREATER_INT:
        return a > b;
    default:
        return a >= b;
    }
}

static bool M_CompareFloats(const int16_t opcode, const float a, const float b)
{
    switch (opcode) {
    case ECL_OP_JUMP_EQUAL_FLOAT:
        return a == b;
    case ECL_OP_JUMP_NOT_EQUAL_FLOAT:
        return a != b;
    case ECL_OP_JUMP_LESS_FLOAT:
        return a < b;
    case ECL_OP_JUMP_LESS_EQUAL_FLOAT:
        return a <= b;
    case ECL_OP_JUMP_GREATER_FLOAT:
        return a > b;
    default:
        return a >= b;
    }
}

static int32_t M_Divide(const int32_t a, const int32_t b, const bool modulo)
{
    // The original faults on these.
    ASSERT(b != 0 && !(a == INT32_MIN && b == -1));
    return modulo ? a % b : a / b;
}

// Runs an opcode that sets a variable.
static void M_StepVars(ENEMY *const enemy, ECL_INSTR *const instr)
{
    // Where two operands can both draw from the RNG, they evaluate in the
    // order of the decompilation.
    switch (instr->opcode) {
    case ECL_OP_SET_INT:
        M_SetInt(enemy, instr, 0, M_IntArg(enemy, instr, 1));
        break;
    case ECL_OP_SET_FLOAT:
        M_SetFloat(enemy, instr, 0, M_FloatArg(enemy, instr, 1));
        break;
    case ECL_OP_NORMALIZE_ANGLE:
        M_SetFloat(
            enemy, instr, 0,
            Angle_AddNormalize(M_FloatArg(enemy, instr, 0), 0.0f));
        break;
    case ECL_OP_RANDOM_INT: {
        const uint32_t range = (uint32_t)M_IntArg(enemy, instr, 1);
        M_SetInt(enemy, instr, 0, (int32_t)Rng_U32InRange(&g_Rng, range));
        break;
    }
    case ECL_OP_RANDOM_INT_FROM: {
        const uint32_t range = (uint32_t)M_IntArg(enemy, instr, 1);
        const int32_t value = (int32_t)Rng_U32InRange(&g_Rng, range);
        M_SetInt(
            enemy, instr, 0,
            M_Wrap((int64_t)value + M_IntArg(enemy, instr, 2)));
        break;
    }
    case ECL_OP_RANDOM_FLOAT:
        M_SetFloat(
            enemy, instr, 0,
            Rng_F32InRange(&g_Rng, M_FloatArg(enemy, instr, 1)));
        break;
    case ECL_OP_RANDOM_FLOAT_FROM: {
        const float value = Rng_F32InRange(&g_Rng, M_FloatArg(enemy, instr, 1));
        M_SetFloat(enemy, instr, 0, value + M_FloatArg(enemy, instr, 2));
        break;
    }
    case ECL_OP_RANDOM_SIGN_INT: {
        const int32_t sign = (Rng_U16(&g_Rng) & 1) != 0 ? 1 : -1;
        M_SetInt(
            enemy, instr, 0, M_Wrap((int64_t)sign * M_IntArg(enemy, instr, 1)));
        break;
    }
    case ECL_OP_RANDOM_SIGN_FLOAT: {
        const float sign = (Rng_U16(&g_Rng) & 1) != 0 ? 1.0f : -1.0f;
        M_SetFloat(enemy, instr, 0, sign * M_FloatArg(enemy, instr, 1));
        break;
    }
    case ECL_OP_INCREMENT:
    case ECL_OP_DECREMENT: {
        const int32_t value = *M_IntVar(enemy, instr, 0);
        M_SetInt(
            enemy, instr, 0,
            M_Wrap(
                (int64_t)value + (instr->opcode == ECL_OP_INCREMENT ? 1 : -1)));
        break;
    }
    case ECL_OP_GET_BOSS_INT: {
        int32_t value = instr->args[1].i;
        if (M_IsVar(instr, 2)) {
            ENEMY *const boss = M_Boss(M_IntArg(enemy, instr, 2));
            ASSERT(boss != nullptr);
            value = M_GetInt(boss, instr->args[1].i);
        }
        M_SetInt(enemy, instr, 0, value);
        break;
    }
    case ECL_OP_GET_BOSS_FLOAT: {
        float value = instr->args[1].f;
        if (M_IsVar(instr, 2)) {
            ENEMY *const boss = M_Boss(M_IntArg(enemy, instr, 2));
            ASSERT(boss != nullptr);
            value = M_GetFloat(boss, instr->args[1].f);
        }
        M_SetFloat(enemy, instr, 0, value);
        break;
    }
    case ECL_OP_ADD_INT:
    case ECL_OP_SUB_INT:
    case ECL_OP_MUL_INT:
    case ECL_OP_DIV_INT:
    case ECL_OP_MOD_INT: {
        const int32_t a = M_IntArg(enemy, instr, 1);
        const int32_t b = M_IntArg(enemy, instr, 2);
        int32_t value;
        switch (instr->opcode) {
        case ECL_OP_ADD_INT:
            value = M_Wrap((int64_t)a + b);
            break;
        case ECL_OP_SUB_INT:
            value = M_Wrap((int64_t)a - b);
            break;
        case ECL_OP_MUL_INT:
            value = M_Wrap((int64_t)a * b);
            break;
        default:
            value = M_Divide(a, b, instr->opcode == ECL_OP_MOD_INT);
            break;
        }
        M_SetInt(enemy, instr, 0, value);
        break;
    }
    case ECL_OP_ADD_FLOAT:
    case ECL_OP_SUB_FLOAT:
    case ECL_OP_MUL_FLOAT:
    case ECL_OP_DIV_FLOAT:
    case ECL_OP_MOD_FLOAT: {
        const float a = M_FloatArg(enemy, instr, 1);
        const float b = M_FloatArg(enemy, instr, 2);
        float value;
        switch (instr->opcode) {
        case ECL_OP_ADD_FLOAT:
            value = a + b;
            break;
        case ECL_OP_SUB_FLOAT:
            value = a - b;
            break;
        case ECL_OP_MUL_FLOAT:
            value = a * b;
            break;
        case ECL_OP_DIV_FLOAT:
            value = a / b;
            break;
        default:
            value = Math_Fmod(a, b);
            break;
        }
        M_SetFloat(enemy, instr, 0, value);
        break;
    }
    case ECL_OP_SIN:
        M_SetFloat(enemy, instr, 0, X87_Sin(M_FloatArg(enemy, instr, 1)));
        break;
    case ECL_OP_COS:
        M_SetFloat(enemy, instr, 0, X87_Cos(M_FloatArg(enemy, instr, 1)));
        break;
    case ECL_OP_ANGLE: {
        const float y =
            M_FloatArg(enemy, instr, 4) - M_FloatArg(enemy, instr, 2);
        const float x =
            M_FloatArg(enemy, instr, 3) - M_FloatArg(enemy, instr, 1);
        M_SetFloat(enemy, instr, 0, X87_Atan2(y, x));
        break;
    }
    case ECL_OP_LERP: {
        const float scale =
            M_FloatArg(enemy, instr, 1) - M_FloatArg(enemy, instr, 2);
        const float value =
            scale * M_FloatArg(enemy, instr, 3) + M_FloatArg(enemy, instr, 2);
        M_SetFloat(enemy, instr, 0, value);
        break;
    }
    case ECL_OP_RANDOM_FLOAT_BETWEEN: {
        const float high = M_FloatArg(enemy, instr, 2);
        const float range = high - M_FloatArg(enemy, instr, 1);
        const float value =
            Rng_F32InRange(&g_Rng, range) + M_FloatArg(enemy, instr, 1);
        M_SetFloat(enemy, instr, 0, value);
        break;
    }
    case ECL_OP_RANDOM_ANGLE_IN_BOUNDS:
        M_SetFloat(enemy, instr, 0, M_RandomAngleInBounds(enemy));
        break;
    case ECL_OP_INTERPOLATE:
        M_SetInterp(enemy, instr);
        break;
    case ECL_OP_POLAR: {
        // The second pair of operands is read after the first store.
        float x;
        float y;
        float angle = M_FloatArg(enemy, instr, 2);
        X87_SinCosScale(angle, M_FloatArg(enemy, instr, 3), &x, &y);
        M_SetFloat(enemy, instr, 1, y);
        angle = M_FloatArg(enemy, instr, 2);
        X87_SinCosScale(angle, M_FloatArg(enemy, instr, 3), &x, &y);
        M_SetFloat(enemy, instr, 0, x);
        break;
    }
    case ECL_OP_RANDOM_SIDE_ANGLE: {
        float angle;
        if ((g_Player.pos.x < enemy->move.pos.x && enemy->move.pos.x > 96.0f)
            || enemy->move.pos.x > 288.0f) {
            angle = Angle_AddNormalize(
                Rng_F32InRange(&g_Rng, M_HALF_PI) + M_THREE_QUARTER_PI, 0.0f);
        } else {
            angle = Rng_F32InRange(&g_Rng, M_HALF_PI) - M_QUARTER_PI;
        }
        M_SetFloat(enemy, instr, 0, angle);
        break;
    }
    default:
        break;
    }
}

// Runs an opcode that moves the enemy.
static void M_StepMove(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    switch (instr->opcode) {
    case ECL_OP_SET_POS:
        enemy->move.pos.x = M_FloatArg(enemy, instr, 0);
        enemy->move.pos.y = M_FloatArg(enemy, instr, 1);
        enemy->move.pos.z = M_FloatArg(enemy, instr, 2);
        Enemy_ClampPos(enemy);
        break;
    case ECL_OP_SET_VELOCITY:
        enemy->move.velocity.x = M_FloatArg(enemy, instr, 0);
        enemy->move.velocity.y = M_FloatArg(enemy, instr, 1);
        enemy->move.velocity.z = M_FloatArg(enemy, instr, 2);
        enemy->move.angle =
            X87_Atan2(enemy->move.velocity.y, enemy->move.velocity.x);
        enemy->flags.move = ENEMY_MOVE_VELOCITY;
        break;
    case ECL_OP_SET_ANGULAR_VELOCITY:
        enemy->move.angular_velocity = M_FloatArg(enemy, instr, 0);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case ECL_OP_AIM_AT_PLAYER:
        enemy->move.angle =
            Player_AngleFrom(enemy->move.pos) + M_FloatArg(enemy, instr, 0);
        enemy->move.speed = M_FloatArg(enemy, instr, 1);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case ECL_OP_SET_SPEED:
        enemy->move.speed = M_FloatArg(enemy, instr, 0);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case ECL_OP_SET_ACCELERATION:
        enemy->move.acceleration = M_FloatArg(enemy, instr, 0);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case ECL_OP_TIME_ANGLE_MOVE:
        M_SetMoveTimer(enemy, instr, ENEMY_MOVE_ANGLE);
        break;
    case ECL_OP_TIME_CIRCLE_MOVE:
        M_SetMoveTimer(enemy, instr, ENEMY_MOVE_CIRCLE);
        break;
    case ECL_OP_TIME_MOVE_TO:
        M_SetMoveTimer(enemy, instr, ENEMY_MOVE_TO);
        break;
    case ECL_OP_MOVE_ANGLE:
        if (M_IntArg(enemy, instr, 0) <= 0) {
            enemy->move.angle =
                Angle_AddNormalize(M_FloatArg(enemy, instr, 2), 0.0f);
            enemy->move.speed = M_FloatArg(enemy, instr, 3);
            M_SetMoveTimer(enemy, instr, ENEMY_MOVE_ANGLE);
        } else {
            M_MoveAngle(enemy, instr);
        }
        break;
    case ECL_OP_MOVE_TO:
        M_MoveTo(enemy, instr);
        break;
    case ECL_OP_MOVE_CIRCLE:
        enemy->move.duration = M_IntArg(enemy, instr, 0);
        Timer_Set(&enemy->move.timer, enemy->move.duration);
        enemy->move.start.x = M_FloatArg(enemy, instr, 1);
        enemy->move.start.y = M_FloatArg(enemy, instr, 2);
        enemy->move.start.z = M_FloatArg(enemy, instr, 3);
        enemy->move.circle.angle = M_FloatArg(enemy, instr, 4);
        enemy->move.circle.angular_velocity = M_FloatArg(enemy, instr, 5);
        enemy->move.circle.radius = M_FloatArg(enemy, instr, 6);
        enemy->move.circle.radius_velocity = M_FloatArg(enemy, instr, 7);
        enemy->flags.move = ENEMY_MOVE_CIRCLE;
        break;
    case ECL_OP_SET_CIRCLE_RADIUS:
        enemy->move.circle.radius = M_FloatArg(enemy, instr, 0);
        enemy->move.circle.radius_velocity = M_FloatArg(enemy, instr, 1);
        break;
    case ECL_OP_SET_CIRCLE_ANGLE:
        enemy->move.circle.angle = M_FloatArg(enemy, instr, 0);
        enemy->move.circle.angular_velocity = M_FloatArg(enemy, instr, 1);
        break;
    case ECL_OP_SET_BOUNDS:
        enemy->move.bounds_min.x = M_FloatArg(enemy, instr, 0);
        enemy->move.bounds_min.y = M_FloatArg(enemy, instr, 1);
        enemy->move.bounds_max.x = M_FloatArg(enemy, instr, 2);
        enemy->move.bounds_max.y = M_FloatArg(enemy, instr, 3);
        enemy->flags.clamp_pos = true;
        break;
    case ECL_OP_CLEAR_BOUNDS:
        enemy->flags.clamp_pos = false;
        break;
    default:
        break;
    }
}

// Runs an opcode of the shots and the lasers.
static void M_StepShots(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    LASER *laser;
    switch (instr->opcode) {
    case ECL_OP_FIRE_FAN_AT_PLAYER:
    case ECL_OP_FIRE_FAN:
    case ECL_OP_FIRE_RING_AT_PLAYER:
    case ECL_OP_FIRE_RING:
    case ECL_OP_FIRE_RING_OFFSET_AT_PLAYER:
    case ECL_OP_FIRE_RING_OFFSET:
    case ECL_OP_FIRE_RANDOM_ANGLE:
    case ECL_OP_FIRE_RING_RANDOM_SPEED:
    case ECL_OP_FIRE_RANDOM:
        M_Fire(enemy, instr);
        break;
    case ECL_OP_SET_SHOT_COMMAND: {
        const int32_t index = M_IntArg(enemy, instr, 0);
        ASSERT(index >= 0 && index < BULLET_COMMAND_COUNT);
        BULLET_COMMAND *const command = &enemy->shots.shooter.commands[index];
        command->flag = (uint32_t)M_IntArg(enemy, instr, 1);
        command->concurrent = M_IntArg(enemy, instr, 2) != 0;
        command->frames = M_IntArg(enemy, instr, 3);
        command->count = M_IntArg(enemy, instr, 4);
        command->args[0] = M_FloatArg(enemy, instr, 5);
        command->args[1] = M_FloatArg(enemy, instr, 6);
        break;
    }
    case ECL_OP_SET_INTERVAL:
    case ECL_OP_SET_INTERVAL_RANDOM: {
        const int32_t interval = M_IntArg(enemy, instr, 0);
        enemy->shots.interval = interval;
        if (interval == 0) {
            break;
        }
        enemy->shots.interval += M_RankLerp(interval / 5, -interval / 5);
        Timer_Set(
            &enemy->shots.timer,
            instr->opcode == ECL_OP_SET_INTERVAL
                ? 0
                : (int32_t)Rng_U32InRange(
                      &g_Rng, (uint32_t)enemy->shots.interval));
        break;
    }
    case ECL_OP_HOLD_FIRE:
        enemy->flags.hold_fire = true;
        break;
    case ECL_OP_RELEASE_FIRE:
        enemy->flags.hold_fire = false;
        break;
    case ECL_OP_FIRE_AGAIN:
        enemy->shots.shooter.pos =
            Vec3_Add(enemy->move.pos, enemy->shots.offset);
        Bullet_FirePattern(&enemy->shots.shooter);
        break;
    case ECL_OP_SET_SHOT_OFFSET:
        enemy->shots.offset.x = M_FloatArg(enemy, instr, 0);
        enemy->shots.offset.y = M_FloatArg(enemy, instr, 1);
        enemy->shots.offset.z = M_FloatArg(enemy, instr, 2);
        break;
    case ECL_OP_CLEAR_BULLETS:
        Bullet_ClearAllToItems();
        break;
    case ECL_OP_DELETE_BULLETS:
        Bullet_ClearAll();
        break;
    case ECL_OP_CLEAR_BULLETS_IN_CIRCLE:
        Bullet_ClearCircleToItems(enemy->move.pos, M_FloatArg(enemy, instr, 0));
        break;
    case ECL_OP_SET_SHOT_SOUND:
        if (M_IntArg(enemy, instr, 0) >= 0) {
            enemy->shots.shooter.sound = M_IntArg(enemy, instr, 0);
            enemy->shots.shooter.flags |= BULLET_FLAG_SOUND;
        } else {
            enemy->shots.shooter.flags &= ~BULLET_FLAG_SOUND;
        }
        enemy->shots.shooter.bullet_sound = M_IntArg(enemy, instr, 1);
        break;
    case ECL_OP_SET_RANK_GROWTH:
        enemy->shots.rank_speed_low = M_FloatArg(enemy, instr, 0);
        enemy->shots.rank_speed_high = M_FloatArg(enemy, instr, 1);
        enemy->shots.rank_columns_low = (int16_t)M_IntArg(enemy, instr, 2);
        enemy->shots.rank_columns_high = (int16_t)M_IntArg(enemy, instr, 3);
        enemy->shots.rank_rows_low = (int16_t)M_IntArg(enemy, instr, 4);
        enemy->shots.rank_rows_high = (int16_t)M_IntArg(enemy, instr, 5);
        break;
    case ECL_OP_LASER:
    case ECL_OP_LASER_AT_PLAYER:
        M_SpawnLaser(enemy, instr);
        break;
    case ECL_OP_SET_LASER_INDEX:
        enemy->shots.laser_index = M_IntArg(enemy, instr, 0);
        break;
    case ECL_OP_FORGET_LASERS:
        memset(enemy->shots.lasers, 0, sizeof(enemy->shots.lasers));
        break;
    case ECL_OP_TURN_LASER:
        laser = *M_Laser(enemy, M_IntArg(enemy, instr, 0));
        if (laser != nullptr) {
            laser->angle =
                Angle_AddNormalize(laser->angle, M_FloatArg(enemy, instr, 1));
        }
        break;
    case ECL_OP_SET_LASER_ANGLE:
        laser = *M_Laser(enemy, M_IntArg(enemy, instr, 0));
        if (laser != nullptr) {
            laser->angle = M_FloatArg(enemy, instr, 1);
        }
        break;
    case ECL_OP_AIM_LASER:
        laser = *M_Laser(enemy, M_IntArg(enemy, instr, 0));
        if (laser != nullptr) {
            laser->angle =
                Player_AngleFrom(laser->pos) + M_FloatArg(enemy, instr, 1);
        }
        break;
    case ECL_OP_MOVE_LASER:
        laser = *M_Laser(enemy, M_IntArg(enemy, instr, 0));
        if (laser != nullptr) {
            laser->pos.x = M_FloatArg(enemy, instr, 1) + enemy->move.pos.x;
            laser->pos.y = M_FloatArg(enemy, instr, 2) + enemy->move.pos.y;
            laser->pos.z = M_FloatArg(enemy, instr, 3) + enemy->move.pos.z;
        }
        break;
    case ECL_OP_HIDE_LASER_ORIGIN:
        laser = *M_Laser(enemy, M_IntArg(enemy, instr, 0));
        if (laser != nullptr) {
            laser->hide_origin_while_starting = M_IntArg(enemy, instr, 1) != 0;
        }
        break;
    case ECL_OP_TEST_LASER:
        // Nothing reads the result of the test.
        M_Laser(enemy, M_IntArg(enemy, instr, 0));
        break;
    case ECL_OP_END_LASER:
        laser = *M_Laser(enemy, M_IntArg(enemy, instr, 0));
        if (laser != nullptr && laser->active && laser->state < LASER_ENDING) {
            laser->state = LASER_ENDING;
            Timer_Set(&laser->timer, 0);
            laser->width = laser->shown_width;
        }
        break;
    case ECL_OP_SET_LASER_LENGTH:
        laser = *M_Laser(enemy, M_IntArg(enemy, instr, 0));
        if (laser != nullptr) {
            laser->max_length = M_FloatArg(enemy, instr, 1);
        }
        break;
    case ECL_OP_SET_LASER_OFFSETS:
        laser = *M_Laser(enemy, M_IntArg(enemy, instr, 0));
        if (laser != nullptr) {
            laser->start_offset = M_FloatArg(enemy, instr, 1);
            laser->end_offset = M_FloatArg(enemy, instr, 2);
        }
        break;
    default:
        break;
    }
}

// Runs an opcode of bosses, spell cards and enemy attributes.
static void M_StepEnemy(ENEMY *const enemy, const ECL_INSTR *const instr)
{
    switch (instr->opcode) {
    case ECL_OP_START_SPELL_CARD:
        M_StartSpellCard(enemy, instr);
        break;
    case ECL_OP_END_SPELL_CARD:
        M_EndSpellCard();
        break;
    case ECL_OP_SET_BOSS:
        M_SetBoss(enemy, instr);
        break;
    case ECL_OP_SET_LIFE:
        enemy->max_life = M_IntArg(enemy, instr, 0);
        enemy->life = enemy->max_life;
        if (enemy->boss_index == 0 && enemy->flags.boss) {
            for (int32_t i = 0; i < GUI_BOSS_SECTION_COUNT; i++) {
                g_Gui.boss.sections[i].end = 0.0f;
                g_Gui.boss.sections[i].start = 0.0f;
            }
        }
        break;
    case ECL_OP_SET_BOSS_SECTION: {
        const int32_t index = M_IntArg(enemy, instr, 0);
        ASSERT(index >= 0 && index < GUI_BOSS_SECTION_COUNT);
        // The two fractions evaluate in the order of the decompilation.
        const float end =
            (float)M_IntArg(enemy, instr, 1) / (float)enemy->max_life;
        const float start =
            (float)M_IntArg(enemy, instr, 2) / (float)enemy->max_life;
        g_Gui.boss.sections[index].end = end;
        g_Gui.boss.sections[index].start = start;
        g_Gui.boss.sections[index].color = M_IntArg(enemy, instr, 3);
        break;
    }
    case ECL_OP_SET_BOSS_LIVES:
        g_Gui.boss.lives = M_IntArg(enemy, instr, 0);
        g_GameManager.run.progress_frames += 1800;
        break;
    case ECL_OP_SET_KEEP_SPELL_CARD:
        enemy->flags.keep_spell_card = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_READ_UNUSED:
        // The original keeps it where nothing reads it.
        M_IntArg(enemy, instr, 0);
        break;
    case ECL_OP_SET_SPELL_CIRCLE:
        enemy->flags.spell_circle_detached =
            (M_IntArg(enemy, instr, 0) & 1) != 0;
        if (!enemy->flags.spell_circle_detached) {
            EFFECT *const circle = enemy->orbits.effects[ENEMY_SPELL_CIRCLE];
            ASSERT(circle != nullptr);
            circle->pos.x = M_FloatArg(enemy, instr, 1);
            circle->pos.y = M_FloatArg(enemy, instr, 2);
            circle->pos.z = M_FloatArg(enemy, instr, 3);
        }
        break;
    case ECL_OP_SET_DEATH_EFFECTS:
        enemy->death.effect = (int8_t)M_ArgByte(instr, 0);
        enemy->death.particle_color = M_ArgByte(instr, 1);
        break;
    case ECL_OP_SET_HITBOX:
        enemy->hitbox_size.x = M_FloatArg(enemy, instr, 0);
        enemy->hitbox_size.y = M_FloatArg(enemy, instr, 1);
        enemy->hitbox_size.z = M_FloatArg(enemy, instr, 2);
        break;
    case ECL_OP_SET_EXTRA_HITBOX:
        enemy->extra_hitbox_size.x = M_FloatArg(enemy, instr, 0);
        enemy->extra_hitbox_size.y = M_FloatArg(enemy, instr, 1);
        enemy->extra_hitbox_size.z = M_FloatArg(enemy, instr, 2);
        break;
    case ECL_OP_SET_CONTACT_DAMAGE:
        enemy->flags.contact_damage = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_SET_TAKES_DAMAGE:
        enemy->flags.takes_damage = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_SET_HIT_BY_SHOTS:
        enemy->flags.hit_by_shots = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_SET_DEATH:
        enemy->flags.death = (ENEMY_DEATH)(instr->args[0].i & 7);
        break;
    case ECL_OP_SET_SHOOTABLE:
        enemy->flags.shootable = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_SET_ROTATE_SPRITE:
        enemy->flags.rotate_sprite = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_SET_NO_CALL_STACK:
        enemy->flags.no_call_stack = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_SET_HIDDEN:
        enemy->flags.hidden = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_SET_GRAZEABLE:
        enemy->flags.grazeable = (instr->args[0].i & 1) != 0;
        enemy->draw_list = ENEMY_DRAW_HIGH;
        break;
    case ECL_OP_SET_STAYS_OFF_SCREEN:
        enemy->flags.stays_off_screen = (instr->args[0].i & 1) != 0;
        break;
    case ECL_OP_SET_HALTED_BY_BOMB:
        enemy->flags.halted_by_bomb = (M_IntArg(enemy, instr, 0) & 1) != 0;
        break;
    case ECL_OP_SET_DEATH_SUB:
        enemy->script.death_sub = M_ArgByte(instr, 0);
        ASSERT(Ecl_IsSub(enemy->script.death_sub));
        break;
    case ECL_OP_SET_INTERRUPT: {
        const int32_t index = M_IntArg(enemy, instr, 1);
        ASSERT(index >= 0 && index < ENEMY_INTERRUPT_COUNT);
        enemy->script.interrupts[index] = M_IntArg(enemy, instr, 0);
        break;
    }
    case ECL_OP_INTERRUPT_BOSS: {
        ENEMY *const boss = M_Boss(M_IntArg(enemy, instr, 0));
        if (boss != nullptr) {
            M_Boss(M_IntArg(enemy, instr, 0))->script.pending_interrupt =
                M_IntArg(enemy, instr, 1);
        }
        break;
    }
    case ECL_OP_SET_CALLBACK_TIMER:
        Timer_Set(&enemy->callbacks.timer, M_IntArg(enemy, instr, 0));
        break;
    case ECL_OP_SET_LIFE_CALLBACK:
        enemy->callbacks.life[0] = M_IntArg(enemy, instr, 0);
        break;
    case ECL_OP_SET_LIFE_CALLBACK_SUB:
        enemy->callbacks.life_sub[0] = M_IntArg(enemy, instr, 0);
        break;
    case ECL_OP_SET_LIFE_CALLBACK_AT: {
        const int32_t index = M_IntArg(enemy, instr, 0);
        ASSERT(index >= 0 && index < ENEMY_LIFE_CALLBACK_COUNT);
        enemy->callbacks.life[index] = M_IntArg(enemy, instr, 1);
        enemy->callbacks.life_sub[M_IntArg(enemy, instr, 0)] =
            M_IntArg(enemy, instr, 2);
        break;
    }
    case ECL_OP_SET_TIME_CALLBACK:
        enemy->callbacks.time = M_IntArg(enemy, instr, 0);
        Timer_Set(&enemy->callbacks.timer, 0);
        break;
    case ECL_OP_SET_TIME_CALLBACK_SUB:
        enemy->callbacks.time_sub = M_IntArg(enemy, instr, 0);
        break;
    case ECL_OP_DEATH_SUB_ON_TIMER:
        enemy->callbacks.time_sub = enemy->script.death_sub;
        Timer_Set(&enemy->callbacks.timer, 0);
        break;
    case ECL_OP_SET_INVULNERABLE:
        Timer_Set(&enemy->invulnerable_timer, M_IntArg(enemy, instr, 0));
        break;
    case ECL_OP_SET_PERIODIC_CALL:
        Timer_Set(&enemy->script.periodic.interval, M_IntArg(enemy, instr, 0));
        enemy->script.periodic.sub = M_IntArg(enemy, instr, 1);
        Timer_Set(&enemy->script.periodic.timer, 0);
        enemy->script.periodic.vars = enemy->script.state.vars;
        break;
    default:
        break;
    }
}

// Runs a sprite, effect, item, spawn, sound or stage opcode.
static void M_StepWorld(ENEMY *const enemy, ECL_INSTR *const instr)
{
    switch (instr->opcode) {
    case ECL_OP_START_SCRIPT:
        Anm_StartScript(&enemy->vm, M_IntArg(enemy, instr, 0) + M_FIRST_SCRIPT);
        break;
    case ECL_OP_START_SPRITE_SCRIPT: {
        // The original writes past its two sprites for a larger index.
        ASSERT(M_IntArg(enemy, instr, 0) < ENEMY_SPRITE_COUNT);
        const int32_t script = M_IntArg(enemy, instr, 1);
        if (script >= 0) {
            const int32_t index = M_IntArg(enemy, instr, 0);
            ASSERT(index >= 0);
            Anm_StartScript(
                &enemy->vms[index], M_IntArg(enemy, instr, 1) + M_FIRST_SCRIPT);
        } else {
            const int32_t index = M_IntArg(enemy, instr, 0);
            ASSERT(index >= 0);
            enemy->vms[index].script_index = -1;
        }
        break;
    }
    case ECL_OP_SET_TURNS:
        enemy->turns.still = M_ArgShort(instr, 0);
        enemy->turns.still_from_left = M_ArgShort(instr, 1);
        enemy->turns.still_from_right = M_ArgShort(instr, 2);
        enemy->turns.left = M_ArgShort(instr, 3);
        enemy->turns.right = M_ArgShort(instr, 4);
        enemy->flags.facing = ENEMY_FACING_UNSET;
        break;
    case ECL_OP_INTERRUPT_SCRIPT:
        enemy->vm.pending_interrupt = (int16_t)M_IntArg(enemy, instr, 0);
        break;
    case ECL_OP_INTERRUPT_SPRITE_SCRIPT: {
        const int32_t index = instr->args[0].i;
        ASSERT(index >= 0 && index < ENEMY_SPRITE_COUNT);
        enemy->vms[index].pending_interrupt = (int16_t)instr->args[1].i;
        break;
    }
    case ECL_OP_SET_SPRITE_ROTATION:
        enemy->vm.rotation.z = M_FloatArg(enemy, instr, 0);
        break;
    case ECL_OP_SET_TRAIL:
        // The original also fills the strip vertices for ENEMY_TRAIL_STRIP,
        // which the draw fills again before it reads them.
        enemy->trail.flags = (uint8_t)instr->args[0].i;
        enemy->trail.count = (int16_t)M_IntArg(enemy, instr, 1);
        enemy->trail.contact_count = (int16_t)M_IntArg(enemy, instr, 2);
        enemy->trail.step = (int16_t)M_IntArg(enemy, instr, 3);
        break;
    case ECL_OP_ADD_ORBIT:
        M_AddOrbit(enemy, instr);
        break;
    case ECL_OP_SPAWN_EFFECT: {
        // The arguments evaluate in the order of the decompilation.
        const int32_t type = M_IntArg(enemy, instr, 0);
        const int32_t count = M_IntArg(enemy, instr, 1);
        const int32_t color = *M_IntVar(enemy, instr, 2);
        Effect_Spawn(
            (EFFECT_TYPE)type, enemy->move.pos, count, (uint32_t)color);
        break;
    }
    case ECL_OP_SPAWN_MOVING_EFFECT: {
        const VEC3 velocity = {
            M_FloatArg(enemy, instr, 3),
            M_FloatArg(enemy, instr, 4),
            M_FloatArg(enemy, instr, 5),
        };
        const int32_t type = M_IntArg(enemy, instr, 0);
        const int32_t count = M_IntArg(enemy, instr, 1);
        const int32_t color = *M_IntVar(enemy, instr, 2);
        Effect_SpawnMoving(
            (EFFECT_TYPE)type, enemy->move.pos, velocity, count,
            (uint32_t)color);
        break;
    }
    case ECL_OP_SET_EFFECT_COLOR: {
        const float red = M_FloatArg(enemy, instr, 0);
        const float green = M_FloatArg(enemy, instr, 1);
        const float blue = M_FloatArg(enemy, instr, 2);
        const float alpha = M_FloatArg(enemy, instr, 3);
        Effect_SetColorScale(red, green, blue, alpha);
        break;
    }
    case ECL_OP_DROP_ITEMS:
        M_DropItems(enemy, instr, true);
        break;
    case ECL_OP_DROP_POINT_ITEMS:
        M_DropItems(enemy, instr, false);
        break;
    case ECL_OP_DROP_ITEM:
        Item_Spawn(
            enemy->move.pos, (ITEM_TYPE)M_IntArg(enemy, instr, 0),
            ITEM_FALLING);
        break;
    case ECL_OP_SPAWN:
    case ECL_OP_SPAWN_RELATIVE:
        M_SpawnChild(enemy, instr, instr->opcode == ECL_OP_SPAWN_RELATIVE);
        break;
    case ECL_OP_CLEAR_ENEMIES:
        Enemy_ClearForBonus(M_CLEAR_BONUS, 0);
        break;
    case ECL_OP_PLAY_SOUND:
        Sound_Queue(M_IntArg(enemy, instr, 0));
        break;
    case ECL_OP_SET_STAGE_LABEL:
        g_Stage.script.label = M_IntArg(enemy, instr, 0);
        break;
    case ECL_OP_ADD_CHERRY:
        GameManager_AddCherry(M_IntArg(enemy, instr, 0));
        break;
    case ECL_OP_CALL_FUNC: {
        const int32_t func = M_IntArg(enemy, instr, 0);
        ASSERT(func >= 0 && func < ECL_FUNC_COUNT);
        Ecl_RunPattern(enemy, (ECL_FUNC)func, instr->args[1].i);
        break;
    }
    case ECL_OP_SET_FRAME_FUNC:
        if (M_IntArg(enemy, instr, 0) >= 0) {
            const int32_t func = M_IntArg(enemy, instr, 0);
            ASSERT(func < ECL_FUNC_COUNT);
            enemy->script.state.frame_func = (ECL_FUNC)func;
            enemy->script.state.frame_func_arg = instr->args[1].i;
        } else {
            enemy->script.state.frame_func = ECL_FUNC_NONE;
        }
        break;
    default:
        break;
    }
}

// Runs instr, the instruction at *pc of enemy. Sets *pc to the target of
// a jump.
static M_STEP M_Step(
    ENEMY *const enemy, ECL_INSTR *const instr, int32_t *const pc)
{
    ECL_STATE *const state = &enemy->script.state;
    switch (instr->opcode) {
    case ECL_OP_DELETE:
        return M_STEP_DELETE;
    case ECL_OP_WAIT:
        Timer_Set(&state->wait, M_IntArg(enemy, instr, 0));
        return M_STEP_NEXT;
    case ECL_OP_LOOP: {
        // The count of a constant operand lives in the instruction.
        const int32_t count = *M_IntVar(enemy, instr, 2);
        M_SetInt(enemy, instr, 2, M_Wrap((int64_t)count - 1));
        if (M_IntArg(enemy, instr, 2) <= 0) {
            return M_STEP_NEXT;
        }
        state->timer.current = instr->args[0].i;
        *pc = instr->target;
        return M_STEP_JUMP;
    }
    case ECL_OP_JUMP:
        state->timer.current = instr->args[0].i;
        *pc = instr->target;
        return M_STEP_JUMP;
    case ECL_OP_JUMP_EQUAL_INT:
    case ECL_OP_JUMP_NOT_EQUAL_INT:
    case ECL_OP_JUMP_LESS_INT:
    case ECL_OP_JUMP_LESS_EQUAL_INT:
    case ECL_OP_JUMP_GREATER_INT:
    case ECL_OP_JUMP_GREATER_EQUAL_INT: {
        const int32_t a = M_IntArg(enemy, instr, 0);
        const int32_t b = M_IntArg(enemy, instr, 1);
        if (!M_CompareInts(instr->opcode, a, b)) {
            return M_STEP_NEXT;
        }
        state->timer.current = instr->args[2].i;
        *pc = instr->target;
        return M_STEP_JUMP;
    }
    case ECL_OP_JUMP_EQUAL_FLOAT:
    case ECL_OP_JUMP_NOT_EQUAL_FLOAT:
    case ECL_OP_JUMP_LESS_FLOAT:
    case ECL_OP_JUMP_LESS_EQUAL_FLOAT:
    case ECL_OP_JUMP_GREATER_FLOAT:
    case ECL_OP_JUMP_GREATER_EQUAL_FLOAT: {
        const float a = M_FloatArg(enemy, instr, 0);
        const float b = M_FloatArg(enemy, instr, 1);
        if (!M_CompareFloats(instr->opcode, a, b)) {
            return M_STEP_NEXT;
        }
        state->timer.current = instr->args[2].i;
        *pc = instr->target;
        return M_STEP_JUMP;
    }
    case ECL_OP_CALL:
        M_Call(enemy, *pc + 1, (int16_t)instr->args[0].i);
        return M_STEP_RESTART;
    case ECL_OP_RETURN:
        M_Return(enemy);
        return M_STEP_RESTART;
    case ECL_OP_INTERRUPT:
        enemy->script.pending_interrupt = M_IntArg(enemy, instr, 0);
        M_Interrupt(enemy, *pc + 1);
        return M_STEP_RESTART;
    case ECL_OP_ADD_TIME:
        Timer_AddAt(&state->timer, M_IntArg(enemy, instr, 0), g_GameSpeed);
        return M_STEP_NEXT;
    case ECL_OP_SET_INT:
    case ECL_OP_SET_FLOAT:
    case ECL_OP_RANDOM_INT:
    case ECL_OP_RANDOM_INT_FROM:
    case ECL_OP_RANDOM_FLOAT:
    case ECL_OP_RANDOM_FLOAT_FROM:
    case ECL_OP_RANDOM_SIGN_INT:
    case ECL_OP_RANDOM_SIGN_FLOAT:
    case ECL_OP_ADD_INT:
    case ECL_OP_SUB_INT:
    case ECL_OP_MUL_INT:
    case ECL_OP_DIV_INT:
    case ECL_OP_MOD_INT:
    case ECL_OP_INCREMENT:
    case ECL_OP_DECREMENT:
    case ECL_OP_ADD_FLOAT:
    case ECL_OP_SUB_FLOAT:
    case ECL_OP_MUL_FLOAT:
    case ECL_OP_DIV_FLOAT:
    case ECL_OP_MOD_FLOAT:
    case ECL_OP_SIN:
    case ECL_OP_COS:
    case ECL_OP_ANGLE:
    case ECL_OP_INTERPOLATE:
    case ECL_OP_NORMALIZE_ANGLE:
    case ECL_OP_GET_BOSS_INT:
    case ECL_OP_GET_BOSS_FLOAT:
    case ECL_OP_RANDOM_FLOAT_BETWEEN:
    case ECL_OP_RANDOM_ANGLE_IN_BOUNDS:
    case ECL_OP_POLAR:
    case ECL_OP_RANDOM_SIDE_ANGLE:
    case ECL_OP_LERP:
        M_StepVars(enemy, instr);
        return M_STEP_NEXT;
    case ECL_OP_SET_POS:
    case ECL_OP_SET_VELOCITY:
    case ECL_OP_SET_ANGULAR_VELOCITY:
    case ECL_OP_SET_SPEED:
    case ECL_OP_SET_ACCELERATION:
    case ECL_OP_AIM_AT_PLAYER:
    case ECL_OP_MOVE_ANGLE:
    case ECL_OP_MOVE_TO:
    case ECL_OP_MOVE_CIRCLE:
    case ECL_OP_SET_CIRCLE_RADIUS:
    case ECL_OP_SET_CIRCLE_ANGLE:
    case ECL_OP_TIME_ANGLE_MOVE:
    case ECL_OP_TIME_CIRCLE_MOVE:
    case ECL_OP_TIME_MOVE_TO:
    case ECL_OP_SET_BOUNDS:
    case ECL_OP_CLEAR_BOUNDS:
        M_StepMove(enemy, instr);
        return M_STEP_NEXT;
    case ECL_OP_FIRE_FAN_AT_PLAYER:
    case ECL_OP_FIRE_FAN:
    case ECL_OP_FIRE_RING_AT_PLAYER:
    case ECL_OP_FIRE_RING:
    case ECL_OP_FIRE_RING_OFFSET_AT_PLAYER:
    case ECL_OP_FIRE_RING_OFFSET:
    case ECL_OP_FIRE_RANDOM_ANGLE:
    case ECL_OP_FIRE_RING_RANDOM_SPEED:
    case ECL_OP_FIRE_RANDOM:
    case ECL_OP_SET_INTERVAL:
    case ECL_OP_SET_INTERVAL_RANDOM:
    case ECL_OP_HOLD_FIRE:
    case ECL_OP_RELEASE_FIRE:
    case ECL_OP_FIRE_AGAIN:
    case ECL_OP_SET_SHOT_OFFSET:
    case ECL_OP_SET_SHOT_COMMAND:
    case ECL_OP_CLEAR_BULLETS:
    case ECL_OP_SET_SHOT_SOUND:
    case ECL_OP_LASER:
    case ECL_OP_LASER_AT_PLAYER:
    case ECL_OP_SET_LASER_INDEX:
    case ECL_OP_TURN_LASER:
    case ECL_OP_AIM_LASER:
    case ECL_OP_MOVE_LASER:
    case ECL_OP_TEST_LASER:
    case ECL_OP_END_LASER:
    case ECL_OP_SET_RANK_GROWTH:
    case ECL_OP_FORGET_LASERS:
    case ECL_OP_CLEAR_BULLETS_IN_CIRCLE:
    case ECL_OP_DELETE_BULLETS:
    case ECL_OP_SET_LASER_ANGLE:
    case ECL_OP_HIDE_LASER_ORIGIN:
    case ECL_OP_SET_LASER_LENGTH:
    case ECL_OP_SET_LASER_OFFSETS:
        M_StepShots(enemy, instr);
        return M_STEP_NEXT;
    case ECL_OP_START_SPELL_CARD:
    case ECL_OP_END_SPELL_CARD:
    case ECL_OP_SET_DEATH_EFFECTS:
    case ECL_OP_SET_BOSS:
    case ECL_OP_SET_HITBOX:
    case ECL_OP_SET_CONTACT_DAMAGE:
    case ECL_OP_SET_TAKES_DAMAGE:
    case ECL_OP_SET_HIT_BY_SHOTS:
    case ECL_OP_SET_DEATH:
    case ECL_OP_SET_DEATH_SUB:
    case ECL_OP_SET_INTERRUPT:
    case ECL_OP_SET_LIFE:
    case ECL_OP_SET_CALLBACK_TIMER:
    case ECL_OP_SET_LIFE_CALLBACK:
    case ECL_OP_SET_LIFE_CALLBACK_SUB:
    case ECL_OP_SET_TIME_CALLBACK:
    case ECL_OP_SET_TIME_CALLBACK_SUB:
    case ECL_OP_SET_SHOOTABLE:
    case ECL_OP_SET_ROTATE_SPRITE:
    case ECL_OP_SET_BOSS_LIVES:
    case ECL_OP_SET_NO_CALL_STACK:
    case ECL_OP_SET_HIDDEN:
    case ECL_OP_DEATH_SUB_ON_TIMER:
    case ECL_OP_SET_KEEP_SPELL_CARD:
    case ECL_OP_SET_GRAZEABLE:
    case ECL_OP_SET_STAYS_OFF_SCREEN:
    case ECL_OP_SET_BOSS_SECTION:
    case ECL_OP_SET_INVULNERABLE:
    case ECL_OP_SET_PERIODIC_CALL:
    case ECL_OP_INTERRUPT_BOSS:
    case ECL_OP_READ_UNUSED:
    case ECL_OP_SET_LIFE_CALLBACK_AT:
    case ECL_OP_SET_SPELL_CIRCLE:
    case ECL_OP_SET_EXTRA_HITBOX:
    case ECL_OP_SET_HALTED_BY_BOMB:
        M_StepEnemy(enemy, instr);
        return M_STEP_NEXT;
    case ECL_OP_SPAWN:
    case ECL_OP_SPAWN_RELATIVE:
    case ECL_OP_CLEAR_ENEMIES:
    case ECL_OP_START_SCRIPT:
    case ECL_OP_SET_TURNS:
    case ECL_OP_START_SPRITE_SCRIPT:
    case ECL_OP_ADD_ORBIT:
    case ECL_OP_PLAY_SOUND:
    case ECL_OP_SPAWN_EFFECT:
    case ECL_OP_SPAWN_MOVING_EFFECT:
    case ECL_OP_DROP_ITEMS:
    case ECL_OP_CALL_FUNC:
    case ECL_OP_SET_FRAME_FUNC:
    case ECL_OP_DROP_ITEM:
    case ECL_OP_SET_STAGE_LABEL:
    case ECL_OP_INTERRUPT_SCRIPT:
    case ECL_OP_INTERRUPT_SPRITE_SCRIPT:
    case ECL_OP_SET_TRAIL:
    case ECL_OP_SET_EFFECT_COLOR:
    case ECL_OP_SET_SPRITE_ROTATION:
    case ECL_OP_DROP_POINT_ITEMS:
    case ECL_OP_ADD_CHERRY:
        M_StepWorld(enemy, instr);
        return M_STEP_NEXT;
    default:
        return M_STEP_NEXT;
    }
}

static void M_Move(ENEMY *const enemy)
{
    switch (enemy->flags.move) {
    case ENEMY_MOVE_CIRCLE: {
        enemy->move.circle.angle = Angle_AddNormalize(
            enemy->move.circle.angle,
            g_GameSpeed * enemy->move.circle.angular_velocity);
        enemy->move.circle.radius =
            g_GameSpeed * enemy->move.circle.radius_velocity
            + enemy->move.circle.radius;
        float x;
        float y;
        X87_SinCosScale(
            enemy->move.circle.angle, enemy->move.circle.radius, &x, &y);
        enemy->move.velocity.x = x + enemy->move.start.x - enemy->move.pos.x;
        enemy->move.velocity.y = y + enemy->move.start.y - enemy->move.pos.y;
        enemy->move.angle =
            X87_Atan2(enemy->move.velocity.y, enemy->move.velocity.x);
        break;
    }
    case ENEMY_MOVE_ANGLE:
        enemy->move.angle = Angle_AddNormalize(
            enemy->move.angle, g_GameSpeed * enemy->move.angular_velocity);
        enemy->move.speed =
            g_GameSpeed * enemy->move.acceleration + enemy->move.speed;
        X87_SinCosScale(
            enemy->move.angle, enemy->move.speed, &enemy->move.velocity.x,
            &enemy->move.velocity.y);
        enemy->move.velocity.z = 0.0f;
        break;
    case ENEMY_MOVE_TO: {
        Timer_SubAt(&enemy->move.timer, 1, g_GameSpeed);
        float t = 1.0f
            - Timer_AsFloat(&enemy->move.timer) / (float)enemy->move.duration;
        if (t < 0.0f) {
            t = 0.0f;
        }
        t = M_Ease(enemy->flags.ease, t);
        enemy->move.velocity = Vec3_Sub(
            Vec3_Add(Vec3_Scale(enemy->move.delta, t), enemy->move.start),
            enemy->move.pos);
        if (enemy->flags.mirrored) {
            enemy->move.velocity.x = -enemy->move.velocity.x;
        }
        enemy->move.angle =
            X87_Atan2(enemy->move.velocity.y, enemy->move.velocity.x);
        if (enemy->move.timer.current <= 0) {
            enemy->flags.move = ENEMY_MOVE_VELOCITY;
            enemy->move.pos = Vec3_Add(enemy->move.start, enemy->move.delta);
            enemy->move.velocity = (VEC3) {};
        }
        return;
    }
    default:
        return;
    }
    if (enemy->move.duration > 0) {
        Timer_SubAt(&enemy->move.timer, 1, g_GameSpeed);
        if (enemy->move.timer.current <= 0) {
            enemy->flags.move = ENEMY_MOVE_VELOCITY;
        }
    }
}

static void M_Turn(ENEMY *const enemy)
{
    uint8_t facing = ENEMY_FACING_STILL;
    if (enemy->move.velocity.x < -M_TURN_SPEED) {
        facing = enemy->flags.mirrored ? ENEMY_FACING_RIGHT : ENEMY_FACING_LEFT;
    } else if (enemy->move.velocity.x > M_TURN_SPEED) {
        facing = enemy->flags.mirrored ? ENEMY_FACING_LEFT : ENEMY_FACING_RIGHT;
    }
    if (enemy->flags.facing == facing) {
        return;
    }
    int32_t script = enemy->turns.right;
    if (facing == ENEMY_FACING_LEFT) {
        script = enemy->turns.left;
    } else if (facing == ENEMY_FACING_STILL) {
        if (enemy->flags.facing == ENEMY_FACING_UNSET) {
            script = enemy->turns.still;
        } else if (enemy->flags.facing == ENEMY_FACING_LEFT) {
            script = enemy->turns.still_from_left;
        } else {
            script = enemy->turns.still_from_right;
        }
    }
    Anm_StartScript(&enemy->vm, script + M_FIRST_SCRIPT);
    enemy->flags.facing = facing;
}

static void M_UpdateSpellCardBonus(ENEMY *const enemy)
{
    if (!enemy->flags.boss || enemy->boss_index != 0
        || g_Enemies.spell_card.state == ENEMY_SPELL_NONE
        || !g_Enemies.spell_card.capturable) {
        return;
    }
    if (!enemy->flags.keep_spell_card) {
        const int32_t index = g_Enemies.spell_card.index;
        g_Enemies.spell_card.bonus = X87_Ftol(
            (float)m_SpellCardBonuses[index]
            - (float)g_Enemies.spell_card.bonus_step
                * Timer_AsFloat(&g_Enemies.spell_card.bonus_timer) / 60.0f);
        g_Enemies.spell_card.bonus -= g_Enemies.spell_card.bonus % 10;
    }
    Timer_TickAt(&g_Enemies.spell_card.bonus_timer, g_GameSpeed);
}

static void M_UpdateHiddenByBomb(ENEMY *const enemy)
{
    if (!enemy->flags.boss || g_GameManager.run.stage < M_STAGE_EXTRA) {
        return;
    }
    if (g_Player.bomb.running && g_Enemies.spell_card.state != ENEMY_SPELL_NONE
        && g_Enemies.spell_card.index >= M_HIDDEN_SPELL_CARDS) {
        enemy->flags.hidden_by_bomb = true;
        enemy->bomb_hide_frames = 1;
    } else if (enemy->bomb_hide_frames > 0) {
        enemy->bomb_hide_frames--;
    } else {
        enemy->flags.hidden_by_bomb = false;
    }
}

void Ecl_StartSub(ECL_STATE *const state, const int16_t sub)
{
    ASSERT(Ecl_IsSub(sub));
    state->pc = 0;
    Timer_Set(&state->timer, 0);
    Timer_Set(&state->wait, 0);
    state->sub = sub;
}

ECL_RUN Ecl_RunEnemy(ENEMY *const enemy)
{
    ECL_STATE *const state = &enemy->script.state;
    const uint32_t difficulty = 1u
        << ((uint32_t)g_GameManager.run.difficulty & 31);
    int32_t pc;
restart:
    pc = state->pc;
    if (enemy->script.pending_interrupt >= 0) {
        // The saved state resumes after the instruction due next, so a
        // pending interrupt skips it.
        M_Interrupt(enemy, pc + 1);
        goto restart;
    }
    if (M_StartPeriodic(enemy)) {
        pc = state->pc;
    }
    for (;;) {
        if (state->wait.current > 0) {
            Timer_SubAt(&state->wait, 1, g_GameSpeed);
            Timer_SubAt(&state->timer, 1, g_GameSpeed);
            break;
        }
        ECL_INSTR *const instr = Ecl_GetInstr(state->sub, pc);
        if (state->timer.current != instr->time) {
            break;
        }
        ASSERT(instr->time != -1);
        if ((instr->difficulty_mask & difficulty) == 0) {
            pc++;
            continue;
        }
        switch (M_Step(enemy, instr, &pc)) {
        case M_STEP_NEXT:
            pc++;
            break;
        case M_STEP_JUMP:
            break;
        case M_STEP_RESTART:
            goto restart;
        case M_STEP_DELETE:
            return ECL_RUN_DELETE;
        }
    }
    M_Move(enemy);
    if (enemy->life > 0) {
        if (enemy->shots.interval > 0) {
            Timer_TickAt(&enemy->shots.timer, g_GameSpeed);
            if (enemy->shots.timer.current >= enemy->shots.interval) {
                enemy->shots.shooter.pos =
                    Vec3_Add(enemy->move.pos, enemy->shots.offset);
                Bullet_FirePattern(&enemy->shots.shooter);
                Timer_Set(&enemy->shots.timer, 0);
            }
        }
        if (enemy->turns.left >= 0) {
            M_Turn(enemy);
        }
        if (state->frame_func != ECL_FUNC_NONE) {
            Ecl_RunPattern(enemy, state->frame_func, state->frame_func_arg);
        }
        M_RunInterps(enemy);
    }
    state->pc = pc;
    Timer_TickAt(&state->timer, g_GameSpeed);
    M_UpdateSpellCardBonus(enemy);
    M_UpdateHiddenByBomb(enemy);
    return ECL_RUN_CONTINUE;
}
