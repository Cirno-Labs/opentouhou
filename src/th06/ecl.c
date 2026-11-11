#include "th06/ecl.h"

#include "th06/angle.h"
#include "th06/anm.h"
#include "th06/bullet.h"
#include "th06/effect.h"
#include "th06/enemy.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"
#include "th06/sound.h"
#include "th06/stage.h"
#include "th06/stubs.h"

#include "core/base/assert.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "core/io/reader.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#include <string.h>

#define M_HEADER_SIZE 0x10
#define M_INSTR_HEADER_SIZE 12
#define M_ARG_COUNT 16
#define M_END_TIME -1
#define M_TIMELINE_HEADER_SIZE 8
#define M_TIMELINE_ARGS_SIZE 20
#define M_SPELL_CARD_COUNT 0x40
#define M_SPELL_NAME_OFFSET 4
#define M_MAX_DEPTH 7
#define M_FIRST_SCRIPT 0x100
#define M_LARGE_HEIGHT 30.0f
#define M_PI 3.1415927f
#define M_TWO_PI 6.2831855f
#define M_HALF_PI 1.5707964f
#define M_QUARTER_PI 0.7853982f

typedef enum {
    M_OP_DELETE = 1,
    M_OP_JUMP = 2,
    M_OP_LOOP = 3, // decrements a counter and jumps while it is above 0
    M_OP_SET_INT = 4,
    M_OP_SET_FLOAT = 5,
    M_OP_RANDOM_INT = 6,
    M_OP_RANDOM_INT_FROM = 7,
    M_OP_RANDOM_FLOAT = 8,
    M_OP_RANDOM_FLOAT_FROM = 9,
    M_OP_GET_X = 10,
    M_OP_GET_Y = 11,
    M_OP_GET_Z = 12,
    M_OP_ADD_INT = 13,
    M_OP_SUB_INT = 14,
    M_OP_MUL_INT = 15,
    M_OP_DIV_INT = 16,
    M_OP_MOD_INT = 17,
    M_OP_INCREMENT = 18,
    M_OP_DECREMENT = 19,
    M_OP_ADD_FLOAT = 20,
    M_OP_SUB_FLOAT = 21,
    M_OP_MUL_FLOAT = 22,
    M_OP_DIV_FLOAT = 23,
    M_OP_MOD_FLOAT = 24,
    M_OP_ANGLE = 25,
    M_OP_NORMALIZE_ANGLE = 26,
    M_OP_COMPARE_INT = 27,
    M_OP_COMPARE_FLOAT = 28,
    M_OP_JUMP_LESS = 29,
    M_OP_JUMP_LESS_EQUAL = 30,
    M_OP_JUMP_EQUAL = 31,
    M_OP_JUMP_GREATER = 32,
    M_OP_JUMP_GREATER_EQUAL = 33,
    M_OP_JUMP_NOT_EQUAL = 34,
    M_OP_CALL = 35,
    M_OP_RETURN = 36,
    M_OP_CALL_LESS = 37,
    M_OP_CALL_LESS_EQUAL = 38,
    M_OP_CALL_EQUAL = 39,
    M_OP_CALL_GREATER = 40,
    M_OP_CALL_GREATER_EQUAL = 41,
    M_OP_CALL_NOT_EQUAL = 42,
    M_OP_SET_POS = 43,
    M_OP_SET_VELOCITY = 44,
    M_OP_SET_ANGLE_SPEED = 45,
    M_OP_SET_ANGULAR_VELOCITY = 46,
    M_OP_SET_SPEED = 47,
    M_OP_SET_ACCELERATION = 48,
    M_OP_RANDOM_ANGLE = 49,
    M_OP_RANDOM_ANGLE_IN_BOUNDS = 50,
    M_OP_AIM_AT_PLAYER = 51,
    M_OP_MOVE_ANGLE_OUT = 52,
    M_OP_MOVE_ANGLE_OUT4 = 53,
    M_OP_MOVE_ANGLE_IN = 54,
    M_OP_MOVE_ANGLE_IN4 = 55,
    M_OP_MOVE_TO_LINEAR = 56,
    M_OP_MOVE_TO_OUT = 57,
    M_OP_MOVE_TO_OUT4 = 58,
    M_OP_MOVE_TO_IN = 59,
    M_OP_MOVE_TO_IN4 = 60,
    M_OP_MOVE_TIME_OUT = 61,
    M_OP_MOVE_TIME_OUT4 = 62,
    M_OP_MOVE_TIME_IN = 63,
    M_OP_MOVE_TIME_IN4 = 64,
    M_OP_SET_BOUNDS = 65,
    M_OP_CLEAR_BOUNDS = 66,
    M_OP_FIRE_FIRST = 67, // to 75, one for each BULLET_AIM
    M_OP_FIRE_LAST = 75,
    M_OP_SET_INTERVAL = 76,
    M_OP_SET_INTERVAL_RANDOM = 77,
    M_OP_HOLD_FIRE = 78,
    M_OP_RELEASE_FIRE = 79,
    M_OP_FIRE_AGAIN = 80,
    M_OP_SET_SHOT_OFFSET = 81,
    M_OP_SET_SHOT_EXTRAS = 82,
    M_OP_CLEAR_BULLETS = 83,
    M_OP_SET_SHOT_SOUND = 84,
    M_OP_LASER = 85,
    M_OP_LASER_AT_PLAYER = 86,
    M_OP_SET_LASER_INDEX = 87,
    M_OP_TURN_LASER = 88,
    M_OP_AIM_LASER = 89,
    M_OP_MOVE_LASER = 90,
    M_OP_TEST_LASER = 91,
    M_OP_END_LASER = 92,
    M_OP_START_SPELL_CARD = 93,
    M_OP_END_SPELL_CARD = 94,
    M_OP_SPAWN = 95,
    M_OP_KILL_ALL = 96,
    M_OP_START_SCRIPT = 97,
    M_OP_SET_TURNS = 98,
    M_OP_START_SPRITE_SCRIPT = 99,
    M_OP_SET_DEATH_EFFECTS = 100,
    M_OP_SET_BOSS = 101,
    M_OP_ADD_ORBIT = 102,
    M_OP_SET_HITBOX = 103,
    M_OP_SET_CONTACT_DAMAGE = 104,
    M_OP_SET_TAKES_DAMAGE = 105,
    M_OP_PLAY_SOUND = 106,
    M_OP_SET_DEATH = 107,
    M_OP_SET_DEATH_SUB = 108,
    M_OP_SET_INTERRUPT = 109,
    M_OP_INTERRUPT = 110,
    M_OP_SET_LIFE = 111,
    M_OP_SET_CALLBACK_TIMER = 112,
    M_OP_SET_LIFE_CALLBACK = 113,
    M_OP_SET_LIFE_CALLBACK_SUB = 114,
    M_OP_SET_TIME_CALLBACK = 115,
    M_OP_SET_TIME_CALLBACK_SUB = 116,
    M_OP_SET_SHOOTABLE = 117,
    M_OP_SPAWN_EFFECT = 118,
    M_OP_DROP_ITEMS = 119,
    M_OP_SET_ROTATE_SPRITE = 120,
    M_OP_CALL_FUNC = 121,
    M_OP_SET_FRAME_FUNC = 122,
    M_OP_ADD_TIME = 123,
    M_OP_DROP_ITEM = 124,
    M_OP_RESUME_STAGE = 125,
    M_OP_SET_BOSS_LIVES = 126,
    M_OP_INTERRUPT_SCRIPT = 128,
    M_OP_INTERRUPT_SPRITE_SCRIPT = 129,
    M_OP_SET_NO_CALL_STACK = 130,
    M_OP_SET_RANK_GROWTH = 131,
    M_OP_SET_HIDDEN = 132,
    M_OP_DEATH_SUB_ON_TIMER = 133,
    M_OP_FORGET_LASERS = 134,
    M_OP_SET_KEEP_SPELL_CARD = 135,
} M_OP;

// Holds one parsed instruction. args holds the argument words as the file
// stores them. Operands are resolved from them, and opcodes 18 and 19
// write through an operand that names no variable into args itself.
typedef struct {
    int32_t time;
    int16_t opcode;
    uint8_t difficulty_mask;
    int32_t target; // the instruction a jump goes to
    int32_t args[M_ARG_COUNT];
    char *text; // the spell card name of opcode 93, in UTF-8
} M_INSTR;

// Holds the instructions of a subroutine. The last one has time
// M_END_TIME and never runs.
typedef struct {
    M_INSTR *instrs;
    int32_t count;
} M_SUB;

// Tells where an operand points.
typedef enum {
    M_OPERAND_INT = 0, // a variable that holds an int
    M_OPERAND_FLOAT = 1, // a variable that holds a float
    M_OPERAND_READ_ONLY = 2,
    M_OPERAND_VALUE = 3, // the operand itself
} M_OPERAND;

typedef enum {
    M_STEP_NEXT,
    M_STEP_JUMP,
    M_STEP_RESTART, // after a call, a return or an interrupt
    M_STEP_DELETE,
} M_STEP;

static const char *const m_Paths[8] = {
    "dummy",
    "data/ecldata1.ecl",
    "data/ecldata2.ecl",
    "data/ecldata3.ecl",
    "data/ecldata4.ecl",
    "data/ecldata5.ecl",
    "data/ecldata6.ecl",
    "data/ecldata7.ecl",
};

static const int32_t m_SpellCardBonuses[M_SPELL_CARD_COUNT] = {
    200000, 200000, 200000, 200000, 200000, 200000, 200000, 250000,
    250000, 250000, 250000, 250000, 250000, 250000, 300000, 300000,
    300000, 300000, 300000, 300000, 300000, 300000, 300000, 300000,
    300000, 300000, 300000, 300000, 300000, 300000, 300000, 300000,
    400000, 400000, 400000, 400000, 400000, 400000, 400000, 400000,
    500000, 500000, 500000, 500000, 500000, 500000, 600000, 600000,
    600000, 600000, 600000, 700000, 700000, 700000, 700000, 700000,
    700000, 700000, 700000, 700000, 700000, 700000, 700000, 700000,
};

static const int32_t m_ShotTypeValues[2][2][3] = {
    { { 0, 3, 1 }, { 2, 3, 4 } },
    { { 1, 4, 0 }, { 4, 2, 3 } },
};

static const uint32_t m_OrbitColors[0x1c] = {
    0xff000000, 0xff303030, 0xff606060, 0xff500000, 0xff900000, 0xffff2020,
    0xff400040, 0xff800080, 0xffff30ff, 0xff000050, 0xff000090, 0xff2020ff,
    0xff203060, 0xff304090, 0xff3080ff, 0xff005000, 0xff009000, 0xff20ff20,
    0xff206000, 0xff409010, 0xff80ff20, 0xff505000, 0xff909000, 0xffffff20,
    0xff603000, 0xff904010, 0xfff08020, 0xffffffff,
};

static M_SUB *m_Subs;
static int32_t m_SubCount;
static ECL_TIMELINE_ENTRY *m_Timeline;

// Holds the star of ECL_FUNC_STAR_PATTERN: its corners, and where its
// centre moves from and to. It lasts across loads.
static struct {
    float angles[6];
    VEC3 from, to;
} m_Star;

// Hold the values that operands -10021, -10023 and -10025 point at.
static float m_PlayerAngle;
static float m_PlayerDistance;
static int32_t m_ShotTypeOperand;

static int32_t M_LoadInt(const void *const ref)
{
    int32_t value;
    memcpy(&value, ref, sizeof(value));
    return value;
}

static float M_LoadFloat(const void *const ref)
{
    float value;
    memcpy(&value, ref, sizeof(value));
    return value;
}

static void M_StoreInt(void *const ref, const int32_t value)
{
    memcpy(ref, &value, sizeof(value));
}

static void M_StoreFloat(void *const ref, const float value)
{
    memcpy(ref, &value, sizeof(value));
}

static int32_t M_Wrap(const int64_t value)
{
    return (int32_t)(uint32_t)(uint64_t)value;
}

