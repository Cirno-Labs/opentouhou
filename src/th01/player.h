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
    // Horizontal input direction, or PLAYER_SPECIAL | PLAYER_FACING.
    int8_t submode, previous_ptn, deflect_duration;
    bool bombing, combo_enabled;
} PLAYER;

// Keeps the suspended bomb branch. Zero-initialize before its first use.
// The field belongs to the Player_ functions; never copy a pending token.
typedef struct {
    bool pending;
} PLAYER_BOMB_CONTINUATION;

typedef struct {
    const PTN *miko;
    const BOS *miko_ac, *miko_ac2;
} PLAYER_SPRITES;

extern const PLAYER *const g_Player;

// Zeroes the player for a new world.
void Player_Init(void);

// Marks the player hit, which ends the life at the next frame.
void Player_Hit(void);

// Drops a pending hit.
void Player_ClearHit(void);

// Ends invincibility and its timer.
void Player_EndInvincibility(void);

// Counts invincibility down by one frame and ends it at the last frame.
void Player_TickInvincibility(void);

// Ends bomb damage and starts 100 invincible frames.
void Player_EndBomb(void);

// Stops deflection and bomb damage.
void Player_StopActions(void);

// Moves the player to the center of the playfield.
void Player_Center(void);

// Moves the player to left, unclamped.
void Player_MoveTo(int16_t left);

// Turns invincibility on and leaves its timer.
void Player_BecomeInvincible(void);

// Turns invincibility back on while its timer runs.
void Player_ResumeInvincibility(void);

// Sets the invincibility timer to frames.
void Player_GrantInvincibility(int16_t frames);

// Lets the bomb damage until Player_EndBomb.
void Player_StartBombDamage(void);

// Sets the frame count that the bomb's damage and casting pose follow.
void Player_SetBombFrame(uint32_t frame);

// Counts one frame on the bomb frame count.
void Player_AdvanceBombFrame(void);

// Sets the remaining bombs; the caller redraws the HUD.
void Player_SetBombs(int16_t count);

// Gains one bomb; the caller keeps the cap and redraws the HUD.
void Player_GainBomb(void);

// Validates all actor image dimensions/counts before ticks. Images remain
// immutable and belong to the caller's grouped asset lifetime.
RESULT Player_ValidateSprites(const PLAYER_SPRITES *sprites);

// Resets action state and consumes input.bomb, keeping hit, invincibility
// and bombs.
void Player_Reset(int32_t difficulty);
void Player_MoveAndClamp(int16_t delta);

// Moves, shoots, swings and slides, drawing with sprites; call it before
// Orb motion. Returns whether a bomb operation is pending: run it, then
// Player_FinishBombUpdate with its result before the next update.
bool Player_BeginUpdate(
    PLAYER_BOMB_CONTINUATION *continuation, const PLAYER_SPRITES *sprites);
void Player_FinishBombUpdate(
    PLAYER_BOMB_CONTINUATION *continuation, const PLAYER_SPRITES *sprites,
    bool done);
