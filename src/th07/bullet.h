#pragma once

#include "th07/anm.h"
#include "th07/item.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include "stg/timer.h"

#include <stdint.h>

// Runs the enemy bullets and lasers: fires the patterns of enemy scripts,
// moves and turns the bullets, and tests both against the player
// (player.h). Each update runs the items first (item.h). The sprite
// scripts of a bullet are part of the simulation: its size comes from its
// sprite, and its spawn and fade states end with their scripts. Drawing
// records sprites with a separate presentation lifetime for each spawn.
// Each update walks bullet 0 first, then the rest from the last down.
// Bullets are drawn by size class, each class in the reverse of that walk.

#define BULLET_COUNT 0x400
#define LASER_COUNT 0x40
#define BULLET_TYPE_COUNT 11
#define BULLET_SPAWN_SPRITE_COUNT 3
#define BULLET_COMMAND_COUNT 5
#define BULLET_SIZE_CLASS_COUNT 6

// Selects how Bullet_FirePattern aims the bullets of a pattern. Enemy
// scripts select a mode by number (opcodes 64 to 72), so the numbers are
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

// Selects the extra behaviors of a bullet, one bit each. A bullet runs
// only the commands whose flag it has; they start in order (see
// BULLET_COMMAND). The running behaviors all run each frame, in the
// order START_FAST, ACCELERATE, CURVE, TURN, TURN_TO, TURN_AT_PLAYER, and
// then the bounces. The fields of the command hold their values.
typedef enum {
    // Starts 5 faster and slows to its speed over 16 frames.
    BULLET_FLAG_START_FAST = 0x1,
    // Plays spawn sprite 1, 2 or 3 first, at 1/2, 1/2.5 or 1/3 of the
    // velocity.
    BULLET_FLAG_SPAWN_1 = 0x2,
    BULLET_FLAG_SPAWN_2 = 0x4,
    BULLET_FLAG_SPAWN_3 = 0x8,
    // Adds args[0] along angle args[1] to the velocity for `frames` frames.
    // An angle of -990 or less is the angle of the bullet. Plays the sound
    // of the bullet as it starts, unless it is the first command.
    BULLET_FLAG_ACCELERATE = 0x10,
    // Adds args[0] to the speed and args[1] to the angle each frame, for
    // `frames` frames. Plays the sound as BULLET_FLAG_ACCELERATE does.
    BULLET_FLAG_CURVE = 0x20,
    // Turns by args[0] every `frames` frames, `count` times, each time
    // taking speed args[1], or the speed it started with when that is -999
    // or less. The speed falls towards 0 between two turns. Each turn plays
    // the sound of the bullet.
    BULLET_FLAG_TURN = 0x40,
    // Is like BULLET_FLAG_TURN, but aims at the player turned by args[0].
    BULLET_FLAG_TURN_AT_PLAYER = 0x80,
    // Is like BULLET_FLAG_TURN, but sets the angle to args[0].
    BULLET_FLAG_TURN_TO = 0x100,
    // Posts the sound of the shooter when the pattern fires.
    BULLET_FLAG_SOUND = 0x200,
    // Bounces off the edges of the play area `frames` times, taking speed
    // args[0], or keeping its own when that is negative. Every frame
    // outside counts as one bounce, and each bounce plays the sound of the
    // bullet.
    BULLET_FLAG_BOUNCE = 0x400,
    // Is like BULLET_FLAG_BOUNCE, but the bottom edge does not turn the
    // bullet.
    BULLET_FLAG_BOUNCE_NO_BOTTOM = 0x800,
    // Is not cleared by a clear area, and does not fade when fired in the
    // 10 frames after a clear; the Bullet_Clear* calls still clear it.
    BULLET_FLAG_RESIST_CLEAR = 0x1000,
    // Command only: skips the play area test for `frames` frames.
    BULLET_FLAG_STAY_OUTSIDE = 0x2000,
} BULLET_FLAG;

// Selects the extra behaviors of a laser, from the shooter's flags.
typedef enum {
    // Fades in over its start and again over its end, instead of
    // widening and narrowing.
    LASER_FLAG_FADE = 0x1,
    // Is kept by Bullet_ClearAll and Bullet_ClearAllToItems, and spawns
    // in the 10 frames after a clear.
    LASER_FLAG_RESIST_CLEAR = 0x4,
} LASER_FLAG;

// Holds one step of a bullet's behavior, started when the steps before
// it have started. A step that is not concurrent waits until no
// behavior runs. A flag of 0 ends the steps.
typedef struct {
    float args[2];
    int32_t frames;
    int32_t count; // the turns of BULLET_FLAG_TURN*
    uint32_t flag; // one BULLET_FLAG
    bool concurrent;
} BULLET_COMMAND;

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
    // Picks the draw pass: 5 up to 8 high, 4 or 3 up to 16, 2 or 1 up to
    // 32, 0 above.
    uint8_t size_class;
} BULLET_SPRITES;

typedef struct BULLET {
    // Holds a copy of the sprites of the bullet type, set to its color.
    BULLET_SPRITES sprites;
    VEC3 pos;
    VEC3 velocity;
    float speed;
    float angle;
    TIMER timer;
    // Grazes once it reaches 16; a spawning bullet holds it back.
    TIMER graze_timer;
    int32_t keep_outside_frames;
    uint16_t running; // the BULLET_FLAG of the behaviors that run
    uint16_t flags; // BULLET_FLAG; the low 16 bits of the shooter's flags
    int16_t color;
    BULLET_STATE state;
    // Counts up while a turning or bouncing bullet is outside, freed at
    // 0x80; otherwise counts down outside and frees at 0.
    uint16_t out_of_bounds_frames;
    bool grazed; // once set, the bullet is tested only for hits
    struct BULLET *next_drawn; // only drawing reads it
    int32_t pattern_state; // 0 when fired; the ECL patterns use it as they like
    int32_t sound; // played as a behavior starts or turns; -1 for none
    int32_t next_command;
    BULLET_COMMAND commands[BULLET_COMMAND_COUNT];

    struct {
        TIMER timer;
    } start_fast;

    struct {
        TIMER timer;
        VEC3 accel;
        int32_t frames;
    } accelerate;

    struct {
        TIMER timer;
        float speed;
        float angle;
        int32_t frames;
    } curve;

    struct {
        TIMER timer;
        float angle;
        float speed;
        int32_t frames;
        int32_t count;
        int32_t max;
    } turn;

    struct {
        float speed;
        int32_t count;
        int32_t max;
    } bounce;
} BULLET;

