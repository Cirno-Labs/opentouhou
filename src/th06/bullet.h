#pragma once

#include "th06/anm.h"
#include "th06/timer.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include <stdint.h>

// Runs the enemy bullets and lasers: fires the patterns of enemy scripts,
// moves and turns the bullets, and tests both against the player
// (player.h). Each update runs the items first (item.h). The sprite
// scripts of a bullet are part of the simulation: its size comes from its
// sprite, and its spawn and fade states end with their scripts. Drawing
// comes with graphics.

#define BULLET_COUNT 0x280
#define LASER_COUNT 0x40
#define BULLET_TYPE_COUNT 10
#define BULLET_SPAWN_SPRITE_COUNT 3

// Selects how Bullet_FirePattern aims the bullets of a pattern. Enemy
// scripts select a mode by number (opcodes 67 to 75), so the numbers are
// fixed. A fan spreads its columns by angle2 around angle1. A ring spaces
// its columns evenly around a circle from angle1 and turns each row by
// angle2. Each row is slower than the row before it, from speed1 towards
// speed2.
typedef enum {
    BULLET_AIM_FAN_AT_PLAYER = 0, // also aims a laser at the player
    BULLET_AIM_FAN = 1,
    BULLET_AIM_RING_AT_PLAYER = 2,
    BULLET_AIM_RING = 3,
    BULLET_AIM_RING_OFFSET_AT_PLAYER = 4, // turned by half a column
    BULLET_AIM_RING_OFFSET = 5,
    BULLET_AIM_RANDOM_ANGLE = 6, // between angle2 and angle1
    BULLET_AIM_RING_RANDOM_SPEED = 7, // between speed2 and speed1
    BULLET_AIM_RANDOM = 8, // a random angle and speed, as in 6 and 7
} BULLET_AIM;

// Selects the extra behaviours of a bullet or laser, one bit each. A
// bullet runs at most one of START_FAST, ACCELERATE and CURVE, tested in
// that order. It runs at most one of the turns and bounces, tested in the
// order TURN, TURN_TO, TURN_AT_PLAYER, BOUNCE, BOUNCE_NO_BOTTOM. The
// ex_floats and ex_ints of the shooter hold their values.
typedef enum {
    // Starts 5 faster and slows to its speed over 16 frames. A laser with
    // this flag fades in over its start instead of widening, and fades in
    // again over the same frames as it ends instead of narrowing.
    BULLET_FLAG_START_FAST = 0x1,
    // Plays spawn sprite 1, 2 or 3 first, at 1/2, 1/2.5 or 1/3 of the
    // velocity.
    BULLET_FLAG_SPAWN_1 = 0x2,
    BULLET_FLAG_SPAWN_2 = 0x4,
    BULLET_FLAG_SPAWN_3 = 0x8,
    // Adds ex_floats[0] along angle ex_floats[1] to the velocity for
    // ex_ints[0] frames. An angle of -999 or less is the angle of the
    // bullet. ex_ints[0] of 0 or less means 99999 frames.
    BULLET_FLAG_ACCELERATE = 0x10,
    // Adds ex_floats[0] to the speed and ex_floats[1] to the angle each
    // frame, for ex_ints[0] frames.
    BULLET_FLAG_CURVE = 0x20,
    // Turns by ex_floats[0] every ex_ints[0] frames, ex_ints[1] times, and
    // then takes speed ex_floats[1], or keeps its own when that is
    // negative. The speed falls towards 0 between two turns.
    BULLET_FLAG_TURN = 0x40,
    // Is like BULLET_FLAG_TURN, but aims at the player turned by
    // ex_floats[0].
    BULLET_FLAG_TURN_AT_PLAYER = 0x80,
    // Is like BULLET_FLAG_TURN, but sets the angle to ex_floats[0].
    BULLET_FLAG_TURN_TO = 0x100,
    // Posts the sound of the shooter when the pattern fires.
    BULLET_FLAG_SOUND = 0x200,
    // Bounces off the edges of the play area ex_ints[0] times and then
    // takes speed ex_floats[0], or keeps its own when that is negative.
    BULLET_FLAG_BOUNCE = 0x400,
    // Is like BULLET_FLAG_BOUNCE, but the bottom edge does not turn the
    // bullet. Each frame below it still counts as a bounce.
    BULLET_FLAG_BOUNCE_NO_BOTTOM = 0x800,
} BULLET_FLAG;

typedef enum {
    BULLET_FREE = 0,
    BULLET_FLYING = 1,
    BULLET_SPAWNING_1 = 2, // with BULLET_FLAG_SPAWN_1, and so on
    BULLET_SPAWNING_2 = 3,
    BULLET_SPAWNING_3 = 4,
    BULLET_FADING = 5, // freed when the fade script ends
} BULLET_STATE;

// Holds the sprites of a bullet type and the hitbox they give it.
typedef struct {
    ANM_VM flying;
    ANM_VM spawning[BULLET_SPAWN_SPRITE_COUNT];
    ANM_VM fading;
    VEC3 hitbox_size;
} BULLET_SPRITES;

