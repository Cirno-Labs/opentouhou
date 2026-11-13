#pragma once

#include "th06/anm.h"
#include "th06/timer.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include <stdint.h>

// Runs the player: its movement, its death and respawn, its shots and its
// bomb (bomb.h). Tests its shots and bomb against enemies, and enemy
// bullets and lasers against the player. The sprite scripts of the shots
// are part of the simulation: a shot ends with its script. Drawing comes
// with graphics.

#define PLAYER_BULLET_COUNT 0x50
#define PLAYER_LASER_COUNT 2
#define PLAYER_OPTION_COUNT 2
#define PLAYER_BOMB_PART_COUNT 8
#define PLAYER_BOMB_SPRITE_COUNT 32
#define PLAYER_BOMB_HIT_COUNT 32
#define PLAYER_CLEAR_AREA_COUNT 16

typedef enum {
    PLAYER_ALIVE = 0,
    PLAYER_RESPAWNING = 1,
    PLAYER_DYING = 2,
    // Holds after a respawn, and through every frame of a bomb.
    PLAYER_INVULNERABLE = 3,
} PLAYER_STATE;

typedef enum {
    PLAYER_OPTIONS_NONE = 0, // the power is below 8
    PLAYER_OPTIONS_OUT = 1,
    PLAYER_OPTIONS_SLIDING_IN = 2, // over 8 frames, while focused
    PLAYER_OPTIONS_IN = 3,
    PLAYER_OPTIONS_SLIDING_OUT = 4,
} PLAYER_OPTIONS;

typedef enum {
    PLAYER_BULLET_STRAIGHT = 0,
    // Steers to homing_target for its first 40 frames, or speeds up to
    // 10 when there is none.
    PLAYER_BULLET_HOMING = 1,
    // Accelerates upwards. It keeps hitting after its first hit, with a
    // quarter of the damage and a larger size each time.
    PLAYER_BULLET_RISING = 2,
    // Stays on its option and reaches to the top of the play area, while
    // its entry of laser_timers runs.
    PLAYER_BULLET_LASER = 3,
} PLAYER_BULLET_TYPE;

typedef enum {
    PLAYER_BULLET_FREE = 0,
    PLAYER_BULLET_FLYING = 1,
    PLAYER_BULLET_HIT = 2, // freed when the hit script ends
} PLAYER_BULLET_STATE;

typedef struct {
    ANM_VM vm;
    VEC3 pos;
    VEC3 size;
    VEC3 velocity;
    float speed;
    float angle;
    TIMER timer;
    int16_t damage;
    PLAYER_BULLET_STATE state;
    PLAYER_BULLET_TYPE type;
    // Hold the laser of a PLAYER_BULLET_LASER: its entry of laser_timers,
    // its option from 1, and its horizontal offset from that option.
    int16_t laser;
    int16_t option;
    float laser_offset_x;
} PLAYER_BULLET;

// Tells what a box touched in Player_CheckHit and Player_CheckGraze.
typedef enum {
    PLAYER_CONTACT_NONE = 0,
    PLAYER_CONTACT_BOX = 1, // the hitbox, or the graze area
    PLAYER_CONTACT_CLEARED = 2, // an area of the bomb that clears bullets
} PLAYER_CONTACT;

// Holds one of the parts that a bomb moves: the orbs of Reimu A, the
// pillars of Reimu B, the stars of Marisa A. For an orb of Reimu A, state
// is 1 while it flies; from 2 it counts the frames of its burst up to 30.
typedef struct {
    int32_t state;
    float speed;
    VEC3 pos;
    VEC3 velocity;
} PLAYER_BOMB_PART;

// Holds a box of the bomb that damages enemies. The box is unused while
// size.x is 0 or less.
typedef struct {
    VEC3 pos;
    VEC3 size;
    int32_t damage; // dealt to each enemy that the box touches, per frame
    int32_t damage_dealt;
} PLAYER_BOMB_HIT;

// Holds a box of the bomb, centred on x and y, that clears the enemy
// bullets that touch it. The box is unused while width is 0.
typedef struct {
    float x, y, width, height;
} PLAYER_CLEAR_AREA;

