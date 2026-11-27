#pragma once

#include "th02/draw.h"
#include "th02/entity.h"

#include "core/vfs.h"

#define SPARK_COUNT 64

typedef struct {
    uint8_t state;
    uint8_t age;
    ENTITY_POINT position[2];
    ENTITY_POINT velocity;
    uint8_t drawing;
    uint8_t reserved_1;
    uint8_t angle;
    uint8_t base_speed;
    uint8_t reserved_2;
    uint8_t preferred;
} SPARK;

typedef struct {
    SPARK slots[SPARK_COUNT];
    uint16_t next;
    int16_t acceleration_x;
    uint8_t sprite_interval;
    uint8_t age_limit;
} SPARKS;

typedef struct {
    uint16_t rows[8][8][8];
} SPARK_IMAGES;

extern const SPARKS *const g_Sparks;

// Clears the sparks, once per MAIN process.
void Sparks_Init(void);

// Reseeds every slot and sets the sprite interval and age limit at stage
// start.
void Sparks_ResetStage(bool reduce_effects);

// Sets the horizontal drift applied to sparks.
void Sparks_SetDrift(int16_t acceleration_x);

// Loads the eight pre-shifted mask banks.
RESULT SparkImages_Load(VFS *files, SPARK_IMAGES *out);

// Adds sparks to the ring of 64, overwriting active ones; the cursor
// advances even for counts that add none.
void Sparks_Add(
    uint8_t back, int16_t left, uint16_t top, int16_t speed, int16_t count,
    bool as_sprite);

// Invalidates the tiles under the sparks and frees removed slots.
void Sparks_Invalidate(PLAYFIELD_PAGES pages);

// Moves and draws the sparks; sprites ignore clipping, dots do not.
void Sparks_Update(PLAYFIELD_PAGES pages, const SPARK_IMAGES *images);
