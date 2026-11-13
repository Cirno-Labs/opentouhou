#pragma once

#include "th06/timer.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include <stdint.h>

// Loads and runs the enemy scripts of a stage (ECL): the subroutines that
// move each enemy, fire its patterns, run its spell cards and branch on
// its state, and the timeline that enemy.h walks to spawn enemies. The
// loaded file is module state.

typedef struct ENEMY ENEMY;

#define ECL_INT_VAR_COUNT 8
#define ECL_FLOAT_VAR_COUNT 4

// Selects one of the pattern functions that an enemy script calls once
// (opcode 121) or sets as the frame function of its state (opcode 122).
// Scripts select a function by number, so the numbers are fixed. Only the
// functions marked "frame" run as a frame function. A frame function runs
// without an opcode, so one that reads the argument of opcode 121 (0, 1,
// 4, 7, 13 and 16) is fatal as a frame function.
typedef enum {
    ECL_FUNC_NONE = -1,
    ECL_FUNC_FREEZE_BULLETS = 0, // stops the bullets, or sends them drifting
    ECL_FUNC_SHOOT_FROM_RANDOM_POINT = 1,
    ECL_FUNC_STAR_PATTERN = 2, // frame
    ECL_FUNC_GET_SHOT_TYPE_VALUES = 3,
    ECL_FUNC_FREEZE_OR_SEND_OFF = 4, // freezes the game, or sends bullets off
    ECL_FUNC_SIDE_ARC_FANS = 5, // frame
    ECL_FUNC_SPARK_CIRCLE = 6, // frame
    ECL_FUNC_TWO_RING_SHOTS = 7,
    ECL_FUNC_SHOOT_FROM_LARGE_BULLETS = 8,
    ECL_FUNC_SWIRL_STOPPED_BULLETS = 9,
    ECL_FUNC_SPARKS_AND_BOMB_SHIELD = 10, // frame
    ECL_FUNC_SCATTER_STOPPED_BULLETS = 11,
    ECL_FUNC_SHOOT_FROM_LASERS = 12,
    ECL_FUNC_ARENA_CIRCLE_SHOTS = 13,
    ECL_FUNC_SHOOT_ALONG_LASERS = 14,
    ECL_FUNC_BURST_AROUND_LARGE_BULLETS = 15, // frame
    ECL_FUNC_FOLLOW_LIFE = 16,
} ECL_FUNC;

// Holds where one enemy is in its script. A call or an interrupt saves
// the whole state on the stack of the enemy.
typedef struct {
    int16_t sub; // the subroutine that runs
    int32_t pc; // the next instruction of sub
    TIMER timer;
    ECL_FUNC frame_func; // runs each frame while the enemy has life
    // Hold the variables of the script. Operands -10001 to -10004 name
    // int_vars 0 to 3, -10005 to -10008 float_vars 0 to 3, and -10009 to
    // -10012 int_vars 4 to 7.
    int32_t int_vars[ECL_INT_VAR_COUNT];
    float float_vars[ECL_FLOAT_VAR_COUNT];
    int32_t compare; // the last comparison: -1 less, 0 equal, 1 greater
} ECL_STATE;

// Tells what a frame of an enemy script asks of the enemy.
typedef enum {
    ECL_RUN_CONTINUE,
    ECL_RUN_DELETE, // opcode 1
} ECL_RUN;

// Selects what an entry of the timeline does on its frame. The spawn
// entries do nothing while a boss is present. A spawn without its own
// values gives the enemy the life and score of its subroutine and an item
// from the drop table. A random spawn places the enemy at random in the
// player area on each axis whose position is -990 or less. A mirrored
// spawn mirrors the enemy after its first frame.
typedef enum {
    ECL_TIMELINE_SPAWN = 0,
    ECL_TIMELINE_SPAWN_DEFAULT = 1,
    ECL_TIMELINE_SPAWN_MIRRORED = 2,
    ECL_TIMELINE_SPAWN_MIRRORED_DEFAULT = 3,
    ECL_TIMELINE_SPAWN_RANDOM = 4,
    ECL_TIMELINE_SPAWN_RANDOM_DEFAULT = 5,
    ECL_TIMELINE_SPAWN_RANDOM_MIRRORED = 6,
    ECL_TIMELINE_SPAWN_RANDOM_MIRRORED_DEFAULT = 7,
    // Starts dialogue arg + 10 * the character, or 3 + 10 * the character
    // for arg 1 on easy stage 5.
    ECL_TIMELINE_START_MSG = 8,
    ECL_TIMELINE_WAIT_MSG = 9, // waits while Gui_MsgHoldsTimeline holds
    ECL_TIMELINE_INTERRUPT_BOSS = 10,
    ECL_TIMELINE_SET_POWER = 11, // sets the power to arg
    ECL_TIMELINE_WAIT_BOSS = 12, // waits while boss arg is active
} ECL_TIMELINE_OP;

// Holds one entry of the timeline. The last entry has a negative time.
typedef struct {
    int16_t time;
    int16_t opcode; // ECL_TIMELINE_OP
    int16_t arg; // the subroutine of a spawn
    union {
        // A negative life or score is the subroutine's own. The item is
        // as in Enemy_Spawn.
        struct {
            VEC3 pos;
            int16_t life;
            int16_t item;
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

// Loads the enemy script file of g_GameManager.run.stage. A file whose
// subroutine table, timeline or instructions run past its end, that names
// a subroutine it does not have, that jumps to a position that is not an
// instruction, or whose spell card names are not CP932 text, fails the
// load.
RESULT Ecl_Load(void);
// Releases the file loaded by Ecl_Load.
void Ecl_Release(void);
// Returns the timeline of the loaded file.
const ECL_TIMELINE_ENTRY *Ecl_GetTimeline(void);

// Starts subroutine sub on state at its frame 0. Keeps the variables, the
// comparison and the frame function of state.
void Ecl_StartSub(ECL_STATE *state, int16_t sub);
// Runs a frame of the script of enemy: the instructions due, including
// calls, returns and interrupts, then the movement and, while the enemy
// has life, the timed shots, the sprite direction and the frame function.
ECL_RUN Ecl_RunEnemy(ENEMY *enemy);
