#pragma once

#include "th01/input.h"
#include "th01/settings.h"
#include "th01/sprite.h"

#define PLAYER_TOP 368

typedef enum {
    PLAYER_MODE_REGULAR,
    PLAYER_MODE_SHOOT,
    PLAYER_MODE_SWING_OR_SLIDE,
    PLAYER_MODE_SPECIAL_FIRST,
    PLAYER_MODE_SPECIAL_SECOND,
    PLAYER_MODE_SPECIAL_THIRD,
} PLAYER_MODE;

typedef enum {
    PLAYER_FACING_RIGHT = 0,
    PLAYER_FACING_LEFT = 1,
} PLAYER_FACING;

typedef enum {
    PLAYER_SPECIAL_FLIPKICK_MOVING = 0,
    PLAYER_SPECIAL_FLIPKICK_STATIONARY = 2,
    PLAYER_SPECIAL_SLIDEKICK = 4,
    PLAYER_SPECIAL_SHOTCOMBO = 6,
} PLAYER_SPECIAL;

typedef enum {
    PLAYER_BOMB_NONE = 0,
    PLAYER_BOMB_START = 2,
    PLAYER_BOMB_ACTIVE = 3,
} PLAYER_BOMB;

typedef struct {
    int16_t left;
    bool deflecting, sliding, is_hit, invincible, invincible_against_orb;
    int16_t invincibility_time, remaining_bombs;
    bool bomb_damaging;
    uint32_t bomb_frame;
    int8_t dash_cycle, mode_frame, mode, dash_direction, bomb_flag;
    // Stores input horizontal direction, or PLAYER_SPECIAL | PLAYER_FACING
    // during special modes. Reset uses -1; transitions retain stale values.
    int8_t submode, previous_ptn, deflect_duration;
    bool bombing, combo_enabled;
} PLAYER;

extern PLAYER g_Player;

// Zeroes the player for a new world.
void Player_Init(void);

// Keeps the suspended bomb branch. Zero-initialize before its first use.
// The field belongs to the Player_ functions; never copy a pending token.
typedef struct {
    bool pending;
} PLAYER_BOMB_CONTINUATION;

typedef struct {
    const PTN *miko;
    const BOS *miko_ac, *miko_ac2;
} PLAYER_SPRITES;

// Validates all actor image dimensions/counts before ticks. Images remain
// immutable and belong to the caller's grouped asset lifetime.
RESULT Player_ValidateSprites(const PLAYER_SPRITES *sprites);

// Resets action state and consumes input.bomb without moving, drawing or
// clearing hit, invincibility, remaining bombs or bomb_frame. Rank values
// outside the four settings ranks use Normal's deflection duration.
void Player_Reset(int32_t difficulty);
void Player_MoveAndClamp(int16_t delta);

// Runs real movement, shots, swings, slides and all three combo stages,
// drawing directly with sprites. Reads only supplied state/input/RNG and
// allocates nothing. Mutates held shot/strike/bomb flags as the source
// consumes them. Call before ordinary Orb motion. Does not update shots,
// sense input or advance bomb_frame.
//
// Bomb start is deferred one update. HUD count follows the decrement, then
// Orb repulsion precedes the real bomb operation; its outcome controls
// drawing and input consumption. Call Player_FinishBombUpdate once that
// operation completes, including its nested waits, with its result; never
// begin another player update while continuation.pending.
//
// Regular lethal Orb tests, invincibility sprites/timer, HUD, items,
// pellets and gameplay scene integration remain external. Bomb and Miss
// provide their reusable cooperative operations in separate modules.
// is_hit is set by real Orb repulsion tests if their friction reaches zero.
// A miss requires its complete cooperative sequence before updates resume;
// resetting alone does not implement a miss or change lives and bombs.
bool Player_BeginUpdate(
    PLAYER_BOMB_CONTINUATION *continuation, const PLAYER_SPRITES *sprites);
void Player_FinishBombUpdate(
    PLAYER_BOMB_CONTINUATION *continuation, const PLAYER_SPRITES *sprites,
    bool done);