typedef struct {
    ANM_VM vm;
    ANM_VM option_vms[PLAYER_OPTION_COUNT];
    VEC3 pos;

    // Holds the boxes that enemy bullets and items test. The sizes are
    // half sizes: a box runs from pos - size to pos + size.
    struct {
        VEC3 hitbox_size;
        VEC3 graze_size;
        VEC3 hitbox_min, hitbox_max;
        VEC3 graze_min, graze_max;
    } box;

    struct {
        // Hold the speeds of the shot type. The diagonal speeds are the
        // straight speeds divided by sqrtf(2).
        float speed, speed_focused;
        float speed_diagonal, speed_diagonal_focused;
        // Scale the movement: 0.3 during the bomb of Marisa B, else 1.
        float speed_scale_x, speed_scale_y;
        float last_x; // the horizontal move of the last frame
        bool focused;
    } move;

    struct {
        PLAYER_STATE value;
        // Counts the frames of the death and the respawn, and counts down
        // the frames of PLAYER_INVULNERABLE.
        TIMER timer;
        // Lets a bomb start while it is not 0. It is 8 at the start of a
        // game and 6 after a respawn, and holds while the player is alive.
        // While the player is dying it counts down, which makes it the
        // window for a bomb after a hit, and the death happens at 0. It is
        // 0 while respawning.
        int32_t bomb_window;
        // Holds the frames left of clearing every bullet after a respawn.
        int32_t respawn_clear_frames;
    } state;

    struct {
        PLAYER_OPTIONS state;
        TIMER timer;
        VEC3 pos[PLAYER_OPTION_COUNT];
    } options;

    struct {
        PLAYER_BULLET bullets[PLAYER_BULLET_COUNT];
        // Counts the frames of a burst of shots: -1 when idle, started
        // while the shot button is held, and stopped after 30 frames.
        TIMER fire_timer;
        // Count down the frames left of each laser.
        TIMER laser_timers[PLAYER_LASER_COUNT];
        // Counts the hits on enemies. An effect bursts every few hits.
        uint8_t hit_counter;
    } shots;

    // Holds the position of the lowest enemy that can be shot. The player
    // update resets it to -999, -999, and the enemy update sets it. An x
    // of -100 or less means no target.
    VEC3 homing_target;

    struct {
        bool running;
        int32_t duration;
        TIMER timer;
        PLAYER_BOMB_PART parts[PLAYER_BOMB_PART_COUNT];
        ANM_VM vms[PLAYER_BOMB_SPRITE_COUNT];
        // The player update clears the boxes at the start of each frame,
        // and the bomb sets those that act in that frame.
        PLAYER_BOMB_HIT hits[PLAYER_BOMB_HIT_COUNT];
        PLAYER_CLEAR_AREA clear_areas[PLAYER_CLEAR_AREA_COUNT];
    } bomb;
} PLAYER;

extern PLAYER g_Player;

// Adds the update element (priority 7) and the draw elements (priorities
// 5 and 7), and resets g_Player. Adding loads the sprites of the
// character, except in SCENE_NEXT_STAGE, which keeps them. It then places
// the player at the bottom of the play area, respawning.
RESULT Player_Register(void);
// Removes the three elements. Releases the sprites of the character,
// except in SCENE_NEXT_STAGE.
void Player_Remove(void);

// Returns the angle from pos to the player, or pi / 2 when they coincide.
float Player_AngleFrom(VEC3 pos);
// Returns the angle from the player to pos, or pi / 2 when they coincide.
float Player_AngleTo(VEC3 pos);

// Tests the box of size centred on pos, an enemy bullet or an enemy.
// Returns PLAYER_CONTACT_CLEARED when the box touches a clear area, else
// PLAYER_CONTACT_BOX when it touches the hitbox, else
// PLAYER_CONTACT_NONE. A touch of the hitbox kills the player when it is
// PLAYER_ALIVE, but returns PLAYER_CONTACT_BOX in every state.
PLAYER_CONTACT Player_CheckHit(VEC3 pos, VEC3 size);
// Is like Player_CheckHit, with the box grown by 20 on each side, and
// grazes instead of killing. A touch of the hitbox returns
// PLAYER_CONTACT_NONE while the player is dying or respawning.
PLAYER_CONTACT Player_CheckGraze(VEC3 pos, VEC3 size);
// Tests a laser: the box of size centred on pos, turned by angle around
// origin. A touch of the hitbox kills the player when it is
// PLAYER_ALIVE. Otherwise, with can_graze, a box within 48 pixels of the
// hitbox grazes it, except while the player is dying or respawning.
void Player_CheckLaserHit(
    VEC3 pos, VEC3 size, VEC3 origin, float angle, bool can_graze);
// Returns whether the box of size centred on pos touches the graze box
// while the player is alive or invulnerable.
bool Player_TouchesGrazeBox(VEC3 pos, VEC3 size);
// Returns the damage that the shots and the bomb deal this frame to the
// box of size centred on pos, an enemy. Each shot that touches the box
// deals its damage, or a third of it (at least 1) during a bomb. A
// straight or homing shot then stops; a rising shot keeps going, weaker.
// Sets *hit_by_bomb to whether a box of a running bomb touched it.
int32_t Player_CalcDamage(VEC3 pos, VEC3 size, bool *hit_by_bomb);