static float M_ArgFloat(const M_INSTR *const instr, const int32_t index)
{
    return M_LoadFloat(&instr->args[index]);
}

static int16_t M_ArgShort(const M_INSTR *const instr, const int32_t index)
{
    const uint32_t word = (uint32_t)instr->args[index / 2];
    return (int16_t)(uint16_t)(word >> (16 * (index % 2)));
}

static uint8_t M_ArgByte(const M_INSTR *const instr, const int32_t index)
{
    const uint32_t word = (uint32_t)instr->args[index / 4];
    return (uint8_t)(word >> (8 * (index % 4)));
}

static VEC3 M_ArgVec3(const M_INSTR *const instr, const int32_t index)
{
    return (VEC3) {
        M_ArgFloat(instr, index),
        M_ArgFloat(instr, index + 1),
        M_ArgFloat(instr, index + 2),
    };
}

// Returns the argument of a pattern function, opcode 121's second word.
// A frame function has no instruction, so a function that reads it is
// fatal there.
static int32_t M_FuncArg(const M_INSTR *const instr)
{
    ASSERT(instr != nullptr);
    return instr->args[1];
}

static void M_SinCosScale(VEC3 *const v, const float angle, const float radius)
{
    X87_SinCosScale(angle, radius, &v->x, &v->y);
}

static float M_RankLerpFloat(const float low, const float high)
{
    return (float)g_GameManager.rank.value * (high - low) / 32.0f + low;
}

static int32_t M_RankLerp(const int32_t low, const int32_t high)
{
    return g_GameManager.rank.value * (high - low) / 32 + low;
}

static bool M_IsSub(const int32_t sub)
{
    return sub >= 0 && sub < m_SubCount;
}

// Returns the slot of the call stack at depth. A depth of -1 is the
// running state itself, as the stack follows it in memory in the
// original; a return at depth 0 reaches it.
static ECL_STATE *M_StackSlot(ENEMY *const enemy, const int32_t depth)
{
    ASSERT(depth >= -1 && depth < ENEMY_STACK_DEPTH);
    return depth < 0 ? &enemy->script.state : &enemy->script.stack[depth];
}

// Returns where operand points: one of the variables -10001 to -10025, or
// operand itself. Sets *type, when type is not nullptr.
static void *M_Resolve(
    ENEMY *const enemy, void *const operand, M_OPERAND *const type)
{
    ECL_STATE *const state = &enemy->script.state;
    M_OPERAND kind = M_OPERAND_VALUE;
    void *ref = operand;
    const int32_t value = M_LoadInt(operand);
    if (value <= -10001 && value >= -10004) {
        kind = M_OPERAND_INT;
        ref = &state->int_vars[-10001 - value];
    } else if (value <= -10005 && value >= -10008) {
        kind = M_OPERAND_FLOAT;
        ref = &state->float_vars[-10005 - value];
    } else if (value <= -10009 && value >= -10012) {
        kind = M_OPERAND_INT;
        ref = &state->int_vars[4 + (-10009 - value)];
    } else {
        switch (value) {
        case -10013:
            kind = M_OPERAND_READ_ONLY;
            ref = &g_GameManager.run.difficulty;
            break;
        case -10014:
            kind = M_OPERAND_READ_ONLY;
            ref = &g_GameManager.rank.value;
            break;
        case -10015:
            kind = M_OPERAND_FLOAT;
            ref = &enemy->move.pos.x;
            break;
        case -10016:
            kind = M_OPERAND_FLOAT;
            ref = &enemy->move.pos.y;
            break;
        case -10017:
            kind = M_OPERAND_FLOAT;
            ref = &enemy->move.pos.z;
            break;
        case -10018:
            kind = M_OPERAND_READ_ONLY;
            ref = &g_Player.pos.x;
            break;
        case -10019:
            kind = M_OPERAND_READ_ONLY;
            ref = &g_Player.pos.y;
            break;
        case -10020:
            kind = M_OPERAND_READ_ONLY;
            ref = &g_Player.pos.z;
            break;
        case -10021:
            m_PlayerAngle = Player_AngleFrom(enemy->move.pos);
            kind = M_OPERAND_READ_ONLY;
            ref = &m_PlayerAngle;
            break;
        case -10022:
            kind = M_OPERAND_INT;
            ref = &enemy->callbacks.timer.current;
            break;
        case -10023: {
            const VEC3 d = Vec3_Sub(g_Player.pos, enemy->move.pos);
            m_PlayerDistance = X87_Sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            kind = M_OPERAND_READ_ONLY;
            ref = &m_PlayerDistance;
            break;
        }
        case -10024:
            kind = M_OPERAND_INT;
            ref = &enemy->life;
            break;
        case -10025:
            m_ShotTypeOperand =
                g_GameManager.run.character * 2 + g_GameManager.run.shot_type;
            kind = M_OPERAND_INT;
            ref = &m_ShotTypeOperand;
            break;
        default:
            break;
        }
    }
    if (type != nullptr) {
        *type = kind;
    }
    return ref;
}

// Resolves a float operand: its value truncated to an int names the
// variable. A float operand that names an int variable reads its bits.
static void *M_ResolveFloat(
    ENEMY *const enemy, void *const operand, M_OPERAND *const type)
{
    int32_t value = X87_Ftol(M_LoadFloat(operand));
    void *const ref = M_Resolve(enemy, &value, type);
    return ref == &value ? operand : ref;
}

static int32_t M_Int(ENEMY *const enemy, void *const operand)
{
    return M_LoadInt(M_Resolve(enemy, operand, nullptr));
}

static float M_Float(ENEMY *const enemy, void *const operand)
{
    return M_LoadFloat(M_ResolveFloat(enemy, operand, nullptr));
}

// Copies the bits of operand src into variable dst. A src that names a
// variable is read through it, even when it is a computed value whose
// bits happen to name one.
static void M_Set(ENEMY *const enemy, int32_t dst, void *const src)
{
    const void *const value = M_Resolve(enemy, src, nullptr);
    M_OPERAND type;
    void *const ref = M_Resolve(enemy, &dst, &type);
    if (type == M_OPERAND_INT || type == M_OPERAND_FLOAT) {
        memmove(ref, value, sizeof(int32_t));
    }
}

// Sets variable dst to a op b, as ints or as floats by the type of dst.
static void M_Arith(
    ENEMY *const enemy, const M_OP op, int32_t dst, void *const a,
    void *const b)
{
    M_OPERAND type;
    void *const ref = M_Resolve(enemy, &dst, &type);
    if (type == M_OPERAND_INT) {
        const int64_t x = M_Int(enemy, a);
        const int64_t y = M_Int(enemy, b);
        switch (op) {
        case M_OP_ADD_INT:
        case M_OP_ADD_FLOAT:
            M_StoreInt(ref, M_Wrap(x + y));
            break;
        case M_OP_SUB_INT:
        case M_OP_SUB_FLOAT:
            M_StoreInt(ref, M_Wrap(x - y));
            break;
        case M_OP_MUL_INT:
        case M_OP_MUL_FLOAT:
            M_StoreInt(ref, M_Wrap(x * y));
            break;
        case M_OP_DIV_INT:
        case M_OP_DIV_FLOAT:
            ASSERT(y != 0 && (x != INT32_MIN || y != -1));
            M_StoreInt(ref, (int32_t)(x / y));
            break;
        default:
            ASSERT(y != 0 && (x != INT32_MIN || y != -1));
            M_StoreInt(ref, (int32_t)(x % y));
            break;
        }
    } else if (type == M_OPERAND_FLOAT) {
        const float x = M_Float(enemy, a);
        const float y = M_Float(enemy, b);
        switch (op) {
        case M_OP_ADD_INT:
        case M_OP_ADD_FLOAT:
            M_StoreFloat(ref, x + y);
            break;
        case M_OP_SUB_INT:
        case M_OP_SUB_FLOAT:
            M_StoreFloat(ref, x - y);
            break;
        case M_OP_MUL_INT:
        case M_OP_MUL_FLOAT:
            M_StoreFloat(ref, x * y);
            break;
        case M_OP_DIV_INT:
        case M_OP_DIV_FLOAT:
            M_StoreFloat(ref, x / y);
            break;
        default:
            M_StoreFloat(ref, Math_Fmod(x, y));
            break;
        }
    }
}

static void M_Angle(ENEMY *const enemy, M_INSTR *const instr)
{
    int32_t dst = instr->args[0];
    M_OPERAND type;
    void *const ref = M_Resolve(enemy, &dst, &type);
    if (type == M_OPERAND_FLOAT) {
        const float x1 = M_Float(enemy, &instr->args[1]);
        const float y1 = M_Float(enemy, &instr->args[2]);
        const float x2 = M_Float(enemy, &instr->args[3]);
        const float y2 = M_Float(enemy, &instr->args[4]);
        M_StoreFloat(ref, X87_Atan2(y2 - y1, x2 - x1));
    }
}

static void M_MoveAngle(ENEMY *const enemy, M_INSTR *const instr)
{
    const int32_t duration = instr->args[0];
    const float angle = M_Float(enemy, &instr->args[1]);
    const float speed = M_ArgFloat(instr, 2);
    enemy->move.delta.x = X87_Cos(angle) * speed * (float)duration / 2.0f;
    enemy->move.delta.y = X87_Sin(angle) * speed * (float)duration / 2.0f;
    enemy->move.delta.z = 0.0f;
    enemy->move.start = enemy->move.pos;
    enemy->move.duration = duration;
    Timer_Set(&enemy->move.timer, enemy->move.duration);
    enemy->flags.move = ENEMY_MOVE_TO;
}

static void M_MoveTo(ENEMY *const enemy, M_INSTR *const instr)
{
    const float x = M_Float(enemy, &instr->args[1]);
    const float y = M_Float(enemy, &instr->args[2]);
    const float z = M_Float(enemy, &instr->args[3]);
    enemy->move.delta = Vec3_Sub((VEC3) { x, y, z }, enemy->move.pos);
    enemy->move.start = enemy->move.pos;
    enemy->move.duration = instr->args[0];
    Timer_Set(&enemy->move.timer, enemy->move.duration);
    enemy->flags.move = ENEMY_MOVE_TO;
    enemy->move.velocity = (VEC3) {};
}

// Moves at the angle and speed of the enemy over a duration. The angle is
// resolved as a float operand, so an angle that truncates to a variable
// number reads that variable.
static void M_MoveTime(ENEMY *const enemy, const M_INSTR *const instr)
{
    const int32_t duration = instr->args[0];
    const float angle = M_Float(enemy, &enemy->move.angle);
    const float speed = enemy->move.speed;
    enemy->move.delta.x = X87_Cos(angle) * speed * (float)duration / 2.0f;
    enemy->move.delta.y = X87_Sin(angle) * speed * (float)duration / 2.0f;
    enemy->move.delta.z = 0.0f;
    enemy->move.start = enemy->move.pos;
    enemy->move.duration = duration;
    Timer_Set(&enemy->move.timer, enemy->move.duration);
    enemy->flags.move = ENEMY_MOVE_TO;
}

// Picks a random angle and turns it away from the edges of the bounds
// that the enemy is near.
static void M_RandomAngleInBounds(
    ENEMY *const enemy, const M_INSTR *const instr)
{
    const float low = M_ArgFloat(instr, 0);
    const float high = M_ArgFloat(instr, 1);
    float angle = Rng_F32InRange(&g_Rng, high - low) + low;
    if (enemy->move.bounds_min.x + 96.0f > enemy->move.pos.x) {
        if (angle > M_HALF_PI) {
            angle = M_PI - angle;
        } else if (angle < -M_HALF_PI) {
            angle = -M_PI - angle;
        }
    }
    if (enemy->move.bounds_max.x - 96.0f < enemy->move.pos.x) {
        if (angle < M_HALF_PI && angle >= 0.0f) {
            angle = M_PI - angle;
        } else if (angle > -M_HALF_PI && angle <= 0.0f) {
            angle = -M_PI - angle;
        }
    }
    if (enemy->move.bounds_min.y + 48.0f > enemy->move.pos.y && angle < 0.0f) {
        angle = -angle;
    }
    if (enemy->move.bounds_max.y - 48.0f < enemy->move.pos.y && angle > 0.0f) {
        angle = -angle;
    }
    enemy->move.angle = angle;
}

