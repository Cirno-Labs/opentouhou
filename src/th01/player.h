#pragma once

#include "th01/input.h"
#include "th01/settings.h"
#include "th01/shots.h"
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

typedef enum {
    PLAYER_ACTION_PTN_PUT,
    PLAYER_ACTION_PTN_ERASE,
    PLAYER_ACTION_BOS_48_PUT,
    PLAYER_ACTION_BOS_48_ERASE,
    PLAYER_ACTION_BOS_48_OVERLAP,
    PLAYER_ACTION_BOS_32_PUT,
    PLAYER_ACTION_BOS_32_ERASE,
    PLAYER_ACTION_BOS_32_OVERLAP,
    PLAYER_ACTION_SHOT,
    PLAYER_ACTION_EFFECT,
    PLAYER_ACTION_HUD_BOMBS,
    PLAYER_ACTION_PELLET_SPEED_RAISE,
} PLAYER_ACTION_KIND;

typedef struct {
    PLAYER_ACTION_KIND kind;
    int16_t left, previous_left, image, previous_image;
    int16_t effect, previous_bombs;
    float pellet_speed_delta;
    SHOTS_ACTION shot;
} PLAYER_ACTION;

typedef struct {
    void (*action)(void *user, PLAYER_ACTION action);
    bool (*bomb_update)(void *user, int16_t frame);
    void *user;
} PLAYER_OUTPUT;

// Keeps the suspended bomb branch. Zero-initialize before its first use.
// The fields belong to the Player_ functions; never copy a pending token.
typedef struct {
    PLAYER_OUTPUT output;
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

// Runs real movement, shots, swings, slides and all three combo stages.
// Reads only supplied state/input/RNG and allocates nothing. Mutates held
// shot/strike/bomb flags as the source consumes them. Call before ordinary
// Orb motion. Does not update shots, sense input or advance bomb_frame.
// Actions execute synchronously and must not mutate actor/input/Orb state.
// Every reached callback is required; there is no default external behavior.
//
// Bomb start is deferred one update. HUD count follows the decrement, then
// Orb repulsion precedes bomb_update; its result controls drawing and input
// consumption. bomb_update implements the real external bomb operation and
// may change shared player invincibility, invincibility_time, bomb_damaging
// and position. frame is the low signed 16 bits of bomb_frame. Use BeginUpdate
// and FinishBombUpdate when the bomb operation suspends for nested waits.
//
// Regular lethal Orb tests, invincibility sprites/timer, HUD, items,
// pellets and gameplay scene integration remain external. Bomb and Miss
// provide their reusable cooperative operations in separate modules.
// is_hit is set by real Orb repulsion tests if their friction reaches zero.
// A miss requires its complete cooperative sequence before updates resume;
// resetting alone does not implement a miss or change lives and bombs.
void Player_UpdateAndRender(PLAYER_OUTPUT output);

// Runs the same update until completion or the bomb callback boundary.
// Returns true only when HUD decrement and Orb repulsion have completed and
// the real bomb operation must now run. It does not invoke bomb_update.
// Keep player/input/output alive and stable while suspended; the bomb may
// change the shared fields described above. Call Finish exactly once after
// that operation completes, including its nested waits. Never begin another
// player update while continuation.pending. No action prefix is replayed.
bool Player_BeginUpdate(
    PLAYER_BOMB_CONTINUATION *continuation, PLAYER_OUTPUT output);
void Player_FinishBombUpdate(PLAYER_BOMB_CONTINUATION *continuation, bool done);

// Runs only drawing actions. Coordinates are regular player left values;
// converts BOS to left-8 and top 352/368. Effects, HUD and pellet operations
// belong to their real external modules. SHOT forwards to Shots_RenderAction
// with supplied stage images. Requires validated player and stage assets.
void Player_RenderAction(
    const PLAYER_SPRITES *sprites, const PTN *stage, PLAYER_ACTION action);
