#pragma once

// The stage's scripted enemies: spawning, their 64-byte movement scripts and
// shot/collision handling.

#include "th02/entity.h"
#include "th02/patterns.h"
#include "th02/stage/data.h"

#define ENEMY_COUNT 25

typedef struct {
    ENTITY_POINT position[2];
    int16_t script_ip;
    int16_t wait_frame;
    int16_t formation;
    uint8_t state;
    uint8_t age;
    uint8_t dying;
    int8_t reserved_11;
    int16_t image;
    int16_t animation;
    int16_t loop_i;
    int16_t spawned_left;
    int16_t loop_j;
    int8_t velocity_x;
    int8_t velocity_y;
    uint8_t clip_y;
    int8_t reserved_1f;
    int16_t damage;
    uint8_t hit_disabled;
    uint8_t collision_disabled;
    uint8_t bullet_pattern;
    uint8_t bullet_speed;
} ENEMY;

typedef struct {
    ENEMY slots[ENEMY_COUNT];
    int16_t line;
    uint8_t count;
} ENEMIES;

extern const ENEMIES *const g_Enemies;

// Clears the enemy pool, once per MAIN process.
void Enemies_Init(void);

// Frees every slot and resets the spawn line at stage start, keeping the
// count and slot payloads.
void Enemies_ResetStage(void);

// Adds an enemy in slot; as in retail, count rises only when it was below
// slot.
void Enemies_Add(
    uint8_t back, int16_t left, int16_t top, uint8_t slot, int16_t formation);

// Spawns at most one row, when rows reaches it exactly, even while the
// midboss suppresses its spawns.
void Enemies_Spawn(
    const STAGE_DATA *data, uint8_t back, int16_t rows, uint8_t midboss_active);

// Invalidates the tiles under the enemies, frees removed slots and
// recounts.
void Enemies_Invalidate(PLAYFIELD_PAGES pages);

// Marks every enemy removed and clears count.
void Enemies_RemoveAll(void);

// Runs and draws the enemies' 64-byte scripts, with scrolled the pixels
// the stage scrolled this frame; as in retail, shots hit a 32x32 box
// whatever the sprite size.
void Enemies_Update(
    PLAYFIELD_PAGES pages, uint8_t scrolled, const STAGE_DATA *data,
    const PATTERN_BANK *patterns, uint32_t frame, uint8_t rank,
    uint8_t bomb_frame_mask);