static void M_Fire(ENEMY *const enemy, M_INSTR *const instr)
{
    BULLET_SHOOTER *const shooter = &enemy->shots.shooter;
    shooter->type = M_ArgShort(instr, 0);
    shooter->aim = (BULLET_AIM)(instr->opcode - M_OP_FIRE_FIRST);
    shooter->columns = (int16_t)M_Int(enemy, &instr->args[1]);
    shooter->columns = (int16_t)(shooter->columns
                                 + M_RankLerp(
                                     enemy->shots.rank_columns_low,
                                     enemy->shots.rank_columns_high));
    if (shooter->columns <= 0) {
        shooter->columns = 1;
    }
    shooter->rows = (int16_t)M_Int(enemy, &instr->args[2]);
    shooter->rows =
        (int16_t)(shooter->rows
                  + M_RankLerp(
                      enemy->shots.rank_rows_low, enemy->shots.rank_rows_high));
    if (shooter->rows <= 0) {
        shooter->rows = 1;
    }
    shooter->pos = Vec3_Add(enemy->move.pos, enemy->shots.offset);
    shooter->angle1 = M_Float(enemy, &instr->args[5]);
    shooter->angle1 = Angle_AddNormalize(shooter->angle1, 0.0f);
    shooter->speed1 = M_Float(enemy, &instr->args[3]);
    if (shooter->speed1 != 0.0f) {
        shooter->speed1 += M_RankLerpFloat(
            enemy->shots.rank_speed_low, enemy->shots.rank_speed_high);
        if (shooter->speed1 < 0.3f) {
            shooter->speed1 = 0.3f;
        }
    }
    shooter->angle2 = M_Float(enemy, &instr->args[6]);
    shooter->speed2 = M_Float(enemy, &instr->args[4]);
    shooter->speed2 +=
        M_RankLerpFloat(
            enemy->shots.rank_speed_low, enemy->shots.rank_speed_high)
        / 2.0f;
    if (shooter->speed2 < 0.3f) {
        shooter->speed2 = 0.3f;
    }
    shooter->flags = instr->args[7];
    int32_t color = M_ArgShort(instr, 1);
    shooter->color = (int16_t)M_Int(enemy, &color);
    if (!enemy->flags.hold_fire) {
        Bullet_FirePattern(shooter);
    }
}

static void M_Laser(ENEMY *const enemy, M_INSTR *const instr)
{
    BULLET_SHOOTER *const shooter = &enemy->shots.laser_shooter;
    shooter->pos = Vec3_Add(enemy->move.pos, enemy->shots.offset);
    shooter->type = M_ArgShort(instr, 0);
    shooter->color = M_ArgShort(instr, 1);
    shooter->angle1 = M_Float(enemy, &instr->args[1]);
    shooter->speed1 = M_Float(enemy, &instr->args[2]);
    shooter->ex_floats[0] = M_Float(enemy, &instr->args[3]);
    shooter->ex_floats[1] = M_Float(enemy, &instr->args[4]);
    shooter->ex_floats[2] = M_Float(enemy, &instr->args[5]);
    shooter->ex_floats[3] = M_ArgFloat(instr, 6);
    shooter->ex_ints[0] = instr->args[7];
    shooter->ex_ints[1] = instr->args[8];
    shooter->ex_ints[2] = instr->args[9];
    shooter->ex_ints[3] = instr->args[10];
    shooter->laser_end_hit_frames = (uint32_t)instr->args[11];
    shooter->flags = instr->args[12];
    shooter->aim = instr->opcode == M_OP_LASER_AT_PLAYER
        ? BULLET_AIM_FAN_AT_PLAYER
        : BULLET_AIM_FAN;
    ASSERT(
        enemy->shots.laser_index >= 0
        && enemy->shots.laser_index < ENEMY_LASER_COUNT);
    enemy->shots.lasers[enemy->shots.laser_index] = Bullet_SpawnLaser(shooter);
}

static LASER *M_GetLaser(const ENEMY *const enemy, const int32_t index)
{
    ASSERT(index >= 0 && index < ENEMY_LASER_COUNT);
    return enemy->shots.lasers[index];
}

static void M_StartSpellCard(ENEMY *const enemy, const M_INSTR *const instr)
{
    Gui_ShowSpellCardName(M_ArgShort(instr, 0), instr->text);
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.state = ENEMY_SPELL_RUNNING;
    g_Enemies.spell_card.index = M_ArgShort(instr, 1);
    ASSERT(
        g_Enemies.spell_card.index >= 0
        && g_Enemies.spell_card.index < M_SPELL_CARD_COUNT);
    g_Enemies.spell_card.bonus = m_SpellCardBonuses[g_Enemies.spell_card.index];
    Bullet_ClearAllToItems();
    g_Stage.spell_card.state = 1;
    g_Stage.spell_card.frames = 0;
    enemy->shots.rank_speed_low = -0.5f;
    enemy->shots.rank_speed_high = 0.5f;
    enemy->shots.rank_columns_low = 0;
    enemy->shots.rank_columns_high = 0;
    enemy->shots.rank_rows_low = 0;
    enemy->shots.rank_rows_high = 0;
    if (!g_GameManager.mode.in_replay) {
        ResultScreen_AddSpellCardAttempt(
            g_Enemies.spell_card.index, instr->text,
            g_Enemies.spell_card.bonus);
    }
}

static void M_EndSpellCard(void)
{
    if (g_Enemies.spell_card.state != ENEMY_SPELL_NONE) {
        Gui_EndSpellCardName();
        if (g_Enemies.spell_card.state == ENEMY_SPELL_RUNNING) {
            Bullet_ClearAllWithBonus(0x3200, true);
            if (g_Enemies.spell_card.capturable) {
                const int32_t bonus = g_Enemies.spell_card.bonus
                    + g_Enemies.spell_card.bonus * g_Gui.boss.seconds / 10;
                Gui_ShowSpellCardBonus(bonus);
                g_GameManager.score.value += (uint32_t)bonus;
                if (!g_GameManager.mode.in_replay) {
                    ResultScreen_AddSpellCardCapture(
                        g_Enemies.spell_card.index,
                        g_GameManager.run.character * 2
                            + g_GameManager.run.shot_type);
                }
                g_GameManager.stats.spell_cards_captured++;
            }
        }
        g_Enemies.spell_card.state = ENEMY_SPELL_NONE;
    }
    g_Stage.spell_card.state = 0;
}

static void M_Spawn(ENEMY *const enemy, M_INSTR *const instr)
{
    VEC3 pos = M_ArgVec3(instr, 1);
    pos.x = M_Float(enemy, &pos.x);
    pos.y = M_Float(enemy, &pos.y);
    pos.z = M_Float(enemy, &pos.z);
    Enemy_Spawn(
        instr->args[0], pos, M_ArgShort(instr, 8), M_ArgShort(instr, 9),
        instr->args[5]);
}

static void M_SetBoss(ENEMY *const enemy, const int32_t index)
{
    if (index >= 0) {
        ASSERT(index < ENEMY_BOSS_COUNT);
        g_Enemies.bosses[index] = enemy;
        g_Gui.boss.present = true;
        g_Gui.boss.health = 1.0f;
        enemy->flags.boss = true;
        enemy->boss_index = (uint8_t)index;
    } else {
        g_Gui.boss.present = false;
        ASSERT(enemy->boss_index < ENEMY_BOSS_COUNT);
        g_Enemies.bosses[enemy->boss_index] = nullptr;
        enemy->flags.boss = false;
    }
}

static void M_AddOrbit(ENEMY *const enemy, const M_INSTR *const instr)
{
    ASSERT(
        instr->args[0] >= 0
        && instr->args[0] < (int32_t)ARRAY_SIZE(m_OrbitColors));
    ASSERT(enemy->orbits.count >= 0 && enemy->orbits.count < ENEMY_ORBIT_COUNT);
    EFFECT *const effect = Effect_Spawn(
        EFFECT_ORBIT, enemy->move.pos, 1, m_OrbitColors[instr->args[0]]);
    enemy->orbits.effects[enemy->orbits.count] = effect;
    effect->axis = M_ArgVec3(instr, 1);
    enemy->orbits.radius = M_ArgFloat(instr, 4);
    enemy->orbits.count++;
}

static void M_DropItems(const ENEMY *const enemy, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        VEC3 pos = enemy->move.pos;
        pos.x += Rng_F32InRange(&g_Rng, 144.0f) - 72.0f;
        pos.y += Rng_F32InRange(&g_Rng, 144.0f) - 72.0f;
        if (g_GameManager.player.power < 0x80) {
            Item_Spawn(pos, i == 0 ? ITEM_BIG_POWER : ITEM_POWER, ITEM_FALLING);
        } else {
            Item_Spawn(pos, ITEM_POINT, ITEM_FALLING);
        }
    }
}

static bool M_IsLarge(const BULLET *const bullet)
{
    return bullet->sprites.flying.sprite_data != nullptr
        && bullet->sprites.flying.sprite_data->height >= M_LARGE_HEIGHT;
}

static bool M_IsSmallStopped(const BULLET *const bullet)
{
    return bullet->sprites.flying.sprite_data != nullptr
        && bullet->sprites.flying.sprite_data->height < M_LARGE_HEIGHT
        && bullet->speed == 0.0f;
}

static bool M_IsShown(const BULLET *const bullet)
{
    return bullet->state != BULLET_FREE && bullet->state != BULLET_FADING;
}

static void M_SetColor(BULLET *const bullet, const int16_t color)
{
    bullet->color = color;
    Anm_SetSprite(
        &bullet->sprites.flying,
        bullet->sprites.flying.base_sprite + bullet->color);
}

static void M_FreezeBullets(ENEMY *const enemy, const M_INSTR *const instr)
{
    const int32_t choice = M_FuncArg(instr);
    Effect_Spawn(EFFECT_SHOCKWAVE, enemy->move.pos, 1, 0xffffffff);
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsShown(bullet)) {
            continue;
        }
        M_SetColor(bullet, 15);
        switch (choice) {
        case 0:
            bullet->speed = 0.0f;
            bullet->velocity = (VEC3) {};
            break;
        case 1:
            bullet->flags |= BULLET_FLAG_ACCELERATE;
            bullet->change.frames = 220;
            Timer_Set(&bullet->timer, 0);
            M_SinCosScale(
                &bullet->change.accel, Rng_F32InRange(&g_Rng, M_TWO_PI) - M_PI,
                0.01f);
            break;
        default:
            break;
        }
    }
}

static void M_ShootFromRandomPoint(
    ENEMY *const enemy, const M_INSTR *const instr)
{
    float range = (float)M_FuncArg(instr);
    BULLET_SHOOTER *const shooter = &enemy->shots.shooter;
    shooter->pos = Vec3_Add(enemy->move.pos, enemy->shots.offset);
    shooter->pos.x =
        Rng_F32InRange(&g_Rng, range) + enemy->move.pos.x - range / 2.0f;
    range *= 0.75f;
    shooter->pos.y =
        Rng_F32InRange(&g_Rng, range) + enemy->move.pos.y - range / 2.0f;
    Bullet_FirePattern(shooter);
}