// Holds what a pattern fires: rows rows of columns bullets each. Aim
// gives angle1, angle2, speed1 and speed2 their meaning (see BULLET_AIM).
// A pattern made on the fly starts from BulletShooter_Init.
typedef struct {
    int16_t type;
    int16_t color;
    VEC3 pos;
    float angle1;
    float angle2;
    float speed1;
    float speed2;
    BULLET_COMMAND commands[BULLET_COMMAND_COUNT]; // copied into each bullet

    struct {
        float start_offset;
        float end_offset;
        float max_length;
        float width;
        int32_t start_frames;
        int32_t firing_frames;
        int32_t end_frames;
        int32_t hit_start_frame;
        int32_t end_hit_frames;
    } laser;

    int16_t columns;
    int16_t rows;
    BULLET_AIM aim;
    int32_t flags; // BULLET_FLAG
    int32_t sound; // posted with BULLET_FLAG_SOUND
    int32_t bullet_sound; // BULLET::sound; -1 for none
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
    float start_offset;
    float end_offset;
    float max_length;
    float width;
    // Holds the width its start has grown to; a clear ends it from there.
    float shown_width;
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
    uint16_t flags; // LASER_FLAG
    LASER_STATE state;
    bool hide_origin_while_starting; // only drawing reads it; ECL sets it
} LASER;

typedef struct {
    BULLET_SPRITES types[BULLET_TYPE_COUNT]; // ECL patterns copy type 5
    BULLET bullets[BULLET_COUNT];
    LASER lasers[LASER_COUNT];
    int32_t count; // the bullets in play at the last update
    int32_t clear_frames; // 10 after a clear, down to 0
    BULLET *draw_lists[BULLET_SIZE_CLASS_COUNT]; // only drawing reads them
    int32_t next_bullet; // where a pattern looks for a free bullet first
    // Selects what cleared bullets turn into: ITEM_STAR, or ITEM_BOSS_STAR
    // as the gui sets.
    ITEM_TYPE clear_item;
} BULLET_MANAGER;

extern BULLET_MANAGER g_Bullets;

// Adds the update element (priority 12) and the draw element (priority
// 10), and resets g_Bullets. Adding loads the bullet sprites, except in
// SCENE_NEXT_STAGE, which keeps them. It then sets the bullet types up from the
// sprites and removes every item.
RESULT Bullet_Register(void);

// Removes both elements and zeroes g_Bullets. Releases the bullet
// sprites, except in the scenes that keep them.
void Bullet_Remove(void);

// Fires the pattern of shooter into free bullets, aimed at the player in
// the modes that aim. Stops at the first bullet that finds no free slot,
// and posts the sound with BULLET_FLAG_SOUND. Does nothing, the sound
// included, while BULLET_COUNT bullets were in play at the last update.
void Bullet_FirePattern(const BULLET_SHOOTER *shooter);

// Starts a laser from shooter in the first free slot. Returns the laser,
// or a spare laser that never runs when no slot is free. In the 10 frames
// after a clear, a laser without LASER_FLAG_RESIST_CLEAR does not start,
// and the first laser is returned.
LASER *Bullet_SpawnLaser(const BULLET_SHOOTER *shooter);

// Makes every bullet fade out, and every laser without
// LASER_FLAG_RESIST_CLEAR end.
void Bullet_ClearAll(void);

// Is like Bullet_ClearAll, but every laser ends.
void Bullet_ClearAllAndKeptLasers(void);

// Turns every bullet that is not fading into a clear item that flies to
// the player. Ends every laser without LASER_FLAG_RESIST_CLEAR, and drops
// a clear item every 32 pixels along each that was not ending yet.
void Bullet_ClearAllToItems(void);

// Makes every bullet fade out and every laser end. Each bullet, fading
// ones too, drops a clear item and scores a bonus shown as a popup: 2000
// for the first, then 20 more for each next one, capped at max_bonus
// from the second one on. With to_items, each laser that was not ending
// yet drops a clear item at its position and every 32 pixels along it.
// Returns the total, which the caller adds to the score.
int32_t Bullet_ClearAllWithBonus(int32_t max_bonus, bool to_items);

// Turns every bullet within radius of pos that is not fading into an
// ITEM_STAR that flies to the player.
void Bullet_ClearCircleToItems(VEC3 pos, float radius);

// Stops every bullet where it is and turns it to color 0. Its behaviors
// keep running.
void Bullet_StopAll(void);

// Returns the bullets in play at the last update.
int32_t Bullet_Count(void);

// Zeroes shooter, with no bullet sound.
void BulletShooter_Init(BULLET_SHOOTER *shooter);

// Sets command index of shooter and adds its flag to the flags.
void BulletShooter_SetCommand(
    BULLET_SHOOTER *shooter, int32_t index, BULLET_COMMAND command);

// Sets command index of bullet and adds its flag to the flags. The
// commands start over from the first.
void Bullet_SetCommand(BULLET *bullet, int32_t index, BULLET_COMMAND command);
