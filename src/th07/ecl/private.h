#pragma once

#include <stdint.h>

#define ECL_ARG_COUNT 16
#define ECL_SPELL_CARD_COUNT 141

// Selects what an instruction does. Opcodes 0, 127, 141 and those above
// 161 do nothing.
typedef enum {
    ECL_OP_DELETE = 1,
    ECL_OP_JUMP = 2,
    ECL_OP_LOOP = 3, // decrements a counter and jumps while it is above 0
    ECL_OP_SET_INT = 4,
    ECL_OP_SET_FLOAT = 5,
    ECL_OP_RANDOM_INT = 6,
    ECL_OP_RANDOM_INT_FROM = 7,
    ECL_OP_RANDOM_FLOAT = 8,
    ECL_OP_RANDOM_FLOAT_FROM = 9,
    ECL_OP_RANDOM_SIGN_INT = 10,
    ECL_OP_RANDOM_SIGN_FLOAT = 11,
    ECL_OP_ADD_INT = 12,
    ECL_OP_SUB_INT = 13,
    ECL_OP_MUL_INT = 14,
    ECL_OP_DIV_INT = 15,
    ECL_OP_MOD_INT = 16,
    ECL_OP_INCREMENT = 17,
    ECL_OP_DECREMENT = 18,
    ECL_OP_ADD_FLOAT = 19,
    ECL_OP_SUB_FLOAT = 20,
    ECL_OP_MUL_FLOAT = 21,
    ECL_OP_DIV_FLOAT = 22,
    ECL_OP_MOD_FLOAT = 23,
    ECL_OP_SIN = 24,
    ECL_OP_COS = 25,
    ECL_OP_ANGLE = 26,
    ECL_OP_INTERPOLATE = 27,
    ECL_OP_JUMP_EQUAL_INT = 28,
    ECL_OP_JUMP_EQUAL_FLOAT = 29,
    ECL_OP_JUMP_NOT_EQUAL_INT = 30,
    ECL_OP_JUMP_NOT_EQUAL_FLOAT = 31,
    ECL_OP_JUMP_LESS_INT = 32,
    ECL_OP_JUMP_LESS_FLOAT = 33,
    ECL_OP_JUMP_LESS_EQUAL_INT = 34,
    ECL_OP_JUMP_LESS_EQUAL_FLOAT = 35,
    ECL_OP_JUMP_GREATER_INT = 36,
    ECL_OP_JUMP_GREATER_FLOAT = 37,
    ECL_OP_JUMP_GREATER_EQUAL_INT = 38,
    ECL_OP_JUMP_GREATER_EQUAL_FLOAT = 39,
    ECL_OP_NORMALIZE_ANGLE = 40,
    ECL_OP_CALL = 41,
    ECL_OP_RETURN = 42,
    ECL_OP_GET_BOSS_INT = 43,
    ECL_OP_GET_BOSS_FLOAT = 44,
    ECL_OP_WAIT = 45,
    ECL_OP_SET_POS = 46,
    ECL_OP_SET_VELOCITY = 47,
    ECL_OP_SET_ANGULAR_VELOCITY = 48,
    ECL_OP_SET_SPEED = 49,
    ECL_OP_SET_ACCELERATION = 50,
    ECL_OP_RANDOM_FLOAT_BETWEEN = 51,
    ECL_OP_RANDOM_ANGLE_IN_BOUNDS = 52,
    ECL_OP_AIM_AT_PLAYER = 53,
    ECL_OP_MOVE_ANGLE = 54,
    ECL_OP_MOVE_TO = 55,
    ECL_OP_MOVE_CIRCLE = 56,
    ECL_OP_SET_CIRCLE_RADIUS = 57,
    ECL_OP_SET_CIRCLE_ANGLE = 58,
    // 59 to 61 move in one mode for a number of frames.
    ECL_OP_TIME_ANGLE_MOVE = 59,
    ECL_OP_TIME_CIRCLE_MOVE = 60,
    ECL_OP_TIME_MOVE_TO = 61,
    ECL_OP_SET_BOUNDS = 62,
    ECL_OP_CLEAR_BOUNDS = 63,
    // 64 to 72 fire with each BULLET_AIM in order.
    ECL_OP_FIRE_FAN_AT_PLAYER = 64,
    ECL_OP_FIRE_FAN = 65,
    ECL_OP_FIRE_RING_AT_PLAYER = 66,
    ECL_OP_FIRE_RING = 67,
    ECL_OP_FIRE_RING_OFFSET_AT_PLAYER = 68,
    ECL_OP_FIRE_RING_OFFSET = 69,
    ECL_OP_FIRE_RANDOM_ANGLE = 70,
    ECL_OP_FIRE_RING_RANDOM_SPEED = 71,
    ECL_OP_FIRE_RANDOM = 72,
    ECL_OP_SET_INTERVAL = 73,
    ECL_OP_SET_INTERVAL_RANDOM = 74,
    ECL_OP_HOLD_FIRE = 75,
    ECL_OP_RELEASE_FIRE = 76,
    ECL_OP_FIRE_AGAIN = 77,
    ECL_OP_SET_SHOT_OFFSET = 78,
    ECL_OP_SET_SHOT_COMMAND = 79,
    ECL_OP_CLEAR_BULLETS = 80,
    ECL_OP_SET_SHOT_SOUND = 81,
    ECL_OP_LASER = 82,
    ECL_OP_LASER_AT_PLAYER = 83,
    ECL_OP_SET_LASER_INDEX = 84,
    ECL_OP_TURN_LASER = 85,
    ECL_OP_AIM_LASER = 86,
    ECL_OP_MOVE_LASER = 87,
    ECL_OP_TEST_LASER = 88,
    ECL_OP_END_LASER = 89,
    ECL_OP_START_SPELL_CARD = 90,
    ECL_OP_END_SPELL_CARD = 91,
    ECL_OP_SPAWN = 92,
    ECL_OP_SPAWN_RELATIVE = 93,
    ECL_OP_CLEAR_ENEMIES = 94,
    ECL_OP_START_SCRIPT = 95,
    ECL_OP_SET_TURNS = 96,
    ECL_OP_START_SPRITE_SCRIPT = 97,
    ECL_OP_SET_DEATH_EFFECTS = 98,
    ECL_OP_SET_BOSS = 99,
    ECL_OP_ADD_ORBIT = 100,
    ECL_OP_SET_HITBOX = 101,
    ECL_OP_SET_CONTACT_DAMAGE = 102,
    ECL_OP_SET_TAKES_DAMAGE = 103,
    ECL_OP_SET_HIT_BY_SHOTS = 104,
    ECL_OP_PLAY_SOUND = 105,
    ECL_OP_SET_DEATH = 106,
    ECL_OP_SET_DEATH_SUB = 107,
    ECL_OP_SET_INTERRUPT = 108,
    ECL_OP_INTERRUPT = 109,
    ECL_OP_SET_LIFE = 110,
    ECL_OP_SET_CALLBACK_TIMER = 111,
    ECL_OP_SET_LIFE_CALLBACK = 112,
    ECL_OP_SET_LIFE_CALLBACK_SUB = 113,
    ECL_OP_SET_TIME_CALLBACK = 114,
    ECL_OP_SET_TIME_CALLBACK_SUB = 115,
    ECL_OP_SET_SHOOTABLE = 116,
    ECL_OP_SPAWN_EFFECT = 117,
    ECL_OP_SPAWN_MOVING_EFFECT = 118,
    ECL_OP_DROP_ITEMS = 119,
    ECL_OP_SET_ROTATE_SPRITE = 120,
    ECL_OP_CALL_FUNC = 121,
    ECL_OP_SET_FRAME_FUNC = 122,
    ECL_OP_ADD_TIME = 123,
    ECL_OP_DROP_ITEM = 124,
    ECL_OP_SET_STAGE_LABEL = 125,
    ECL_OP_SET_BOSS_LIVES = 126,
    ECL_OP_INTERRUPT_SCRIPT = 128,
    ECL_OP_INTERRUPT_SPRITE_SCRIPT = 129,
    ECL_OP_SET_NO_CALL_STACK = 130,
    ECL_OP_SET_RANK_GROWTH = 131,
    ECL_OP_SET_HIDDEN = 132,
    ECL_OP_DEATH_SUB_ON_TIMER = 133,
    ECL_OP_FORGET_LASERS = 134,
    ECL_OP_SET_KEEP_SPELL_CARD = 135,
    ECL_OP_SET_GRAZEABLE = 136,
    ECL_OP_SET_STAYS_OFF_SCREEN = 137,
    ECL_OP_SET_TRAIL = 138,
    ECL_OP_SET_BOSS_SECTION = 139,
    ECL_OP_SET_EFFECT_COLOR = 140,
    ECL_OP_SET_INVULNERABLE = 142,
    ECL_OP_CLEAR_BULLETS_IN_CIRCLE = 143,
    ECL_OP_SET_PERIODIC_CALL = 144,
    ECL_OP_INTERRUPT_BOSS = 145,
    ECL_OP_DELETE_BULLETS = 146,
    ECL_OP_READ_UNUSED = 147, // reads a value that nothing uses
    ECL_OP_SET_LIFE_CALLBACK_AT = 148,
    ECL_OP_SET_SPELL_CIRCLE = 149,
    ECL_OP_SET_SPRITE_ROTATION = 150,
    ECL_OP_POLAR = 151, // sets x and y from an angle and a distance
    ECL_OP_SET_LASER_ANGLE = 152,
    ECL_OP_SET_EXTRA_HITBOX = 153,
    ECL_OP_DROP_POINT_ITEMS = 154,
    ECL_OP_RANDOM_SIDE_ANGLE = 155, // toward the side of the player
    ECL_OP_HIDE_LASER_ORIGIN = 156,
    ECL_OP_SET_LASER_LENGTH = 157,
    ECL_OP_SET_LASER_OFFSETS = 158,
    ECL_OP_LERP = 159,
    ECL_OP_ADD_CHERRY = 160,
    ECL_OP_SET_HALTED_BY_BOMB = 161,
} ECL_OP;