// Every sixth frame until int_vars[2] reaches int_vars[3], fires five
// patterns from points on a star of radius float_vars[3], whose centre
// moves from the enemy towards where the player was.
static void M_StarPattern(ENEMY *const enemy)
{
    ECL_STATE *const state = &enemy->script.state;
    BULLET_SHOOTER *const shooter = &enemy->shots.shooter;
    if (state->int_vars[2] >= state->int_vars[3]) {
        state->frame_func = ECL_FUNC_NONE;
        return;
    }
    if (state->int_vars[2] == 0) {
        m_Star.from = enemy->move.pos;
        m_Star.to = g_Player.pos;
        m_Star.angles[0] = Rng_F32InRange(&g_Rng, M_TWO_PI) - M_PI;
        m_Star.angles[1] = Angle_AddNormalize(m_Star.angles[0], 2.5132742f);
    }
    if (state->int_vars[2] % 30 == 0) {
        m_Star.angles[0] = m_Star.angles[1];
        for (int32_t i = 1; i < 6; i++) {
            m_Star.angles[i] =
                Angle_AddNormalize(m_Star.angles[i - 1], 2.5132742f);
        }
    }
    if (state->int_vars[2] % 6 == 0) {
        float progress = (float)state->int_vars[2] / (float)state->int_vars[3];
        const float t = progress * 0.1f;
        VEC3 centre = Vec3_Add(
            Vec3_Scale(Vec3_Sub(m_Star.to, m_Star.from), t), m_Star.from);
        centre.z = 0.0f;
        progress += 0.5f;
        shooter->angle1 = 1.0471976f * progress;
        for (int32_t i = 0; i < 5; i++) {
            const float s = (float)(state->int_vars[2] % 30) / 30.0f;
            VEC3 from = {};
            VEC3 to = {};
            M_SinCosScale(&from, m_Star.angles[i], state->float_vars[3]);
            M_SinCosScale(&to, m_Star.angles[i + 1], state->float_vars[3]);
            from = Vec3_Add(Vec3_Scale(Vec3_Sub(to, from), s), from);
            from.z = 0.0f;
            shooter->pos = Vec3_Add(centre, from);
            const float speed = shooter->speed1;
            shooter->speed1 += Rng_F32InRange(&g_Rng, shooter->speed2);
            Bullet_FirePattern(shooter);
            shooter->speed1 = speed;
            shooter->angle1 -= 0.5235988f * progress;
        }
        Sound_Queue(SOUND_SHOT_0_QUIET);
    }
    state->int_vars[2]++;
}

static void M_GetShotTypeValues(ENEMY *const enemy)
{
    const uint8_t character = g_GameManager.run.character;
    const uint8_t shot_type = g_GameManager.run.shot_type;
    ASSERT(character < 2 && shot_type < 2);
    const int32_t *const values = m_ShotTypeValues[character][shot_type];
    enemy->script.state.int_vars[1] = values[0];
    enemy->script.state.int_vars[2] = values[1];
    enemy->script.state.int_vars[3] = values[2];
}

// Turns up to count large bullets, one in four at random, into color 5
// and sends them off: away from the player when it is near, else at a
// random angle in range from offset.
static void M_SendOff(int32_t count, const float range, const float offset)
{
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsShown(bullet)) {
            continue;
        }
        if (M_IsLarge(bullet) && bullet->color != 5
            && Rng_U16(&g_Rng) % 4 == 0) {
            M_SetColor(bullet, 5);
            const float dx = bullet->pos.x - g_Player.pos.x;
            const float dy = bullet->pos.y - g_Player.pos.y;
            if (X87_Sqrt(dy * dy + dx * dx) > 128.0f) {
                bullet->angle = Rng_F32InRange(&g_Rng, range) + offset;
            } else {
                const float turn = Rng_F32InRange(&g_Rng, M_TWO_PI);
                bullet->angle = Player_AngleTo(bullet->pos) + M_HALF_PI + turn;
            }
            M_SinCosScale(&bullet->velocity, bullet->angle, bullet->speed);
            count--;
            if (count == 0) {
                break;
            }
        }
    }
}

static void M_FreezeOrSendOff(ENEMY *const enemy, const M_INSTR *const instr)
{
    const int32_t arg = M_FuncArg(instr);
    if (arg < 2) {
        Effect_Spawn(EFFECT_SHOCKWAVE, enemy->move.pos, 1, 0xffffffff);
        g_GameManager.mode.frozen = (int8_t)arg != 0;
    } else if (g_GameManager.run.difficulty <= 1) {
        M_SendOff(14, 2.3561945f, M_QUARTER_PI);
    } else {
        M_SendOff(52, M_TWO_PI, 0.0f);
    }
    enemy->script.state.int_vars[2] = 0;
}

// Every ninth frame, fires a fan of nine bullets from an arc to one side
// of the enemy, the side alternating, on the way towards the player.
static void M_SideArcFans(ENEMY *const enemy)
{
    ECL_STATE *const state = &enemy->script.state;
    if (state->int_vars[2] % 9 == 0) {
        const int32_t count = state->int_vars[2] / 9;
        BULLET_SHOOTER shooter = {};
        shooter.type = 8;
        shooter.aim = BULLET_AIM_FAN_AT_PLAYER;
        shooter.columns = g_GameManager.run.difficulty <= 1 ? 1 : 3;
        shooter.rows = 1;
        shooter.angle2 = 0.5235988f;
        shooter.flags = 0;
        const float scale = 0.5f - (float)count * 0.5f / 9.0f;
        VEC3 to_player = Vec3_Sub(g_Player.pos, enemy->move.pos);
        VEC3 perp = Vec3_Normalize(to_player);
        const float side = (count & 1) != 0 ? -256.0f : 256.0f;
        perp = Vec3_Scale(perp, side);
        to_player = Vec3_Scale(to_player, scale);
        const VEC3 offset = Vec3_Add(to_player, perp);
        perp = (VEC3) { -perp.x, -perp.y, -perp.z };
        float c = X87_Cos(M_QUARTER_PI);
        float s = X87_Sin(M_QUARTER_PI);
        VEC3 old = perp;
        perp.x = old.x * c + old.y * s;
        perp.y = -old.x * s + old.y * c;
        c = X87_Cos(-0.17453294f);
        s = X87_Sin(-0.17453294f);
        shooter.angle1 = 0.0f;
        float angle = -M_QUARTER_PI;
        for (int32_t i = 0; i < 9; i++, angle += 0.17453294f) {
            old = perp;
            perp.x = old.x * c + old.y * s;
            perp.y = -old.x * s + old.y * c;
            shooter.pos = Vec3_Add(Vec3_Add(enemy->move.pos, perp), offset);
            shooter.speed1 = 2.0f;
            if ((count & 1) != 0 && g_GameManager.run.difficulty <= 1) {
                shooter.angle1 = angle;
            }
            shooter.color = 3;
            Bullet_FirePattern(&shooter);
        }
        Sound_Queue(SOUND_SHOT_0);
    }
    state->int_vars[2]++;
}

static void M_SpawnSpark(const VEC3 pos, const int32_t k)
{
    EFFECT *const effect = Effect_Spawn(EFFECT_SPARK, pos, 1, 0xff3030ff);
    effect->velocity.x = (Rng_F32(&g_Rng) * 40.0f - 20.0f) / 60.0f;
    effect->velocity.y = 8.0f * (float)k / 60.0f - 0.26666668f;
    effect->velocity.z = 0.0f;
    const VEC3 back = {
        -effect->velocity.x,
        -effect->velocity.y,
        -effect->velocity.z,
    };
    effect->acceleration = Vec3_Scale(back, 1.0f / 120.0f);
}

// While the enemy is shown, spawns pairs of sparks on a turning circle
// around it, more often as sparks.timer counts up. Releases the orbits of
// a hidden enemy.
static void M_SparkCircle(ENEMY *const enemy)
{
    if (enemy->flags.hidden) {
        Enemy_ReleaseOrbits(enemy);
        return;
    }
    enemy->sparks.angle += 0.017453292f;
    if (enemy->sparks.angle >= M_QUARTER_PI) {
        enemy->sparks.angle -= M_HALF_PI;
    }
    const TIMER *const timer = &enemy->sparks.timer;
    if (Timer_HasTicked(timer)
        && (timer->current > 120
            || (timer->current > 60 && timer->current % 2 == 0)
            || (timer->current > 30 && timer->current % 4 == 0)
            || timer->current % 8 == 0)) {
        int32_t k = timer->current % 16;
        k = Rng_U16InRange(&g_Rng, (uint16_t)(k / 2)) + k / 2;
        float r = (float)k * 160.0f / 16.0f + 32.0f;
        const float angle = enemy->sparks.angle - (float)k * M_PI / 40.0f;
        if (r < -M_QUARTER_PI) {
            r += M_HALF_PI;
        }
        VEC3 pos = enemy->move.pos;
        pos.x += X87_Cos(angle) * r;
        pos.y += X87_Sin(angle) * r;
        M_SpawnSpark(pos, k);
        pos = enemy->move.pos;
        pos.x -= X87_Cos(angle) * r;
        pos.y += X87_Sin(angle) * r;
        M_SpawnSpark(pos, k);
    }
    Timer_Tick(&enemy->sparks.timer);
}

// From two rings of eight points around the enemy, turning opposite ways,
// fires lasers (argument 0) or the shooter of the enemy outwards, three
// times.
static void M_TwoRingShots(ENEMY *const enemy, const M_INSTR *const instr)
{
    const int32_t mode = M_FuncArg(instr);
    BULLET_SHOOTER shooter = {};
    VEC3 at[8];
    const float origin = Rng_F32InRange(&g_Rng, M_TWO_PI);
    for (int32_t half = 0; half < 2; half++) {
        const float start = half == 0 ? -M_PI + origin : -2.7488937f + origin;
        const float step = half == 0 ? M_QUARTER_PI : -M_QUARTER_PI;
        float angle = start;
        float radius = 32.0f;
        for (int32_t i = 0; i < 8; i++) {
            at[i] = enemy->move.pos;
            at[i].x += X87_Cos(angle) * radius;
            at[i].y += X87_Sin(angle) * radius;
            angle += M_QUARTER_PI;
        }
        angle = start;
        for (int32_t k = 0; k < 3; k++) {
            radius = k < 2 ? 112.0f : 480.0f;
            for (int32_t i = 0; i < 8; i++) {
                shooter.pos = at[i];
                shooter.type = 1;
                if (mode == 0) {
                    const bool easy = g_GameManager.run.difficulty <= 1;
                    shooter.color = easy ? 2 : 8;
                    shooter.angle1 = angle;
                    shooter.speed1 = 0.0f;
                    shooter.ex_floats[0] = 0.0f;
                    shooter.ex_floats[1] = easy ? radius : 440.0f;
                    shooter.ex_floats[2] = easy ? radius : 440.0f;
                    shooter.ex_floats[3] = easy ? 28.0f : 20.0f;
                    shooter.ex_ints[0] = k * 16 + 60;
                    shooter.ex_ints[1] = 90 - k * 16;
                    shooter.ex_ints[2] = 16;
                    shooter.ex_ints[3] = 50;
                    shooter.laser_end_hit_frames = 16;
                    shooter.flags = BULLET_FLAG_SPAWN_1;
                    shooter.aim = BULLET_AIM_FAN;
                    Bullet_SpawnLaser(&shooter);
                } else {
                    enemy->shots.shooter.pos = shooter.pos;
                    Bullet_FirePattern(&enemy->shots.shooter);
                }
                at[i].x += X87_Cos(angle) * radius;
                at[i].y += X87_Sin(angle) * radius;
                angle += M_QUARTER_PI;
            }
            angle += step - M_TWO_PI;
        }
    }
}

// Fires one bullet in a random direction from every large bullet, and
// counts them in int_vars[3]. The bullets it fires are tested too.
static void M_ShootFromLargeBullets(ENEMY *const enemy)
{
    int32_t count = 0;
    BULLET_SHOOTER shooter = {};
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        const BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsShown(bullet) || !M_IsLarge(bullet)) {
            continue;
        }
        shooter.pos = bullet->pos;
        shooter.type = 3;
        shooter.color = 1;
        shooter.angle1 = Rng_F32InRange(&g_Rng, M_TWO_PI) - M_PI;
        shooter.speed1 = 0.0f;
        shooter.columns = 1;
        shooter.rows = 1;
        shooter.flags = BULLET_FLAG_SPAWN_3;
        shooter.aim = BULLET_AIM_FAN;
        Bullet_FirePattern(&shooter);
        count++;
    }
    enemy->script.state.int_vars[3] = count;
}

