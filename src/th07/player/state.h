#pragma once

#include "th07/anm.h"
#include "th07/effect.h"
#include "th07/item.h"
#include "th07/player/shot_type.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include "stg/timer.h"

#include <stdint.h>

// Runs the player: its movement, its death and respawn, its shots, its bomb
// (bomb.h) and the Supernatural Border. Tests its shots and bomb against
// enemies, and enemy bullets and lasers against the player. The shot types
// come from SHT files (shot_type.h). The movement, the shots and the timers
// run at g_GameSpeed. th07/game_manager fills the cherry plus that starts
// the Border. The sprite scripts of the shots are part of the simulation: a
// shot ends with its script. Drawing comes with graphics.

#define PLAYER_BULLET_COUNT 0x60
#define PLAYER_LASER_COUNT 3
#define PLAYER_OPTION_COUNT 2
#define PLAYER_TRAIL_COUNT 16
#define PLAYER_DAMAGE_BOX_COUNT 0x70
#define PLAYER_LASER_BOX_FIRST 0x60 // the 16 boxes of the trail laser
#define PLAYER_CLEAR_AREA_COUNT 0x60
#define PLAYER_BOMB_PART_COUNT 0x80
#define PLAYER_BOMB_PART_SPRITE_COUNT 8
#define PLAYER_BOMB_PART_TRAIL_COUNT 0x20

typedef enum {
    PLAYER_ALIVE = 0,
    PLAYER_RESPAWNING = 1,
    PLAYER_DYING = 2,
    // Holds after a respawn, through every frame of a bomb, and for 40
    // frames after the Border.
    PLAYER_INVULNERABLE = 3,
    PLAYER_IN_BORDER = 4,
} PLAYER_STATE;

typedef enum {
    PLAYER_BORDER_NONE = 0,
    PLAYER_BORDER_RUNNING = 1,
    PLAYER_BORDER_PENDING = 2, // starts once the player can take it
} PLAYER_BORDER;

typedef enum {
    PLAYER_OPTIONS_NONE = 0, // before the first add
    PLAYER_OPTIONS_OUT = 1, // the lasers fire
    PLAYER_OPTIONS_SLIDING_IN = 2,
    PLAYER_OPTIONS_IN = 3, // the trail lasers fire
    PLAYER_OPTIONS_SLIDING_OUT = 4,
} PLAYER_OPTIONS;

typedef enum {
    PLAYER_BULLET_FREE = 0,
    PLAYER_BULLET_FLYING = 1,
    PLAYER_BULLET_HIT = 2, // freed when the hit script ends
} PLAYER_BULLET_STATE;

typedef struct {
    ANM_VM vm;
    VEC3 pos;
    VEC3 trail[PLAYER_TRAIL_COUNT]; // newest first
    VEC3 size;
    VEC3 velocity;
    float speed;
    float angle;
    TIMER timer;
    int16_t damage;
    PLAYER_BULLET_STATE state;
    SHOT_KIND kind;
    // Hold the laser of a laser or trail laser: its entry of lasers, its
    // option from 1, and its horizontal offset.
    int16_t laser;
    int16_t option;
    float laser_offset_x;
    int16_t trail_length; // the trail points that a trail laser uses
    const SHOT *shot; // the shot fired into the bullet
} PLAYER_BULLET;

// Holds a laser: the frames left, its bullet while one flies, and the shot
// that fired it.
typedef struct {
    TIMER timer;
    PLAYER_BULLET *bullet;
    const SHOT *shot;
} PLAYER_LASER;

// Tells what a box touched in Player_CheckHit and Player_CheckGraze.
typedef enum {
    PLAYER_CONTACT_NONE = 0,
    PLAYER_CONTACT_BOX = 1, // the hitbox, or the graze area
    PLAYER_CONTACT_CLEARED = 2, // a clear area
} PLAYER_CONTACT;