// Holds one argument word, which an opcode reads as an int or a float.
typedef union {
    int32_t i;
    float f;
} ECL_ARG;

// Holds one parsed instruction. Normally bit n of param_mask marks word n
// as a variable. For opcodes 64 to 72, bits 0 and 1 are the two halves of
// word 0 (sprite, color), then bits 2 to 7 are words 1 to 6. For 82 and
// 83, bit 1 is the high half of word 0 (color), then bit n + 1 is word n.
// Instructions stay writable: an operand that names no variable is
// written into the instruction itself, which every enemy on that
// subroutine shares.
typedef struct {
    int32_t time;
    int16_t opcode;
    uint8_t difficulty_mask;
    uint16_t param_mask;
    int32_t target; // the instruction a jump goes to
    ECL_ARG args[ECL_ARG_COUNT];
    char *text; // the spell card name of opcode 90, in UTF-8
} ECL_INSTR;

// Holds the instructions of a subroutine. The last one has time -1 and
// never runs.
typedef struct {
    ECL_INSTR *instrs;
    int32_t count;
} ECL_SUB;

// Returns instruction pc of subroutine sub of the loaded file.
ECL_INSTR *Ecl_GetInstr(int16_t sub, int32_t pc);

// Returns whether sub names a subroutine of the loaded file.
bool Ecl_IsSub(int32_t sub);