// Starts a small stopped bullet accelerating, in color 2.
static void M_StartStopped(BULLET *const bullet)
{
    bullet->flags |= BULLET_FLAG_ACCELERATE;
    M_SetColor(bullet, 2);
    bullet->speed = 0.01f;
    Timer_Set(&bullet->timer, 0);
    bullet->change.frames = 120;
}

static void M_SwirlStoppedBullets(ENEMY *const enemy)
{
    const float base = Rng_F32InRange(&g_Rng, M_TWO_PI) - M_PI;
    Effect_Spawn(EFFECT_SHOCKWAVE, enemy->move.pos, 1, 0xffffffff);
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsShown(bullet) || !M_IsSmallStopped(bullet)) {
            continue;
        }
        M_StartStopped(bullet);
        const float dx = enemy->move.pos.x - bullet->pos.x;
        const float dy = enemy->move.pos.y - bullet->pos.y;
        float dist = dx * dx + dy * dy;
        dist = dist > 0.1f ? X87_Sqrt(dist) : 0.0f;
        M_SinCosScale(
            &bullet->change.accel, dist * M_PI / 256.0f + base, 0.01f);
    }
}

// Is like M_SwirlStoppedBullets, each bullet accelerating in a random
// direction. It draws the base angle of the swirl all the same.
static void M_ScatterStoppedBullets(ENEMY *const enemy)
{
    Rng_F32InRange(&g_Rng, M_TWO_PI);
    Effect_Spawn(EFFECT_SHOCKWAVE, enemy->move.pos, 1, 0xffffffff);
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &g_Bullets.bullets[i];
        if (!M_IsShown(bullet) || !M_IsSmallStopped(bullet)) {
            continue;
        }
        M_StartStopped(bullet);
        M_SinCosScale(
            &bullet->change.accel, Rng_F32InRange(&g_Rng, M_TWO_PI) - M_PI,
            0.01f);
    }
}

// Runs M_SparkCircle while the enemy has life. A bomb makes the enemy
// unable to be shot until 60 frames after it ends.
static void M_SparksAndBombShield(ENEMY *const enemy)
{
    if (enemy->life <= 0) {
        return;
    }
    M_SparkCircle(enemy);
    if (g_Player.bomb.running) {
        if (enemy->turns.left >= 0) {
            Anm_StartScript(&enemy->vm, 0x1a5);
            enemy->turns.left = -1;
        }
        enemy->flags.shootable = false;
        Timer_Set(&enemy->shield_timer, 60);
    } else if (enemy->shield_timer.current > 0) {
        enemy->shield_timer.current -= 1;
        if (enemy->shield_timer.current == 0) {
            if (enemy->turns.left < 0) {
                Anm_StartScript(&enemy->vm, 0x1a0);
                enemy->turns.left = 0xa1;
            }
            enemy->flags.shootable = true;
        }
    }
}

static void M_ShootFromLasers(ENEMY *const enemy)
{
    BULLET_SHOOTER *const shooter = &enemy->shots.shooter;
    for (int32_t i = 0; i < 8; i++) {
        const LASER *const laser = enemy->shots.lasers[i];
        if (laser != nullptr && laser->active) {
            shooter->pos = (VEC3) { 64.0f, 0.0f, 0.0f };
            Angle_Rotate2D(&shooter->pos, &shooter->pos, laser->angle);
            shooter->pos = Vec3_Add(shooter->pos, enemy->move.pos);
            Bullet_FirePattern(shooter);
        }
    }
}

// Every sixth frame of int_vars[3], fires a copy of the shooter from
// points on a circle of radius float_vars[3] around the centre of the
// play area, from angle float_vars[2], each aimed outwards and turned by
// float_vars[1].
static void M_ArenaCircleShots(ENEMY *const enemy, const M_INSTR *const instr)
{
    ECL_STATE *const state = &enemy->script.state;
    BULLET_SHOOTER shooter = enemy->shots.shooter;
    const int32_t count = M_FuncArg(instr);
    float angle = state->float_vars[2];
    if (state->int_vars[3] % 6 == 0) {
        for (int32_t i = 0; i < count; i++, angle += M_TWO_PI / (float)count) {
            M_SinCosScale(&shooter.pos, angle, state->float_vars[3]);
            shooter.pos.x += 192.0f;
            shooter.pos.y += 224.0f;
            shooter.angle1 = angle + state->float_vars[1];
            Bullet_FirePattern(&shooter);
        }
    }
    state->int_vars[3]++;
}

static void M_ShootAlongLasers(ENEMY *const enemy)
{
    enemy->script.state.int_vars[3] = 0;
    for (int32_t i = 0; i < 8; i++) {
        const LASER *const laser = enemy->shots.lasers[i];
        if (laser == nullptr || !laser->active) {
            continue;
        }
        float r = laser->start_offset;
        float s;
        float c;
        X87_SinCos(laser->angle, &s, &c);
        while (laser->end_offset > r) {
            enemy->shots.shooter.pos = (VEC3) {
                c * r + laser->pos.x,
                s * r + laser->pos.y,
                0.0f,
            };
            Bullet_FirePattern(&enemy->shots.shooter);
            r += 48.0f;
        }
        enemy->script.state.int_vars[3]++;
    }
}

// Around every large bullet, starts the small stopped bullets within 64
// accelerating away from the enemy, bent towards the direction of the
// large one, and moves them to the next color. Then runs
// M_SparksAndBombShield, and counts the large bullets in int_vars[3].
static void M_BurstAroundLargeBullets(ENEMY *const enemy)
{
    int32_t count = 0;
    for (int32_t i = 0; i < BULLET_COUNT; i++) {
        const BULLET *const large = &g_Bullets.bullets[i];
        if (!M_IsShown(large) || !M_IsLarge(large)) {
            continue;
        }
        count++;
        const float large_angle = X87_Atan2(
            large->pos.y - enemy->move.pos.y, large->pos.x - enemy->move.pos.x);
        for (int32_t j = 0; j < BULLET_COUNT; j++) {
            BULLET *const small = &g_Bullets.bullets[j];
            if (!M_IsShown(small) || !M_IsSmallStopped(small)) {
                continue;
            }
            const float dx = small->pos.x - large->pos.x;
            const float dy = small->pos.y - large->pos.y;
            if (X87_Sqrt(dx * dx + dy * dy) >= 64.0f) {
                continue;
            }
            small->flags |= BULLET_FLAG_ACCELERATE;
            small->speed = 0.01f;
            Timer_Set(&small->timer, 0);
            small->change.frames = 120;
            const float angle = X87_Atan2(
                small->pos.y - enemy->move.pos.y,
                small->pos.x - enemy->move.pos.x);
            small->angle = (angle - large_angle) * 2.2f + large_angle;
            M_SinCosScale(&small->change.accel, small->angle, 0.01f);
            M_SetColor(small, (int16_t)(small->color + 1));
        }
    }
    M_SparksAndBombShield(enemy);
    enemy->script.state.int_vars[3] = count;
}

// Sets values that follow the life of the enemy, which counts as 0 from
// frame 7200 of callbacks.timer: with argument 0, float_vars[3] and
// int_vars[5]; else a random point in float_vars[2] and float_vars[3], in
// a box that widens as the life falls.
static void M_FollowLife(ENEMY *const enemy, const M_INSTR *const instr)
{
    ECL_STATE *const state = &enemy->script.state;
    int32_t life = enemy->life;
    if (enemy->callbacks.timer.current >= 7200) {
        life = 0;
    }
    if (M_FuncArg(instr) == 0) {
        state->float_vars[3] = 2.0f - (float)life * 1.0f / 6000.0f;
        state->int_vars[5] = life * 240 / 6000 + 40;
    } else {
        float range = 320.0f - (float)life * 160.0f / 6000.0f;
        state->float_vars[2] =
            192.0f - range / 2.0f + Rng_F32InRange(&g_Rng, range);
        range = 128.0f - (float)life * 64.0f / 6000.0f;
        state->float_vars[3] =
            96.0f - range / 2.0f + Rng_F32InRange(&g_Rng, range);
    }
}

// Runs pattern function func. A frame function runs with no instruction.
static void M_RunFunc(
    ENEMY *const enemy, const ECL_FUNC func, const M_INSTR *const instr)
{
    switch (func) {
    case ECL_FUNC_FREEZE_BULLETS:
        M_FreezeBullets(enemy, instr);
        break;
    case ECL_FUNC_SHOOT_FROM_RANDOM_POINT:
        M_ShootFromRandomPoint(enemy, instr);
        break;
    case ECL_FUNC_STAR_PATTERN:
        M_StarPattern(enemy);
        break;
    case ECL_FUNC_GET_SHOT_TYPE_VALUES:
        M_GetShotTypeValues(enemy);
        break;
    case ECL_FUNC_FREEZE_OR_SEND_OFF:
        M_FreezeOrSendOff(enemy, instr);
        break;
    case ECL_FUNC_SIDE_ARC_FANS:
        M_SideArcFans(enemy);
        break;
    case ECL_FUNC_SPARK_CIRCLE:
        M_SparkCircle(enemy);
        break;
    case ECL_FUNC_TWO_RING_SHOTS:
        M_TwoRingShots(enemy, instr);
        break;
    case ECL_FUNC_SHOOT_FROM_LARGE_BULLETS:
        M_ShootFromLargeBullets(enemy);
        break;
    case ECL_FUNC_SWIRL_STOPPED_BULLETS:
        M_SwirlStoppedBullets(enemy);
        break;
    case ECL_FUNC_SPARKS_AND_BOMB_SHIELD:
        M_SparksAndBombShield(enemy);
        break;
    case ECL_FUNC_SCATTER_STOPPED_BULLETS:
        M_ScatterStoppedBullets(enemy);
        break;
    case ECL_FUNC_SHOOT_FROM_LASERS:
        M_ShootFromLasers(enemy);
        break;
    case ECL_FUNC_ARENA_CIRCLE_SHOTS:
        M_ArenaCircleShots(enemy, instr);
        break;
    case ECL_FUNC_SHOOT_ALONG_LASERS:
        M_ShootAlongLasers(enemy);
        break;
    case ECL_FUNC_BURST_AROUND_LARGE_BULLETS:
        M_BurstAroundLargeBullets(enemy);
        break;
    case ECL_FUNC_FOLLOW_LIFE:
        M_FollowLife(enemy, instr);
        break;
    default:
        LOG_FATAL("pattern function %d does not exist", func);
    }
}

// Saves the state at next, when the enemy keeps a call stack, and starts
// sub. A call counts its depth only with a call stack; an interrupt
// counts it in every case. Neither counts past M_MAX_DEPTH, so a deeper
// one overwrites the last slot.
static void M_Call(
    ENEMY *const enemy, const int32_t next, const int16_t sub,
    const bool is_interrupt)
{
    ECL_STATE *const state = &enemy->script.state;
    state->pc = next;
    if (!enemy->flags.no_call_stack) {
        *M_StackSlot(enemy, enemy->script.depth) = *state;
    }
    Ecl_StartSub(state, sub);
    if ((is_interrupt || !enemy->flags.no_call_stack)
        && enemy->script.depth < M_MAX_DEPTH) {
        enemy->script.depth++;
    }
}

// Starts the pending interrupt. The state saved for the return resumes
// after the instruction at pc, so a pending interrupt skips the
// instruction that was due next.
static void M_Interrupt(ENEMY *const enemy, const int32_t pc)
{
    const int32_t index = enemy->script.pending_interrupt;
    ASSERT(index >= 0 && index < ENEMY_INTERRUPT_COUNT);
    M_Call(enemy, pc + 1, (int16_t)enemy->script.interrupts[index], true);
    enemy->script.pending_interrupt = -1;
}