// Holds one of the parts that a bomb moves.
typedef struct {
    int32_t state;
    int32_t burst_timer;
    union {
        float homing_speed;
        float laser_angle;
        float speed_step; // added to speed every frame
    };
    float speed;
    float angle;
    VEC3 pos;
    VEC3 trail[PLAYER_BOMB_PART_TRAIL_COUNT]; // newest first
    VEC3 velocity;
    VEC3 acceleration;
    ANM_VM vms[PLAYER_BOMB_PART_SPRITE_COUNT];
    EFFECT *effect;
    TIMER timer;
} PLAYER_BOMB_PART;

// Holds a box that damages enemies: a bomb's, or from
// PLAYER_LASER_BOX_FIRST a trail laser's. The box is unused while size.x is
// 0 or less.
typedef struct {
    VEC3 pos;
    VEC3 size;
    int32_t damage; // dealt to each enemy that the box touches, per frame
    int32_t damage_dealt;
} PLAYER_DAMAGE_BOX;

// Holds an area, centred on x and y, that clears the enemy bullets that
// touch it: a box while width is set, else a circle while radius is set.
// The area is freed at the start of the update after frames_left runs out.
typedef struct {
    float x, y, width, height;
    float radius, radius_growth;
    int32_t frames_left;
    ITEM_TYPE item; // what a cleared bullet turns into
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
        VEC3 collect_size; // 12, 12, 5
        VEC3 hitbox_min, hitbox_max;
        VEC3 graze_min, graze_max;
        VEC3 collect_min, collect_max;
    } box;

    struct {
        // Scale the movement during some bombs, else 1.
        float speed_scale_x, speed_scale_y;
        float last_x; // the horizontal move of the last frame
        float delta_x; // last_x at the game speed
        bool focused;
    } move;

    struct {
        PLAYER_STATE value;
        // Counts the frames of the death and the respawn, and counts down
        // the frames of PLAYER_INVULNERABLE and PLAYER_IN_BORDER.
        TIMER timer;
        // Counts down the frames left to bomb while dying. The death
        // happens at 0.
        int32_t bomb_window;
        int32_t respawn_clear_frames; // of clearing every bullet, from 60
        TIMER border_length; // the timer when the Border started
        int32_t bomb_cooldown; // 40 after the Border
        bool bomb_started; // set as a bomb starts, until the bomb clears it
    } state;

    PLAYER_BORDER border_state;
    // Holds the item of the clear area that the last hit test touched. The
    // hit tests set it to ITEM_STAR first.
    ITEM_TYPE clear_item;

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
        // Counts the hits on enemies. An effect bursts every few hits.
        uint8_t hit_counter;
        PLAYER_LASER lasers[PLAYER_LASER_COUNT];
        float aim_angle; // that aimed shots fly at
    } shots;

    // Hold the positions that the homing shots and the shots that fire at
    // a target go to. The player update resets them to -999, -999 and
    // target_found to false, and the enemy update sets them. An x of -100
    // or less means no target.
    VEC3 homing_target;
    VEC3 aim_target;
    bool target_found;

    // The player update clears the boxes at the start of each frame, and
    // the bomb and the trail laser set those that act in that frame.
    PLAYER_DAMAGE_BOX damage_boxes[PLAYER_DAMAGE_BOX_COUNT];
    PLAYER_CLEAR_AREA clear_areas[PLAYER_CLEAR_AREA_COUNT];

    // Hold the effects that follow the player: while focused, through a
    // bomb, and through the Border.
    struct {
        EFFECT *focus;
        EFFECT *bomb_aura;
        EFFECT *border;
    } effects;

    struct {
        bool running;
        bool focused; // when the bomb started
        int32_t duration;
        int32_t drain; // of the cherry, each frame
        TIMER timer;
        PLAYER_BOMB_PART parts[PLAYER_BOMB_PART_COUNT];
        VEC3 start_pos;
    } bomb;

    // Hold the shot types of g_GameManager.run.shot_index, unfocused and
    // focused, from the add of the player to its removal.
    SHOT_TYPE shot_type, shot_type_focused;
} PLAYER;