typedef struct {
    // Holds a copy of the sprites of the bullet type, set to its color.
    BULLET_SPRITES sprites;
    VEC3 pos;
    VEC3 velocity;
    float speed;
    float angle;

    // Holds the values of BULLET_FLAG_ACCELERATE and BULLET_FLAG_CURVE.
    struct {
        VEC3 accel;
        float speed;
        float angle;
        int32_t frames;
    } change;

    // Holds the values of the turn and bounce flags. A bounce uses only
    // speed, count and max.
    struct {
        float angle;
        float speed;
        int32_t interval;
        int32_t count;
        int32_t max;
    } turn;

    TIMER timer;
    uint16_t flags; // BULLET_FLAG; the low 16 bits of the shooter's flags
    int16_t color;
    BULLET_STATE state;
    // Counts the frames that a turning or bouncing bullet spends outside
    // the play area. At 0x100 the bullet is freed.
    uint16_t out_of_bounds_frames;
    bool grazed; // once set, the bullet is tested only for hits
} BULLET;

// Holds what a pattern fires: rows rows of columns bullets each. Aim
// gives angle1, angle2, speed1 and speed2 their meaning (see BULLET_AIM).
// A pattern made on the fly starts zeroed.
typedef struct {
    int16_t type;
    int16_t color;
    VEC3 pos;
    float angle1, angle2;
    float speed1, speed2;
    // Hold the values of the flags (see BULLET_FLAG). For a laser,
    // ex_floats hold the start offset, the end offset, the maximum length
    // and the width; ex_ints hold the frames of the start, of the firing
    // and of the end, and the frame of the start at which it starts to
    // hit.
    float ex_floats[4];
    int32_t ex_ints[4];
    uint32_t laser_end_hit_frames;
    int16_t columns;
    int16_t rows;
    BULLET_AIM aim;
    int32_t flags; // BULLET_FLAG
    int32_t sound; // posted with BULLET_FLAG_SOUND
} BULLET_SHOOTER;

typedef enum {
    LASER_STARTING = 0,
    LASER_FIRING = 1,
    LASER_ENDING = 2,
} LASER_STATE;

// Holds a beam from pos along angle, between start_offset and end_offset.
// Each frame the end moves out by speed. Once the beam is max_length
// long, the start follows.
typedef struct {
    ANM_VM beam; // only drawing reads it
    ANM_VM origin; // the glow at the start; only drawing reads it
    VEC3 pos;
    float angle;
    float start_offset, end_offset;
    float max_length;
    float width;
    float speed;
    int32_t start_frames;
    int32_t hit_start_frame; // the frame of LASER_STARTING that starts to hit
    int32_t duration_frames; // the length of LASER_FIRING
    // Holds the length of LASER_ENDING. With 0, the laser ends after it
    // fires.
    int32_t end_frames;
    int32_t end_hit_frames; // how long the ending beam still hits
    bool active;
    TIMER timer;
    uint16_t flags; // BULLET_FLAG
    LASER_STATE state;
} LASER;

typedef struct {
    BULLET bullets[BULLET_COUNT];
    LASER lasers[LASER_COUNT];
    int32_t next_bullet; // where a pattern looks for a free bullet first
} BULLET_MANAGER;

extern BULLET_MANAGER g_Bullets;

// Adds the update element (priority 11) and the draw element (priority
// 9), and resets g_Bullets. Adding loads the bullet sprites, except in
// SCENE_NEXT_STAGE, which keeps them. It then sets the bullet types up
// from the sprites and removes every item.
RESULT Bullet_Register(void);
// Removes both elements. Releases the bullet sprites, except in
// SCENE_NEXT_STAGE.
void Bullet_Remove(void);

// Fires the pattern of shooter into free bullets, aimed at the player in
// the modes that aim. Stops at the first bullet that finds no free slot.
void Bullet_FirePattern(const BULLET_SHOOTER *shooter);
// Starts a laser from shooter in the first free slot. Returns the laser,
// or a spare laser that never runs when no slot is free.
LASER *Bullet_SpawnLaser(const BULLET_SHOOTER *shooter);

// Makes every bullet fade out and every laser end.
void Bullet_ClearAll(void);
// Turns every bullet that is not fading into a star item (ITEM_STAR).
// Ends every laser, and drops a star item every 32 pixels along each
// laser that was not ending yet.
void Bullet_ClearAllToItems(void);
// Makes every bullet fade out and every laser end, and scores each
// bullet, fading ones too: 2000 for the first, then 10 more for each next
// one, capped at max_bonus from the second one on.
// Adds the total to the score and shows it with Gui_ShowBonusPopup. With
// to_items, also drops the star items of Bullet_ClearAllToItems, plus one
// at the position of each laser.
void Bullet_ClearAllWithBonus(int32_t max_bonus, bool to_items);
