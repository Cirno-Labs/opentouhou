#pragma once

#include "th02/boss/meira.h"
#include "th02/entity.h"

#include "core/base/types.h"

#define MEIRA_FLOWER_COUNT 40
#define MEIRA_TRAIL_COUNT 3

// Stage 2's midboss, sharing Meira's fight state.
typedef struct {
    int16_t kill_frame;
    bool active;
} MEIRA_MIDBOSS;

// One of Meira's scattered flower bullets, which linger and fire on their
// own timer.
typedef struct {
    uint8_t flag;
    uint8_t group;
    ENTITY_POINT position;
    uint8_t angle;
    uint8_t frame;
    uint8_t speed;
    uint8_t sprite;
    uint8_t frames_left;
    uint8_t reserved;
} MEIRA_FLOWER;

// Meira's current attack phase.
typedef enum {
    MEIRA_OPENING,
    MEIRA_PRESSURE,
    MEIRA_BILLIARDS,
} MEIRA_PHASE;

// Stage 2 boss Meira's own state: her dash trail, scattered flowers and
// which fan/burst patterns the current phase fires.
typedef struct {
    MEIRA_FLOWER flowers[MEIRA_FLOWER_COUNT];
    ENTITY_POINT trail[2][MEIRA_TRAIL_COUNT];
    ENTITY_POINT origin;
    ENTITY_POINT target;
    int16_t kill_frame;
    int16_t flower_frames;
    int16_t speed[2];
    int16_t step;
    uint8_t flower_sprite;
    uint8_t flower_fire_i;
    uint8_t flower_group;
    uint8_t flower_speed;
    MEIRA_PHASE phase;
    uint8_t direction;
    uint8_t pattern;
    uint8_t spread_pattern;
    uint8_t fan_pattern;
    uint8_t burst_pattern;
    bool trail_active;
} MEIRA;

// Writable view of the module's private state, for tests.
extern MEIRA *const p_Meira;
extern MEIRA_MIDBOSS *const p_MeiraMidboss;

// Places Meira at her entrance, before the first Meira_Begin.
void Meira_Place(BOSS_STATE *boss);

// Starts Meira's fight: resets damage, trail and her fire patterns for
// rank.
void Meira_Begin(BOSS_STATE *boss, uint8_t rank);

// Advances Meira one frame: moves, fires flowers and checks for defeat.
BOSS_RESULT Meira_Update(
    BOSS_STATE *boss, const PATTERN_BANK *patterns, uint8_t rank);

// Restores the scrolled-out tile rows Meira's sprite and trail marked
// dirty.
void Meira_Invalidate(BOSS_STATE *boss);

// Advances the stage 2 midboss one frame.
void Meira_MidbossUpdate(BOSS_STATE *boss, const PATTERN_BANK *patterns);

// Restores the midboss's scrolled-out tile row; reports whether it is
// still active.
bool Meira_MidbossInvalidate(BOSS_STATE *boss);