extern PLAYER g_Player;

// Adds the update element (priority 8) and the draw elements (priorities
// 6 and 8), and resets g_Player. Adding loads both shot types of
// g_GameManager.run.shot_index, and fails when one does not load. It loads
// the sprites of the character, except in SCENE_NEXT_STAGE,
// SCENE_RESTART_STAGE and SCENE_NEXT_STAGE_AGAIN, which keep them. It then
// places the player at the bottom of the play area, respawning, which
// starts the Border when the cherry plus is full.
RESULT Player_Register(void);

// Removes the three elements and releases the shot types. Releases the
// sprites of the character, except in the scenes that keep them.
void Player_Remove(void);

// Returns the angle from pos to the player, or pi / 2 when they coincide.
float Player_AngleFrom(VEC3 pos);

// Tests the box of size centred on pos, an enemy bullet or an enemy.
// Returns PLAYER_CONTACT_CLEARED when the box touches a clear area, and
// sets clear_item to its item, else PLAYER_CONTACT_BOX when it touches the
// hitbox, else PLAYER_CONTACT_NONE. A touch of the hitbox marks
// REPLAY_EVENT_HIT, breaks a running Border, and kills the player when it
// is PLAYER_ALIVE, but returns PLAYER_CONTACT_BOX in every state.
PLAYER_CONTACT Player_CheckHit(VEC3 pos, VEC3 size);

// Is like Player_CheckHit, with the box grown by 20 on each side, and
// grazes instead of killing. The clear areas test the box before it grows.
// A touch of the hitbox returns PLAYER_CONTACT_NONE while the player is
// dying or respawning.
PLAYER_CONTACT Player_CheckGraze(VEC3 pos, VEC3 size);

// Tests a laser: the box of size centred on pos, turned by angle around
// origin. A touch of the hitbox marks REPLAY_EVENT_HIT, breaks a running
// Border, and kills the player when it is PLAYER_ALIVE. Otherwise, with
// can_graze, a box within 48 pixels of the hitbox grazes it, except while
// the player is dying or respawning.
void Player_CheckLaserHit(
    VEC3 pos, VEC3 size, VEC3 origin, float angle, bool can_graze);

// Returns whether the box of size centred on pos touches the collect box
// while the player is alive, invulnerable or in the Border.
bool Player_TouchesCollectBox(VEC3 pos, VEC3 size);

// Returns the damage that the shots and the damage boxes deal this frame
// to the box of size centred on pos, an enemy, and nothing before the state
// timer has ticked, which leaves *hit_by_bomb unchanged. Each shot that
// touches the box deals its damage, or a third of it (at least 1) during a
// bomb; a hit behaviour may skip the hit. Lasers deal it on even frames and
// never stop; piercing shots keep hitting at full speed; other shots stop,
// slowed to an eighth. Every 4th hit of a damage box bursts a spark. Sets
// *hit_by_bomb to whether a damage box touched it while a bomb runs.
int32_t Player_CalcDamage(VEC3 pos, VEC3 size, bool *hit_by_bomb);

// Starts the Border for 540 frames. During a bomb, a dialogue, a respawn,
// invulnerability, or a death past the bomb window, it is left pending
// instead; a death within the window breaks it at once.
void Player_StartBorder(void);

// Ends the Border: adds 10000 to the cherry and its maximum, scores 10
// times the cherry, and makes the player invulnerable for 40 frames.
void Player_EndBorder(void);

// Sets the first free clear area, else the last, to a width by height box
// at pos that lasts until the next update, and returns it.
PLAYER_CLEAR_AREA *Player_AddClearArea(
    VEC3 pos, float width, float height, ITEM_TYPE item);

// Is like Player_AddClearArea for a circle of radius that grows by growth
// each frame for frames frames.
PLAYER_CLEAR_AREA *Player_AddClearCircle(
    VEC3 pos, float radius, float growth, int32_t frames, ITEM_TYPE item);
