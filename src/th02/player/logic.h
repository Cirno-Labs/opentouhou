#pragma once

#include "th02/entity.h"
#include "th02/input.h"
#include "th02/patterns.h"

typedef struct {
    ENTITY_POINT position[2];
    ENTITY_POINT option[2];
    ENTITY_POINT current;
    int16_t sprite;
    uint8_t option_sprite;
    uint8_t power;
    uint8_t hit;
    uint8_t invincibility;
    uint8_t bomb_invincible;
    int8_t speed_x;
    int8_t speed_y;
    int8_t diagonal_x;
    int8_t diagonal_y;
    uint8_t shot_type;
} PLAYER;

extern const PLAYER *const g_Player;

// Initializes the player, once per gameplay process; stage and continue
// setup keep weapon and animation history.
void Player_Init(void);

// Resets the player for shot type 0..2 at stage start, keeping power,
// positions, cadence and slots; type zero keeps the second maximum.
void Player_ResetStage(uint8_t type);

// Resets only the positions on both pages.
void Player_ResetPosition(void);

// Moves the player on page back, clamped to X 28..390 and Y 16..344.
void Player_Move(uint8_t back, int16_t dx, int16_t dy);

// Invalidates the tiles under the player and both options, even below
// option power.
void Player_Invalidate(PLAYFIELD_PAGES pages, bool reduce_effects);

// Moves, fires and bombs for keys, with retail's diagonal precedence and
// signed speed quirks.
void Player_Controls(PLAYFIELD_PAGES pages, uint16_t keys);

// Runs and draws one player frame; returns true on game over (hit 255).
bool Player_Update(
    PLAYFIELD_PAGES pages, uint32_t frame, bool reduce_effects,
    const PATTERN_BANK *patterns, uint8_t start_bombs);

// Marks the player hit, which ends the life at the next frame.
void Player_Hit(void);

// Drops a pending hit.
void Player_ClearHit(void);

// Forces the game-over hit without granting further invincibility.
void Player_Kill(void);

// Starts invincibility for the given number of frames.
void Player_BeginInvincibility(uint8_t frames);

// Sets power directly.
void Player_SetPower(uint8_t power);

// Adds to power without clamping.
void Player_AddPower(int16_t delta);

// Moves both page positions back to the post-miss spawn point.
void Player_Respawn(void);

// Copies the option at from into to, for boss-intro positioning.
void Player_SyncOptions(uint8_t from, uint8_t to);

// Starts a bomb's invincibility.
void Player_BeginBomb(void);

// Ends a bomb's invincibility and starts its recovery invincibility.
void Player_EndBomb(void);

// Copies the current Y into page, for boss intros.
void Player_SyncPositionY(uint8_t page);

// Places page's option below-left of the player, for boss intros.
void Player_PlaceOption(uint8_t page);

// Copies the given page's position into current, for the miss animation.
void Player_SyncCurrent(uint8_t back);

// Moves the given page's position vertically during the miss animation.
void Player_MoveMissY(uint8_t back, int16_t dy);
