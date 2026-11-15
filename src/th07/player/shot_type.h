#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"

#include <stdint.h>

// Reads th07's shot type files (.sht): a shot type's speeds, boxes, bombs
// and item reach, and its shots by power level. Each shot type has one file
// for unfocused and one for focused play; the focused file gives only its
// shots.

typedef enum {
    SHOT_FIRE_PLAIN = 0, // on frames where frame % interval is phase
    SHOT_FIRE_LASER = 2, // from an option while the options are out
    SHOT_FIRE_TRAIL_LASER = 3, // from the player while the options are in
    SHOT_FIRE_HOMING = 4, // at the aim target, at 1.5 times its speed
    SHOT_FIRE_AIMED = 5, // turned by the aim angle
} SHOT_FIRE;

typedef enum {
    SHOT_MOVE_NONE = 0,
    // Steers to the homing target for 40 frames at up to 10, or speeds up
    // to 10 without one.
    SHOT_MOVE_HOMING = 1,
    SHOT_MOVE_HOMING_FAST = 2, // as SHOT_MOVE_HOMING, up to 18
    SHOT_MOVE_RISING = 3, // speeds up upwards by a random 0.27 to 0.37
    SHOT_MOVE_LASER = 4,
    SHOT_MOVE_TRAIL_LASER = 5,
} SHOT_MOVE;

typedef enum {
    SHOT_DRAW_PLAIN = 0,
    SHOT_DRAW_TRAIL = 1,
} SHOT_DRAW;

typedef enum {
    SHOT_HIT_PLAIN = 0,
    // Grows and drifts up on its first hit, then hits every other frame, a
    // third as hard and 12% slower each time.
    SHOT_HIT_LINGER = 1,
    SHOT_HIT_SPARK = 2, // bursts an effect every 8 hits
} SHOT_HIT;

typedef enum {
    SHOT_KIND_PLAIN = 0, // kinds 1 and 2 of the file act as 0
    SHOT_KIND_PIERCING = 3,
    SHOT_KIND_LASER = 4,
    SHOT_KIND_TRAIL_LASER = 5,
} SHOT_KIND;

// Holds one shot of a power level. A laser keeps its frames (a trail laser
// its trail length) in interval, and its entry of the lasers in phase.
typedef struct {
    int16_t interval, phase;
    float offset_x, offset_y; // from the player, or from the option
    float size_x, size_y;
    float angle;
    float speed;
    int16_t damage;
    uint8_t option; // 0 for the player, else the option from 1
    SHOT_KIND kind;
    int16_t script;
    int16_t sound; // queued when the shot fires, or -1
    SHOT_FIRE fire;
    SHOT_MOVE move;
    SHOT_DRAW draw;
    SHOT_HIT hit;
} SHOT;

// Holds the shots of a power level, which lasts while the power is below
// max_power.
typedef struct {
    int32_t max_power;
    int32_t count;
    SHOT *shots;
} SHOT_TABLE;

typedef struct {
    float bombs; // a life starts with them
    int32_t bomb_window; // frames to bomb after a hit
    float hitbox_size, graze_size;
    float item_pull_speed;
    float item_collect_size;
    float death_cherry_share; // of the cherry that a death takes
    float item_line; // above it, the player draws the items in
    float speed, speed_focused, speed_diagonal, speed_diagonal_focused;
    int32_t table_count;
    SHOT_TABLE *tables;
} SHOT_TYPE;

// Reads a shot type file into *out, which ShotType_Release frees. Fails on
// an offset outside the file, a table without shots or without its closing
// shot (interval < 0), an index outside its enum, an option above 2, a laser
// entry above 2, a trail longer than 16, a plain interval of 0, a laser move
// without a laser fire from an option, or no table past full power (128);
// *out is then empty.
RESULT ShotType_Read(READER *reader, SHOT_TYPE *out);

// Reads the shot type file at path through th07/files.
RESULT ShotType_Load(const char *path, SHOT_TYPE *out);

// Frees the tables of shot_type and leaves it empty.
void ShotType_Release(SHOT_TYPE *shot_type);