static bool M_Compare(const M_OP op, const int32_t compare)
{
    switch (op) {
    case M_OP_JUMP_LESS:
    case M_OP_CALL_LESS:
        return compare < 0;
    case M_OP_JUMP_LESS_EQUAL:
    case M_OP_CALL_LESS_EQUAL:
        return compare <= 0;
    case M_OP_JUMP_EQUAL:
    case M_OP_CALL_EQUAL:
        return compare == 0;
    case M_OP_JUMP_GREATER:
    case M_OP_CALL_GREATER:
        return compare > 0;
    case M_OP_JUMP_GREATER_EQUAL:
    case M_OP_CALL_GREATER_EQUAL:
        return compare >= 0;
    default:
        return compare != 0;
    }
}

static int32_t M_CompareInts(const int32_t x, const int32_t y)
{
    return x == y ? 0 : (x < y ? -1 : 1);
}

// Runs instr, the instruction at *pc of enemy. Sets *pc to the target of
// a jump.
static M_STEP M_Step(
    ENEMY *const enemy, M_INSTR *const instr, int32_t *const pc)
{
    ECL_STATE *const state = &enemy->script.state;
    const M_OP op = (M_OP)instr->opcode;
    switch (op) {
    case M_OP_DELETE:
        return M_STEP_DELETE;
    case M_OP_LOOP: {
        int32_t value = M_Wrap((int64_t)M_Int(enemy, &instr->args[2]) - 1);
        M_Set(enemy, instr->args[2], &value);
        if (value <= 0) {
            return M_STEP_NEXT;
        }
        state->timer.current = instr->args[0];
        *pc = instr->target;
        return M_STEP_JUMP;
    }
    case M_OP_JUMP:
        state->timer.current = instr->args[0];
        *pc = instr->target;
        return M_STEP_JUMP;
    case M_OP_SET_INT:
    case M_OP_SET_FLOAT:
        M_Set(enemy, instr->args[0], &instr->args[1]);
        break;
    case M_OP_NORMALIZE_ANGLE: {
        float angle = M_LoadFloat(M_Resolve(enemy, &instr->args[0], nullptr));
        angle = Angle_AddNormalize(angle, 0.0f);
        M_Set(enemy, instr->args[0], &angle);
        break;
    }
    case M_OP_RANDOM_INT: {
        const uint32_t range = (uint32_t)M_Int(enemy, &instr->args[1]);
        int32_t value = (int32_t)Rng_U32InRange(&g_Rng, range);
        M_Set(enemy, instr->args[0], &value);
        break;
    }
    case M_OP_RANDOM_INT_FROM: {
        const uint32_t range = (uint32_t)M_Int(enemy, &instr->args[1]);
        const int32_t low = M_Int(enemy, &instr->args[2]);
        int32_t value = (int32_t)Rng_U32InRange(&g_Rng, range);
        value = M_Wrap((int64_t)value + low);
        M_Set(enemy, instr->args[0], &value);
        break;
    }
    case M_OP_RANDOM_FLOAT: {
        const float range = M_Float(enemy, &instr->args[1]);
        float value = Rng_F32InRange(&g_Rng, range);
        M_Set(enemy, instr->args[0], &value);
        break;
    }
    case M_OP_RANDOM_FLOAT_FROM: {
        const float range = M_Float(enemy, &instr->args[1]);
        const float low = M_Float(enemy, &instr->args[2]);
        float value = Rng_F32InRange(&g_Rng, range);
        value += low;
        M_Set(enemy, instr->args[0], &value);
        break;
    }
    case M_OP_GET_X:
        M_Set(enemy, instr->args[0], &enemy->move.pos.x);
        break;
    case M_OP_GET_Y:
        M_Set(enemy, instr->args[0], &enemy->move.pos.y);
        break;
    case M_OP_GET_Z:
        M_Set(enemy, instr->args[0], &enemy->move.pos.z);
        break;
    case M_OP_ADD_INT:
    case M_OP_SUB_INT:
    case M_OP_MUL_INT:
    case M_OP_DIV_INT:
    case M_OP_MOD_INT:
    case M_OP_ADD_FLOAT:
    case M_OP_SUB_FLOAT:
    case M_OP_MUL_FLOAT:
    case M_OP_DIV_FLOAT:
    case M_OP_MOD_FLOAT:
        M_Arith(enemy, op, instr->args[0], &instr->args[1], &instr->args[2]);
        break;
    // Write the int bits of the operand whatever it names: a float, a
    // read-only value, or the argument of the instruction itself.
    case M_OP_INCREMENT:
    case M_OP_DECREMENT: {
        void *const ref = M_Resolve(enemy, &instr->args[0], nullptr);
        const int64_t step = op == M_OP_INCREMENT ? 1 : -1;
        M_StoreInt(ref, M_Wrap(M_LoadInt(ref) + step));
        break;
    }
    case M_OP_ANGLE:
        M_Angle(enemy, instr);
        break;
    case M_OP_COMPARE_INT: {
        const int32_t x = M_Int(enemy, &instr->args[0]);
        const int32_t y = M_Int(enemy, &instr->args[1]);
        state->compare = M_CompareInts(x, y);
        break;
    }
    case M_OP_COMPARE_FLOAT: {
        const float a = M_Float(enemy, &instr->args[0]);
        const float b = M_Float(enemy, &instr->args[1]);
        state->compare = a == b ? 0 : (a < b ? -1 : 1);
        break;
    }
    case M_OP_JUMP_LESS:
    case M_OP_JUMP_LESS_EQUAL:
    case M_OP_JUMP_EQUAL:
    case M_OP_JUMP_GREATER:
    case M_OP_JUMP_GREATER_EQUAL:
    case M_OP_JUMP_NOT_EQUAL:
        if (!M_Compare(op, state->compare)) {
            break;
        }
        state->timer.current = instr->args[0];
        *pc = instr->target;
        return M_STEP_JUMP;
    case M_OP_CALL_LESS:
    case M_OP_CALL_LESS_EQUAL:
    case M_OP_CALL_EQUAL:
    case M_OP_CALL_GREATER:
    case M_OP_CALL_GREATER_EQUAL:
    case M_OP_CALL_NOT_EQUAL:
        if (!M_Compare(
                op,
                M_CompareInts(M_Int(enemy, &instr->args[3]), instr->args[4]))) {
            break;
        }
        [[fallthrough]];
    case M_OP_CALL:
        M_Call(enemy, *pc + 1, (int16_t)instr->args[0], false);
        state->int_vars[0] = instr->args[1];
        state->float_vars[0] = M_ArgFloat(instr, 2);
        return M_STEP_RESTART;
    // Pops the stack also without a call stack, which then restores what
    // an earlier call saved.
    case M_OP_RETURN:
        enemy->script.depth--;
        *state = *M_StackSlot(enemy, enemy->script.depth);
        return M_STEP_RESTART;
    case M_OP_START_SCRIPT:
        Anm_StartScript(&enemy->vm, instr->args[0] + M_FIRST_SCRIPT);
        break;
    case M_OP_START_SPRITE_SCRIPT:
        ASSERT(instr->args[0] >= 0 && instr->args[0] < ENEMY_SPRITE_COUNT);
        Anm_StartScript(
            &enemy->vms[instr->args[0]], instr->args[1] + M_FIRST_SCRIPT);
        break;
    // Resolves each component in place, so a component that names a
    // component of the position reads the value just copied into it.
    case M_OP_SET_POS: {
        const VEC3 previous = enemy->move.pos;
        enemy->move.pos = M_ArgVec3(instr, 0);
        enemy->move.pos.x = M_Float(enemy, &enemy->move.pos.x);
        enemy->move.pos.y = M_Float(enemy, &enemy->move.pos.y);
        enemy->move.pos.z = M_Float(enemy, &enemy->move.pos.z);
        Enemy_ClampPos(enemy);
        if (enemy->move.pos.x != previous.x || enemy->move.pos.y != previous.y
            || enemy->move.pos.z != previous.z) {
            Enemy_RenewPresentation(enemy);
        }
        break;
    }
    case M_OP_SET_VELOCITY:
        enemy->move.velocity = M_ArgVec3(instr, 0);
        enemy->move.velocity.x = M_Float(enemy, &enemy->move.velocity.x);
        enemy->move.velocity.y = M_Float(enemy, &enemy->move.velocity.y);
        enemy->move.velocity.z = M_Float(enemy, &enemy->move.velocity.z);
        enemy->flags.move = ENEMY_MOVE_VELOCITY;
        break;
    case M_OP_SET_ANGLE_SPEED:
        enemy->move.angle = M_Float(enemy, &instr->args[0]);
        enemy->move.speed = M_Float(enemy, &instr->args[1]);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case M_OP_SET_ANGULAR_VELOCITY:
        enemy->move.angular_velocity = M_Float(enemy, &instr->args[0]);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case M_OP_AIM_AT_PLAYER:
        enemy->move.angle =
            Player_AngleFrom(enemy->move.pos) + M_ArgFloat(instr, 0);
        enemy->move.speed = M_Float(enemy, &instr->args[1]);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case M_OP_SET_SPEED:
        enemy->move.speed = M_Float(enemy, &instr->args[0]);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case M_OP_SET_ACCELERATION:
        enemy->move.acceleration = M_Float(enemy, &instr->args[0]);
        enemy->flags.move = ENEMY_MOVE_ANGLE;
        break;
    case M_OP_SET_SHOT_EXTRAS:
        for (int32_t i = 0; i < 4; i++) {
            enemy->shots.shooter.ex_ints[i] = M_Int(enemy, &instr->args[i]);
        }
        for (int32_t i = 0; i < 4; i++) {
            enemy->shots.shooter.ex_floats[i] =
                M_Float(enemy, &instr->args[4 + i]);
        }
        break;
    case M_OP_SET_DEATH_EFFECTS:
        enemy->death.effect = M_ArgByte(instr, 0);
        enemy->death.particle_color = M_ArgByte(instr, 1);
        break;
    case M_OP_SET_INTERVAL:
        enemy->shots.interval = instr->args[0];
        enemy->shots.interval +=
            M_RankLerp(enemy->shots.interval / 5, -enemy->shots.interval / 5);
        Timer_Set(&enemy->shots.timer, 0);
        break;
    case M_OP_SET_INTERVAL_RANDOM:
        enemy->shots.interval = instr->args[0];
        enemy->shots.interval +=
            M_RankLerp(enemy->shots.interval / 5, -enemy->shots.interval / 5);
        if (enemy->shots.interval != 0) {
            Timer_Set(
                &enemy->shots.timer,
                (int32_t)Rng_U32InRange(
                    &g_Rng, (uint32_t)enemy->shots.interval));
        }
        break;
    case M_OP_HOLD_FIRE:
        enemy->flags.hold_fire = true;
        break;
    case M_OP_RELEASE_FIRE:
        enemy->flags.hold_fire = false;
        break;
    case M_OP_FIRE_AGAIN:
        enemy->shots.shooter.pos =
            Vec3_Add(enemy->move.pos, enemy->shots.offset);
        Bullet_FirePattern(&enemy->shots.shooter);
        break;
    case M_OP_SET_SHOT_OFFSET:
        enemy->shots.offset.x = M_Float(enemy, &instr->args[0]);
        enemy->shots.offset.y = M_Float(enemy, &instr->args[1]);
        enemy->shots.offset.z = M_Float(enemy, &instr->args[2]);
        break;
    case M_OP_LASER:
    case M_OP_LASER_AT_PLAYER:
        M_Laser(enemy, instr);
        break;
    case M_OP_SET_LASER_INDEX:
        enemy->shots.laser_index = M_Int(enemy, &instr->args[0]);
        break;
    case M_OP_TURN_LASER: {
        LASER *const laser = M_GetLaser(enemy, instr->args[0]);
        if (laser != nullptr) {
            laser->angle = laser->angle + M_Float(enemy, &instr->args[1]);
        }
        break;
    }
    case M_OP_AIM_LASER: {
        LASER *const laser = M_GetLaser(enemy, instr->args[0]);
        if (laser != nullptr) {
            laser->angle =
                Player_AngleFrom(laser->pos) + M_Float(enemy, &instr->args[1]);
        }
        break;
    }
    case M_OP_MOVE_LASER: {
        LASER *const laser = M_GetLaser(enemy, instr->args[0]);
        if (laser != nullptr) {
            laser->pos = Vec3_Add(enemy->move.pos, M_ArgVec3(instr, 1));
        }
        break;
    }
    case M_OP_TEST_LASER: {
        const LASER *const laser = M_GetLaser(enemy, instr->args[0]);
        state->compare = laser != nullptr && laser->active ? 0 : 1;
        break;
    }
    case M_OP_END_LASER: {
        LASER *const laser = M_GetLaser(enemy, instr->args[0]);
        if (laser != nullptr && laser->active && laser->state < LASER_ENDING) {
            laser->state = LASER_ENDING;
            Timer_Set(&laser->timer, 0);
        }
        break;
    }
    case M_OP_FORGET_LASERS:
        for (int32_t i = 0; i < ENEMY_LASER_COUNT; i++) {
            enemy->shots.lasers[i] = nullptr;
        }
        break;
    case M_OP_SET_BOSS:
        M_SetBoss(enemy, instr->args[0]);
        break;
    case M_OP_ADD_ORBIT:
        M_AddOrbit(enemy, instr);
        break;
    case M_OP_MOVE_ANGLE_OUT:
    case M_OP_MOVE_ANGLE_OUT4:
    case M_OP_MOVE_ANGLE_IN:
    case M_OP_MOVE_ANGLE_IN4:
        M_MoveAngle(enemy, instr);
        enemy->flags.ease = (ENEMY_EASE)(op - M_OP_MOVE_ANGLE_OUT + 1);
        break;
    case M_OP_MOVE_TO_LINEAR:
    case M_OP_MOVE_TO_OUT:
    case M_OP_MOVE_TO_OUT4:
    case M_OP_MOVE_TO_IN:
    case M_OP_MOVE_TO_IN4:
        M_MoveTo(enemy, instr);
        enemy->flags.ease = (ENEMY_EASE)(op - M_OP_MOVE_TO_LINEAR);
        break;
    case M_OP_MOVE_TIME_OUT:
    case M_OP_MOVE_TIME_OUT4:
    case M_OP_MOVE_TIME_IN:
    case M_OP_MOVE_TIME_IN4:
        M_MoveTime(enemy, instr);
        enemy->flags.ease = (ENEMY_EASE)(op - M_OP_MOVE_TIME_OUT + 1);
        break;
    case M_OP_SET_BOUNDS:
        enemy->move.bounds_min =
            (VEC2) { M_ArgFloat(instr, 0), M_ArgFloat(instr, 1) };
        enemy->move.bounds_max =
            (VEC2) { M_ArgFloat(instr, 2), M_ArgFloat(instr, 3) };
        enemy->flags.clamp_pos = true;
        break;
    case M_OP_CLEAR_BOUNDS:
        enemy->flags.clamp_pos = false;
        break;
    case M_OP_RANDOM_ANGLE: {
        const float low = M_ArgFloat(instr, 0);
        const float high = M_ArgFloat(instr, 1);
        enemy->move.angle = Rng_F32InRange(&g_Rng, high - low) + low;
        break;
    }
    case M_OP_RANDOM_ANGLE_IN_BOUNDS:
        M_RandomAngleInBounds(enemy, instr);
        break;
    case M_OP_SET_TURNS:
        enemy->turns.still = M_ArgShort(instr, 0);
        enemy->turns.still_from_left = M_ArgShort(instr, 1);
        enemy->turns.still_from_right = M_ArgShort(instr, 2);
        enemy->turns.left = M_ArgShort(instr, 3);
        enemy->turns.right = M_ArgShort(instr, 4);
        enemy->flags.facing = ENEMY_FACING_UNSET;
        break;
    case M_OP_SET_HITBOX:
        enemy->hitbox_size = M_ArgVec3(instr, 0);
        break;
    case M_OP_SET_CONTACT_DAMAGE:
        enemy->flags.contact_damage = (instr->args[0] & 1) != 0;
        break;
    case M_OP_SET_TAKES_DAMAGE:
        enemy->flags.takes_damage = (instr->args[0] & 1) != 0;
        break;
    case M_OP_PLAY_SOUND:
        Sound_Queue(instr->args[0]);
        break;
    case M_OP_SET_DEATH:
        enemy->flags.death = (ENEMY_DEATH)(instr->args[0] & 7);
        break;
    case M_OP_SET_DEATH_SUB:
        enemy->script.death_sub = instr->args[0];
        break;
    case M_OP_SET_INTERRUPT:
        ASSERT(instr->args[1] >= 0 && instr->args[1] < ENEMY_INTERRUPT_COUNT);
        enemy->script.interrupts[instr->args[1]] = instr->args[0];
        break;
    case M_OP_INTERRUPT:
        enemy->script.pending_interrupt = instr->args[0];
        M_Interrupt(enemy, *pc);
        return M_STEP_RESTART;
    case M_OP_SET_LIFE:
        enemy->max_life = instr->args[0];
        enemy->life = enemy->max_life;
        break;
    case M_OP_START_SPELL_CARD:
        M_StartSpellCard(enemy, instr);
        break;
    case M_OP_END_SPELL_CARD:
        M_EndSpellCard();
        break;
    case M_OP_SET_CALLBACK_TIMER:
        Timer_Set(&enemy->callbacks.timer, instr->args[0]);
        break;
    case M_OP_SET_LIFE_CALLBACK:
        enemy->callbacks.life = instr->args[0];
        break;
    case M_OP_SET_LIFE_CALLBACK_SUB:
        enemy->callbacks.life_sub = instr->args[0];
        break;
    case M_OP_SET_TIME_CALLBACK:
        enemy->callbacks.time = instr->args[0];
        Timer_Set(&enemy->callbacks.timer, 0);
        break;
    case M_OP_SET_TIME_CALLBACK_SUB:
        enemy->callbacks.time_sub = instr->args[0];
        break;
    case M_OP_SET_SHOOTABLE:
        enemy->flags.shootable = (instr->args[0] & 1) != 0;
        break;
    case M_OP_SPAWN_EFFECT:
        Effect_Spawn(
            (EFFECT_TYPE)instr->args[0], enemy->move.pos, instr->args[1],
            (uint32_t)instr->args[2]);
        break;
    case M_OP_DROP_ITEMS:
        M_DropItems(enemy, instr->args[0]);
        break;
    case M_OP_SET_ROTATE_SPRITE:
        enemy->flags.rotate_sprite = (instr->args[0] & 1) != 0;
        break;
    case M_OP_CALL_FUNC:
        M_RunFunc(enemy, (ECL_FUNC)instr->args[0], instr);
        break;
    case M_OP_SET_FRAME_FUNC:
        state->frame_func =
            instr->args[0] >= 0 ? (ECL_FUNC)instr->args[0] : ECL_FUNC_NONE;
        break;
    case M_OP_ADD_TIME:
        state->timer.current = M_Wrap(
            (int64_t)state->timer.current + M_Int(enemy, &instr->args[0]));
        break;
    case M_OP_DROP_ITEM:
        Item_Spawn(enemy->move.pos, (ITEM_TYPE)instr->args[0], ITEM_FALLING);
        break;
    case M_OP_RESUME_STAGE:
        g_Stage.script.resume = true;
        break;
    case M_OP_SET_BOSS_LIVES:
        g_Gui.boss.lives = instr->args[0];
        g_GameManager.run.progress_frames += 1800;
        break;
    case M_OP_SPAWN:
        M_Spawn(enemy, instr);
        break;
    case M_OP_KILL_ALL:
        Enemy_KillAll();
        break;
    case M_OP_INTERRUPT_SCRIPT:
        enemy->vm.pending_interrupt = M_ArgShort(instr, 0);
        break;
    case M_OP_INTERRUPT_SPRITE_SCRIPT:
        ASSERT(instr->args[0] >= 0 && instr->args[0] < ENEMY_SPRITE_COUNT);
        enemy->vms[instr->args[0]].pending_interrupt = M_ArgShort(instr, 2);
        break;
    case M_OP_CLEAR_BULLETS:
        Bullet_ClearAllToItems();
        break;
    case M_OP_SET_SHOT_SOUND:
        if (instr->args[0] >= 0) {
            enemy->shots.shooter.sound = instr->args[0];
            enemy->shots.shooter.flags |= BULLET_FLAG_SOUND;
        } else {
            enemy->shots.shooter.flags &= ~BULLET_FLAG_SOUND;
        }
        break;
    case M_OP_SET_NO_CALL_STACK:
        enemy->flags.no_call_stack = (instr->args[0] & 1) != 0;
        break;
    case M_OP_SET_RANK_GROWTH:
        enemy->shots.rank_speed_low = M_ArgFloat(instr, 0);
        enemy->shots.rank_speed_high = M_ArgFloat(instr, 1);
        enemy->shots.rank_columns_low = (int16_t)instr->args[2];
        enemy->shots.rank_columns_high = (int16_t)instr->args[3];
        enemy->shots.rank_rows_low = (int16_t)instr->args[4];
        enemy->shots.rank_rows_high = (int16_t)instr->args[5];
        break;
    case M_OP_SET_HIDDEN:
        enemy->flags.hidden = (instr->args[0] & 1) != 0;
        break;
    case M_OP_DEATH_SUB_ON_TIMER:
        enemy->callbacks.time_sub = enemy->script.death_sub;
        Timer_Set(&enemy->callbacks.timer, 0);
        break;
    case M_OP_SET_KEEP_SPELL_CARD:
        enemy->flags.keep_spell_card = (instr->args[0] & 1) != 0;
        break;
    default:
        if (op >= M_OP_FIRE_FIRST && op <= M_OP_FIRE_LAST) {
            M_Fire(enemy, instr);
        }
        break;
    }
    return M_STEP_NEXT;
}

