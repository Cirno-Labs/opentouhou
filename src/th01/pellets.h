#pragma once

#include "pc98/zun/math16.h"
#include "th01/pellet_assets.h"
#include "th01/player.h"

#define PELLETS_COUNT 100

typedef enum {
    PELLET_REGULAR = 0,
    PELLET_GRAVITY,
    PELLET_SLING_AIMED,
    PELLET_BOUNCE_FROM_TOP_THEN_GRAVITY,
    PELLET_FALL_STRAIGHT_FROM_TOP_THEN_REGULAR,
    PELLET_SPIN,
    PELLET_CHASE,
} PELLET_MOTION;

typedef enum {
    PELLET_GROUP_SINGLE = 1,
    PELLET_GROUP_SINGLE_AIMED = 12,
    PELLET_GROUP_TWO_WIDE,
    PELLET_GROUP_TWO_NARROW,
    PELLET_GROUP_THREE_WIDE,
    PELLET_GROUP_THREE_NARROW,
    PELLET_GROUP_FOUR_WIDE,
    PELLET_GROUP_FOUR_NARROW,
    PELLET_GROUP_FIVE_WIDE,
    PELLET_GROUP_FIVE_NARROW,
    PELLET_GROUP_TWO_WIDE_AIMED,
    PELLET_GROUP_TWO_NARROW_AIMED,
    PELLET_GROUP_THREE_WIDE_AIMED,
    PELLET_GROUP_THREE_NARROW_AIMED,
    PELLET_GROUP_FOUR_WIDE_AIMED,
    PELLET_GROUP_FOUR_NARROW_AIMED,
    PELLET_GROUP_FIVE_WIDE_AIMED,
    PELLET_GROUP_FIVE_NARROW_AIMED,
    PELLET_GROUP_RANDOM_NARROW_AIMED,
    PELLET_GROUP_RANDOM_WIDE,
} PELLET_GROUP;

typedef struct {
    bool moving, not_rendered;
    uint8_t motion_type;
    int16_t left, top, previous_left, previous_top, from_group;
    MATH16_VECTOR spin_center, velocity, spin_velocity;
    int16_t age, speed, decay_frame, cloud_frame, cloud_left, cloud_top;
    int16_t angle, sling_direction;
} PELLET;

typedef struct {
    PELLET slots[PELLETS_COUNT];
    int16_t alive_count_excluding_clouds_after_reset;
    // Uses 1/40 pixel units, normally -15 through 20.
    int16_t speed;
    // Accumulates destroyed-pellet score in unsigned 16 bits until committed.
    uint16_t destroy_score_delta;
    bool interlace_field, interlace, spawn_with_cloud;
} PELLETS;

extern PELLETS g_Pellets;

// Zeroes the pellets and their shared speed for a new world.
void Pellets_Init(void);

// Removes moving pellets and sets the count to zero; delay clouds survive.
// The count can go negative and does not report pool occupancy.
void Pellets_ResetMoving(void);
void Pellets_EraseAndResetMoving(void);

// Spawns a group into free slots, in Q12.4 speeds and integer-pixel positions.
// Rejects groups outside [-8,639] by [56,400], whose velocities the original
// reads uninitialized.
void Pellets_AddGroup(
    int32_t rank, int16_t left, int16_t top, PELLET_GROUP group, int16_t speed);
void Pellets_AddSingle(
    int32_t rank, int16_t left, int16_t top, uint8_t angle, int16_t speed_base,
    PELLET_MOTION motion, int16_t speed_for_motion, int16_t spin_center_x,
    int16_t spin_center_y);

// Slows each moving, non-decaying pellet, starts decay and awards 10.
// Pending clouds are excluded. Overflow of the shared score wraps.
void Pellets_Decay(void);
void Pellets_SpeedLower(int16_t max, int16_t delta);
void Pellets_SpeedRaise(int16_t delta);

// Draws and tests every other moving pellet per frame, alternating, until
// Pellets_Init.
void Pellets_Interlace(void);

// Moves pellets, updates shots once, then draws pellets and tests the Orb
// and player. Run it as the Orb composite's pellets_update, after its wait;
// never update shots separately.
void Pellets_UpdateAndRender(
    const PELLET_ASSETS *assets, const PTN *stage_sprites);
