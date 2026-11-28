#pragma once

#include "th02/background.h"
#include "th02/boss/stones.h"
#include "th02/entity.h"

#include "core/base/types.h"

#define STONE_COUNT 5
#define STONE_MIDBOSS_COUNT 2

// A stone slot's position in the circle: the two inner stones open first,
// then the two outer, then the lone north stone.
typedef enum {
    STONE_INNER_WEST,
    STONE_INNER_EAST,
    STONE_OUTER_WEST,
    STONE_OUTER_EAST,
    STONE_NORTH,
} STONE_INDEX;

// A stone's life cycle: dormant until the fight reaches it, active while
// it fires, then killed and removed one frame apart.
typedef enum {
    STONE_DORMANT,
    STONE_ACTIVE,
    STONE_KILL,
    STONE_REMOVE,
    STONE_REMOVED,
} STONE_FLAG;

// The stage 3 midboss' own life cycle, shared by its two sprites.
typedef enum {
    STONE_MIDBOSS_ACTIVE,
    STONE_MIDBOSS_KILL,
    STONE_MIDBOSS_REMOVED,
} STONE_MIDBOSS_FLAG;

// The fight's current stage: opening the inner stones, then outer, then
// the closing north-stone sequence.
typedef enum {
    STONES_OPEN_INNER,
    STONES_INNER,
    STONES_OUTER,
    STONES_OPEN_NORTH,
    STONES_NORTH_FIRST,
    STONES_CLOSE_NORTH,
    STONES_NORTH_CLOSED,
    STONES_REOPEN_NORTH,
    STONES_NORTH_FINAL,
} STONES_PHASE;

// Stage 3 boss state: five circling stones, their shared ring attack and
// which sweep/fan pattern the current phase fires.
typedef struct {
    uint8_t flag[STONE_COUNT];
    int16_t damage[STONE_COUNT];
    int16_t sprite[STONE_COUNT];
    ENTITY_POINT midboss_position[STONE_COUNT][2];
    int16_t midboss_kill_frame[STONE_MIDBOSS_COUNT];
    uint8_t midboss_angle[STONE_MIDBOSS_COUNT];
    uint8_t midboss_rounds;
    ENTITY_POINT position[STONE_COUNT];
    ENTITY_POINT bullet;
    int16_t kill_frame[STONE_COUNT];
    int16_t pattern_frame;
    int16_t ring_y;
    int16_t pillar_left;
    int32_t frames;
    STONES_PHASE phase;
    uint8_t flash[STONE_COUNT];
    uint8_t pattern;
    uint8_t pillar_state;
    uint8_t spread;
    uint8_t sprite_start;
    uint8_t ring;
    uint8_t firing_ring;
    uint8_t sweep_angle;
    uint8_t fan_angle;
    uint8_t ring_angle;
    uint8_t aim_angle[STONE_COUNT];
    uint8_t ring_flag[TILE_COLUMNS];
    uint8_t ring_phase;
    uint8_t ring_index;
    uint8_t ring_done;
    int16_t ring_tile[3];
} STONES;

// Writable view of the module's private state, for tests.
extern STONES *const p_Stones;

// Clears the stones' and midboss's state, once per MAIN process.
void Stones_Init(void);

// Starts the stage 3 fight: resets damage, stones and the ring attack.
void Stones_Begin(BOSS_STATE *boss, uint8_t rank);

// Advances the fight one frame: moves, fires and checks for defeat.
BOSS_RESULT Stones_Update(
    BOSS_STATE *boss, const PATTERN_BANK *patterns, uint8_t rank);

// Restores the scrolled-out tile rows the stones' sprites marked dirty.
void Stones_Invalidate(void);

// Advances the stage 3 midboss one frame.
void Stones_MidbossUpdate(
    BOSS_STATE *boss, const PATTERN_BANK *patterns, uint8_t rank);

// Restores the midboss's scrolled-out tile row; reports whether it is
// still active.
bool Stones_MidbossInvalidate(void);