static void M_Move(ENEMY *const enemy)
{
    switch (enemy->flags.move) {
    case ENEMY_MOVE_ANGLE:
        enemy->move.angle =
            Angle_AddNormalize(enemy->move.angle, enemy->move.angular_velocity);
        enemy->move.speed = enemy->move.acceleration + enemy->move.speed;
        M_SinCosScale(
            &enemy->move.velocity, enemy->move.angle, enemy->move.speed);
        enemy->move.velocity.z = 0.0f;
        break;
    case ENEMY_MOVE_TO: {
        enemy->move.timer.current -= 1;
        float t =
            (float)enemy->move.timer.current / (float)enemy->move.duration;
        if (t >= 1.0f) {
            t = 1.0f;
        }
        switch (enemy->flags.ease) {
        case ENEMY_EASE_LINEAR:
            t = 1.0f - t;
            break;
        case ENEMY_EASE_OUT:
            t = 1.0f - t * t;
            break;
        case ENEMY_EASE_OUT4:
            t = 1.0f - t * t * t * t;
            break;
        case ENEMY_EASE_IN:
            t = 1.0f - t;
            t = t * t;
            break;
        case ENEMY_EASE_IN4:
            t = 1.0f - t;
            t = t * t * t * t;
            break;
        }
        enemy->move.velocity = Vec3_Sub(
            Vec3_Add(Vec3_Scale(enemy->move.delta, t), enemy->move.start),
            enemy->move.pos);
        enemy->move.angle =
            X87_Atan2(enemy->move.velocity.y, enemy->move.velocity.x);
        if (enemy->move.timer.current <= 0) {
            enemy->flags.move = ENEMY_MOVE_VELOCITY;
            enemy->move.pos = Vec3_Add(enemy->move.start, enemy->move.delta);
            enemy->move.velocity = (VEC3) {};
        }
        break;
    }
    default:
        break;
    }
}

