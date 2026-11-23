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

// Shares these source globals with the gameplay owner, bomb and miss
// consumers. speed uses units of 1/40 pixel, normally -15 through 20.
// destroy_score_delta accumulates in unsigned 16 bits; HUD consumes it.
typedef struct {
    int16_t speed;
    uint16_t destroy_score_delta;
} PELLET_SHARED;

typedef struct {
    PELLET slots[PELLETS_COUNT];
    int16_t alive_count_excluding_clouds_after_reset;
    bool interlace_field, interlace, spawn_with_cloud;
} PELLETS;

typedef struct {
    const PELLET_ASSETS *assets;
    PLAYER *player;
    ORB *orb;
    SHOTS *shots;
    uint32_t *rng;
    PELLET_SHARED *shared;
    SHOTS_OUTPUT shots_output;
} PELLET_CONTEXT;

// Zero-initialize PELLETS and PELLET_SHARED once. Keep slot storage across
// resets: sling direction, angle and unused motion fields survive reuse.
// Reset leaves pending delay clouds alive and sets the count to zero.
// This count can be negative and does not report pool occupancy.
void Pellets_ResetMoving(PELLETS *pellets);
void Pellets_EraseAndResetMoving(PELLETS *pellets);

// Spawns only into actually free slots. No RNG draw occurs for a full pool.
// Group coordinates must be in [-8,639] by [56,400]. Invalid groups are
// rejected: the source otherwise reads uninitialized local velocities.
// Single spawns do not apply the group bounds. Speeds/positions are Q12.4;
// arguments left/top and spin_center are integer pixels. Rank tuning uses
// 0,6,12,18 subpixels for Easy, Normal, Hard, Lunatic (also invalid ranks).
void Pellets_AddGroup(
    PELLETS *pellets, PELLET_SHARED *shared, const PLAYER *player,
    uint32_t *rng, int32_t rank, int16_t left, int16_t top, PELLET_GROUP group,
    int16_t speed);
void Pellets_AddSingle(
    PELLETS *pellets, const PELLET_SHARED *shared, int32_t rank, int16_t left,
    int16_t top, uint8_t angle, int16_t speed_base, PELLET_MOTION motion,
    int16_t speed_for_motion, int16_t spin_center_x, int16_t spin_center_y);

// Slows each moving, non-decaying pellet, starts decay and awards 10.
// Pending clouds are excluded. Overflow of the shared score wraps.
void Pellets_Decay(PELLETS *pellets, PELLET_SHARED *shared);
void Pellets_SpeedLower(PELLET_SHARED *shared, int16_t max, int16_t delta);
void Pellets_SpeedRaise(PELLET_SHARED *shared, int16_t delta);

// Runs clouds, all pellet motion and shot collision/clipping, then calls
// real Shots_UpdateAndRender exactly once, then draws pellets and tests
// Orb/deflection and lethal player hits. Every moving pellet ages even when
// interlace skips its drawing and Orb/deflection tests. The unused top-bounce
// motion preserves the source's mixed-unit comparison.
// Allocates nothing and uses only supplied state and shared RNG. Execute as
// the stage Orb composite's pellets_update operation after its wait; never
// update shots separately. Every reached shot callback is required and
// synchronous, and must not modify pellet, player, Orb or shot state.
// Requires immutable loaded masks. Gameplay scene wiring remains external.
void Pellets_UpdateAndRender(PELLETS *pellets, const PELLET_CONTEXT *context);
