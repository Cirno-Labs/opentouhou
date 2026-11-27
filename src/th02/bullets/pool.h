#pragma once

#include "th02/entity.h"
#include "th02/patterns.h"

#include "core/vfs.h"

#define BULLET_COUNT 150

typedef enum {
    BULLET_PELLET = 1,
    BULLET_SPRITE,
} BULLET_KIND;

typedef enum {
    BULLET_MOTION_CHASE = 0x80,
    BULLET_MOTION_HOMING,
    BULLET_MOTION_SLOW_THEN_AIM,
    BULLET_MOTION_GRAVITY,
    BULLET_MOTION_DRIFT,
    BULLET_MOTION_DRIFT_CHASE,
    BULLET_MOTION_BOUNCE = 0x87,
    BULLET_MOTION_BOUNCE_VERTICAL,
    BULLET_MOTION_SINGLE = 0xff,
} BULLET_MOTION;

// The spawn pattern given to Bullets_AddPellets, or the sprite motion given
// to Bullets_AddSprites for motion<0x80.
typedef enum {
    BULLET_FAN2 = 1,
    BULLET_FAN2_MEDIUM,
    BULLET_FAN2_WIDE,
    BULLET_FAN2_AIMED,
    BULLET_FAN2_MEDIUM_AIMED,
    BULLET_FAN2_WIDE_AIMED,
    BULLET_FAN3,
    BULLET_FAN3_MEDIUM,
    BULLET_FAN3_WIDE,
    BULLET_FAN3_AIMED,
    BULLET_FAN3_MEDIUM_AIMED,
    BULLET_FAN3_WIDE_AIMED,
    BULLET_FAN4,
    BULLET_FAN4_MEDIUM,
    BULLET_FAN4_WIDE,
    BULLET_FAN4_AIMED,
    BULLET_FAN4_MEDIUM_AIMED,
    BULLET_FAN4_WIDE_AIMED,
    BULLET_FAN5,
    BULLET_FAN5_MEDIUM,
    BULLET_FAN5_WIDE,
    BULLET_FAN5_AIMED,
    BULLET_FAN5_MEDIUM_AIMED,
    BULLET_FAN5_WIDE_AIMED,
    BULLET_AIMED,
    BULLET_RANDOM_ONE,
    BULLET_RANDOM,
    BULLET_RANDOM_SPEED,
    BULLET_SINGLE = 0x20,
    BULLET_RING4,
    BULLET_RING2,
    BULLET_MIRROR,
    BULLET_RING8,
    BULLET_RING16,
    BULLET_FAN2_EXTRA_WIDE_AIMED,
    BULLET_RING32,
} BULLET_PATTERN;

typedef struct {
    uint8_t state;
    int8_t kind;
    ENTITY_POINT position[2];
    ENTITY_POINT velocity;
    uint8_t sprite;
    uint8_t motion;
    uint8_t angle;
    uint8_t speed;
    uint8_t phase;
    uint8_t reserved;
} BULLET;

typedef struct {
    BULLET slots[BULLET_COUNT];
    int16_t motion_a;
    int16_t motion_b;
    int16_t turn_limit;
} BULLETS;

extern const BULLETS *const g_Bullets;

typedef struct {
    uint16_t rows[8][8];
} BULLET_IMAGES;

// Loads the eight pre-shifted pellet masks.
RESULT BulletImages_Load(VFS *files, BULLET_IMAGES *out);

// Clears the bullet pool and spawn state, once per MAIN process.
void Bullets_Init(void);

// Frees every slot at stage start, drawing two random values per spark.
// Keeps slot history, the spawn's slow_cycle and the RNG.
void Bullets_ResetStage(int8_t rank, bool reduce_effects);

// Fires pellets of a pattern aimed at player+(12,20); as in retail, a full
// pool skips the pattern's RNG but still tunes speed.
void Bullets_AddPellets(
    uint8_t back, int16_t left, int16_t top, uint8_t angle, uint8_t pattern,
    int16_t speed);

// Fires sprite bullets, one un-aimed per layer for motion>=0x80; only
// gravity skips speed tuning.
void Bullets_AddSprites(
    uint8_t back, int16_t left, int16_t top, uint8_t angle, uint8_t motion,
    int16_t image, int16_t speed);

// Invalidates the tiles under the bullets, then frees removed slots; as in
// retail, negative tops alias other tiles.
void Bullets_Invalidate(PLAYFIELD_PAGES pages);

// Invalidates, then marks every bullet removed; a repeated call frees the
// previous call's removals.
void Bullets_RemoveAll(PLAYFIELD_PAGES pages);

// Moves and draws the bullets; as in retail, hits set player.hit without
// testing protection or bombing.
void Bullets_Update(
    PLAYFIELD_PAGES pages, uint32_t frame, const PATTERN_BANK *patterns,
    const BULLET_IMAGES *images);

// Sets the BULLET_MOTION_DRIFT motion and drift-chase turn limit.
void Bullets_SetMotion(int16_t motion_a, int16_t motion_b);

// Sets the turn limit used by homing and drift-chase bullets.
void Bullets_SetTurnLimit(int16_t turn_limit);