static void M_Turn(ENEMY *const enemy)
{
    uint8_t facing = ENEMY_FACING_STILL;
    if (enemy->move.velocity.x < 0.0f) {
        facing = ENEMY_FACING_LEFT;
    } else if (enemy->move.velocity.x > 0.0f) {
        facing = ENEMY_FACING_RIGHT;
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

static M_INSTR *M_GetInstr(const ECL_STATE *const state, const int32_t pc)
{
    ASSERT(M_IsSub(state->sub));
    const M_SUB *const sub = &m_Subs[state->sub];
    ASSERT(pc >= 0 && pc < sub->count);
    return &sub->instrs[pc];
}

static void M_FreeSubs(void)
{
    for (int32_t i = 0; i < m_SubCount; i++) {
        for (int32_t j = 0; j < m_Subs[i].count; j++) {
            Mem_Free(m_Subs[i].instrs[j].text);
        }
        Mem_Free(m_Subs[i].instrs);
    }
    Mem_Free(m_Subs);
    m_Subs = nullptr;
    m_SubCount = 0;
}

static RESULT M_ReadText(
    READER *const reader, const size_t size, M_INSTR *const instr)
{
    char *const bytes = Mem_Alloc(size);
    Reader_ReadBytes(reader, bytes, size);
    const char *const end = memchr(bytes, '\0', size);
    RESULT result = OK;
    if (end == nullptr) {
        result = FAIL("the spell card name runs past its instruction");
    } else {
        result = Cp932_Decode(bytes, (size_t)(end - bytes), &instr->text);
    }
    Mem_Free(bytes);
    return result;
}

// Reads the instruction at the position into instr, and moves to the
// next one.
static RESULT M_ReadInstr(READER *const reader, M_INSTR *const instr)
{
    const size_t start = Reader_Tell(reader);
    instr->time = Reader_ReadI32(reader);
    if (instr->time == M_END_TIME) {
        instr->opcode = -1;
        return OK;
    }
    instr->opcode = Reader_ReadI16(reader);
    const int16_t size = Reader_ReadI16(reader);
    Reader_Skip(reader, 1);
    instr->difficulty_mask = Reader_ReadU8(reader);
    Reader_Skip(reader, 2);
    FAIL_IF(
        Reader_Failed(reader) || size < M_INSTR_HEADER_SIZE
            || (size_t)size > Reader_Size(reader) - start,
        "the instruction at 0x%zx runs past the file", start);
    const int32_t words =
        MIN((size - M_INSTR_HEADER_SIZE) / 4, (int32_t)M_ARG_COUNT);
    for (int32_t i = 0; i < words; i++) {
        instr->args[i] = Reader_ReadI32(reader);
    }
    if (instr->opcode == M_OP_START_SPELL_CARD) {
        const size_t offset = M_INSTR_HEADER_SIZE + M_SPELL_NAME_OFFSET;
        FAIL_IF(
            (size_t)size <= offset, "the spell card at 0x%zx has no name",
            start);
        Reader_Seek(reader, start + offset);
        MUST(M_ReadText(reader, (size_t)size - offset, instr));
    }
    Reader_Seek(reader, start + (size_t)size);
    return OK;
}

static bool M_IsJump(const int16_t opcode)
{
    return opcode == M_OP_JUMP || opcode == M_OP_LOOP
        || (opcode >= M_OP_JUMP_LESS && opcode <= M_OP_JUMP_NOT_EQUAL);
}

// Parses the subroutine at offset up to its end, and turns the byte
// offset of each jump into an instruction index.
static RESULT M_ParseSub(
    READER *const reader, const size_t offset, M_SUB *const sub)
{
    Reader_Seek(reader, offset);
    int32_t count = 0;
    M_INSTR instr;
    do {
        instr = (M_INSTR) {};
        const RESULT result = M_ReadInstr(reader, &instr);
        Mem_Free(instr.text);
        MUST(result);
        FAIL_IF(
            Reader_Failed(reader), "instruction %d runs past the file", count);
        count++;
    } while (instr.time != M_END_TIME);

    sub->instrs = Mem_AllocArrZ((size_t)count, sizeof(M_INSTR));
    sub->count = count;
    int64_t *const offsets = Mem_AllocArr((size_t)count, sizeof(int64_t));
    Reader_Seek(reader, offset);
    RESULT result = OK;
    for (int32_t i = 0; i < count && IS_OK(result); i++) {
        offsets[i] = (int64_t)(Reader_Tell(reader) - offset);
        result = M_ReadInstr(reader, &sub->instrs[i]);
    }
    for (int32_t i = 0; i < count && IS_OK(result); i++) {
        M_INSTR *const jump = &sub->instrs[i];
        if (!M_IsJump(jump->opcode)) {
            continue;
        }
        const int64_t target = offsets[i] + jump->args[1];
        jump->target = 0;
        while (jump->target < count && offsets[jump->target] != target) {
            jump->target++;
        }
        if (jump->target == count) {
            result = FAIL(
                "instruction %d jumps to %lld, which is not an instruction", i,
                (long long)target);
        }
    }
    Mem_Free(offsets);
    return result;
}

// Returns whether value, a subroutine that may be negative for none,
// names a subroutine of the file.
static bool M_IsSubOrNone(const int32_t value)
{
    return value < 0 || M_IsSub((int16_t)value);
}

static RESULT M_CheckSubs(void)
{
    for (int32_t i = 0; i < m_SubCount; i++) {
        for (int32_t j = 0; j < m_Subs[i].count; j++) {
            const M_INSTR *const instr = &m_Subs[i].instrs[j];
            bool valid = true;
            switch (instr->opcode) {
            case M_OP_CALL:
            case M_OP_CALL_LESS:
            case M_OP_CALL_LESS_EQUAL:
            case M_OP_CALL_EQUAL:
            case M_OP_CALL_GREATER:
            case M_OP_CALL_GREATER_EQUAL:
            case M_OP_CALL_NOT_EQUAL:
            case M_OP_SPAWN:
                valid = M_IsSub((int16_t)instr->args[0]);
                break;
            case M_OP_SET_DEATH_SUB:
            case M_OP_SET_INTERRUPT:
            case M_OP_SET_LIFE_CALLBACK_SUB:
            case M_OP_SET_TIME_CALLBACK_SUB:
                valid = M_IsSubOrNone(instr->args[0]);
                break;
            default:
                break;
            }
            FAIL_IF(
                !valid, "instruction %d of subroutine %d names subroutine %d",
                j, i, instr->args[0]);
        }
    }
    return OK;
}

// Reads the timeline entry at the position into entry, and moves to the
// next one. An entry holds only the argument bytes that its size gives it;
// the others are zero.
static RESULT M_ReadTimelineEntry(
    READER *const reader, ECL_TIMELINE_ENTRY *const entry)
{
    const size_t start = Reader_Tell(reader);
    entry->time = Reader_ReadI16(reader);
    FAIL_IF(Reader_Failed(reader), "the timeline runs past the file");
    if (entry->time < 0) {
        return OK;
    }
    entry->arg = Reader_ReadI16(reader);
    entry->opcode = Reader_ReadI16(reader);
    const int16_t size = Reader_ReadI16(reader);
    FAIL_IF(
        Reader_Failed(reader) || size < M_TIMELINE_HEADER_SIZE
            || (size_t)size > Reader_Size(reader) - start,
        "the timeline entry at 0x%zx runs past the file", start);
    uint8_t bytes[M_TIMELINE_ARGS_SIZE] = {};
    Reader_ReadBytes(
        reader, bytes,
        MIN((size_t)size - M_TIMELINE_HEADER_SIZE, sizeof(bytes)));
    READER args;
    Reader_OpenBuffer(&args, bytes, sizeof(bytes));
    entry->args.spawn.pos.x = Reader_ReadF32(&args);
    entry->args.spawn.pos.y = Reader_ReadF32(&args);
    entry->args.spawn.pos.z = Reader_ReadF32(&args);
    entry->args.spawn.life = Reader_ReadI16(&args);
    entry->args.spawn.item = Reader_ReadI16(&args);
    entry->args.spawn.score = Reader_ReadI32(&args);
    if (entry->opcode == ECL_TIMELINE_INTERRUPT_BOSS) {
        Reader_Seek(&args, 0);
        entry->args.interrupt.boss = Reader_ReadI32(&args);
        entry->args.interrupt.interrupt = Reader_ReadI32(&args);
    }
    FAIL_IF(
        entry->opcode >= ECL_TIMELINE_SPAWN
            && entry->opcode <= ECL_TIMELINE_SPAWN_RANDOM_MIRRORED_DEFAULT
            && !M_IsSub(entry->arg),
        "the timeline entry at 0x%zx spawns subroutine %d", start, entry->arg);
    Reader_Seek(reader, start + (size_t)size);
    return OK;
}

static RESULT M_ParseTimeline(READER *const reader, const size_t offset)
{
    Reader_Seek(reader, offset);
    int32_t count = 0;
    ECL_TIMELINE_ENTRY entry;
    do {
        entry = (ECL_TIMELINE_ENTRY) {};
        MUST(M_ReadTimelineEntry(reader, &entry));
        count++;
    } while (entry.time >= 0);
    m_Timeline = Mem_AllocArrZ((size_t)count, sizeof(ECL_TIMELINE_ENTRY));
    Reader_Seek(reader, offset);
    for (int32_t i = 0; i < count; i++) {
        MUST(M_ReadTimelineEntry(reader, &m_Timeline[i]));
    }
    return OK;
}

static RESULT M_Parse(READER *const reader)
{
    const int16_t sub_count = Reader_ReadI16(reader);
    Reader_Skip(reader, 2);
    const int32_t timeline = Reader_ReadI32(reader);
    FAIL_IF(
        Reader_Failed(reader) || sub_count < 0 || timeline < 0, "bad header");
    m_Subs = Mem_AllocArrZ((size_t)sub_count, sizeof(M_SUB));
    m_SubCount = sub_count;
    for (int32_t i = 0; i < sub_count; i++) {
        Reader_Seek(reader, M_HEADER_SIZE + 4 * (size_t)i);
        const int32_t offset = Reader_ReadI32(reader);
        FAIL_IF(
            Reader_Failed(reader) || offset < 0,
            "the subroutine table runs past the file");
        MUST(
            M_ParseSub(reader, (size_t)offset, &m_Subs[i]), "subroutine %d", i);
    }
    MUST(M_CheckSubs());
    return M_ParseTimeline(reader, (size_t)timeline);
}

RESULT Ecl_Load(void)
{
    Ecl_Release();
    const int32_t stage = g_GameManager.run.stage;
    ASSERT(stage >= 0 && stage < (int32_t)ARRAY_SIZE(m_Paths));
    const char *const path = m_Paths[stage];
    READER reader;
    MUST(Files_Load(path, &reader), "%s", path);
    const RESULT result = M_Parse(&reader);
    Reader_Close(&reader);
    if (!IS_OK(result)) {
        Ecl_Release();
    }
    return Result_Prefix(result, "%s", path);
}

void Ecl_Release(void)
{
    M_FreeSubs();
    Mem_Free(m_Timeline);
    m_Timeline = nullptr;
}

const ECL_TIMELINE_ENTRY *Ecl_GetTimeline(void)
{
    ASSERT(m_Timeline != nullptr);
    return m_Timeline;
}

void Ecl_StartSub(ECL_STATE *const state, const int16_t sub)
{
    ASSERT(M_IsSub(sub));
    state->pc = 0;
    Timer_Set(&state->timer, 0);
    state->sub = sub;
}

ECL_RUN Ecl_RunEnemy(ENEMY *const enemy)
{
    ECL_STATE *const state = &enemy->script.state;
    int32_t pc;
restart:
    pc = state->pc;
    if (enemy->script.pending_interrupt >= 0) {
        M_Interrupt(enemy, pc);
        goto restart;
    }
    for (;;) {
        M_INSTR *const instr = M_GetInstr(state, pc);
        if (state->timer.current != instr->time) {
            break;
        }
        ASSERT(instr->time != M_END_TIME);
        const uint32_t difficulty = (uint32_t)1
            << ((uint32_t)g_GameManager.run.difficulty & 31);
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
            Timer_Tick(&enemy->shots.timer);
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
            M_RunFunc(enemy, state->frame_func, nullptr);
        }
    }
    state->pc = pc;
    Timer_Tick(&state->timer);
    return ECL_RUN_CONTINUE;
}
