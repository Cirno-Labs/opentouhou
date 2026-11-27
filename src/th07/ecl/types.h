#pragma once

#include "core/math/vec.h"
#include "stg/timer.h"

#include <stdint.h>

// The types that more than one part of th07/ecl shares.

typedef struct ENEMY ENEMY;

#define ECL_TIMELINE_COUNT 16
#define ECL_INT_VAR_COUNT 12
#define ECL_FLOAT_VAR_COUNT 14
#define ECL_INTERP_COUNT 8

// Selects one of the pattern functions that an enemy script calls once
// (opcode 121) or sets as the frame function of its state (opcode 122).
// Scripts pick them by number, so the numbers are fixed. Each function
// receives word 1 of the instruction that selected it.
typedef enum {
    ECL_FUNC_NONE = -1,
    // Takes the position, velocity and angle of boss arg.
    ECL_FUNC_WARP_TO_BOSS = 0,
    // Shakes, flashes and turns the bullets not turned yet.
    ECL_FUNC_SPIN_BULLETS = 1,
    ECL_FUNC_FANS_FROM_NEAR_BULLETS = 2,
    ECL_FUNC_NOP = 3,
    // Puts the position of a tall bullet into floats 0 and 1.
    ECL_FUNC_TALL_BULLET_TO_EFFECT = 4,
    ECL_FUNC_COPY_BOSS_CIRCLE = 5,
    ECL_FUNC_FANS_BACK_FROM_BULLETS = 6,
    ECL_FUNC_TURN_AT_LASER_EDGES = 7,
    ECL_FUNC_DEFLECT_OFF_LASERS = 8,
    ECL_FUNC_START_PETALS_FALLING = 9,
    ECL_FUNC_SLOW_DOWN = 10, // sets the game speed to 1 / arg
    ECL_FUNC_END_SLOW_DOWN = 11,
    ECL_FUNC_BURST_TALL_BULLETS = 12, // by difficulty
    ECL_FUNC_TURN_BULLETS_BELOW = 13,
    ECL_FUNC_AIM_TURNED_BULLETS = 14,
    ECL_FUNC_FLASH = 15,
    ECL_FUNC_FIRE_BACK_FROM_UNTURNED = 16,
    ECL_FUNC_BULLETS_TO_ENEMIES = 17,
    ECL_FUNC_COUNT_UNTURNED = 18, // into ints 0
    ECL_FUNC_FADE_OUT_MUSIC = 19,
    ECL_FUNC_PLAY_BOSS_MUSIC = 20,
    ECL_FUNC_BURST_TALL_BULLETS_FIXED = 21,
    ECL_FUNC_FIRE_FROM_TALL_BULLETS = 22,
    ECL_FUNC_FIRE_FROM_TALL_BULLETS_2 = 23,
    ECL_FUNC_COUNT = 24,
} ECL_FUNC;

// Hold the variables of one script; spawns and periodic calls copy them
// whole. Operands 10000-10003, 10012-10015 and 10029-10032 name ints 0 to
// 11; 10004-10011, 10072-10073 and 10033-10036 name floats 0 to 13.
typedef struct {
    int32_t ints[ECL_INT_VAR_COUNT];
    float floats[ECL_FLOAT_VAR_COUNT];
} ECL_VARS;

// Moves one variable along a curve over duration frames (opcode 27).
typedef struct {
    TIMER timer;
    int32_t duration;
    int32_t kind; // 0 to 6 linear, 7 Hermite
    int32_t ease; // ENEMY_EASE
    float from;
    float to;
    float tangent_from;
    float tangent_to;
    // Holds the file's float operand. One that names no float variable
    // takes the values itself.
    float var;
    bool active;
} ECL_INTERP;

// Holds where one enemy is in its script. A call or an interrupt saves
// the whole state.
typedef struct {
    int16_t sub;
    int32_t pc;
    TIMER timer;
    ECL_FUNC frame_func;
    int32_t frame_func_arg; // word 1 of its opcode 122
    ECL_VARS vars;
    TIMER wait; // holds the instructions while above 0 (opcode 45)
    ECL_INTERP interps[ECL_INTERP_COUNT];
    // Is set by the timed call of opcode 144. Its return copies vars to
    // the periodic vars of the enemy.
    bool in_timed_call;
} ECL_STATE;

// Tells what a frame of an enemy script asks of the enemy.
typedef enum {
    ECL_RUN_CONTINUE,
    ECL_RUN_DELETE,
} ECL_RUN;

// Selects what an entry of a timeline does on its frame. The spawn
// entries do nothing while a boss is present. A spawn without its own
// values gives the enemy the life and score of its subroutine and an item
// from the drop table. A random spawn places the enemy at random in the
// player area on each axis whose position is -990 or less. The mirrored
// spawns 2 and 3 mirror the enemy before its first frame, 6 and 7 after
// it.
typedef enum {
    ECL_TIMELINE_SPAWN = 0,
    ECL_TIMELINE_SPAWN_DEFAULT = 1,
    ECL_TIMELINE_SPAWN_MIRRORED = 2,
    ECL_TIMELINE_SPAWN_MIRRORED_DEFAULT = 3,
    ECL_TIMELINE_SPAWN_RANDOM = 4,
    ECL_TIMELINE_SPAWN_RANDOM_DEFAULT = 5,
    ECL_TIMELINE_SPAWN_RANDOM_MIRRORED = 6,
    ECL_TIMELINE_SPAWN_RANDOM_MIRRORED_DEFAULT = 7,
    ECL_TIMELINE_START_MSG = 8, // starts dialogue arg + 10 * the character
    ECL_TIMELINE_WAIT_MSG = 9, // waits while Gui_MsgHoldsTimeline holds
    ECL_TIMELINE_INTERRUPT_BOSS = 10,
    ECL_TIMELINE_SET_POWER = 11, // sets the power to arg
    ECL_TIMELINE_WAIT_BOSS = 12, // waits while boss arg is active
} ECL_TIMELINE_OP;

// Holds one entry of a timeline. The last entry has a negative time.
typedef struct {
    int16_t time;
    int16_t opcode; // ECL_TIMELINE_OP
    int16_t arg; // the subroutine of a spawn
    union {
        // A negative life or score is the subroutine's own. The item is
        // as in Enemy_Spawn.
        struct {
            VEC3 pos;
            int32_t life;
            int32_t item;
            int32_t score;
        } spawn;
        // The file stores these two in the bytes of spawn.pos.x and
        // spawn.pos.y.
        struct {
            int32_t boss;
            int32_t interrupt;
        } interrupt;
    } args;
} ECL_TIMELINE_ENTRY;
