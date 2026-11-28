#pragma once

#include "th02/bullets/pool.h"

// A pattern's spawn direction, as its bullet index resolves.
typedef struct {
    int16_t speed;
    int16_t next_index;
    uint8_t angle;
    bool aimed;
    bool last;
} BULLET_DIRECTION;

// Writable view of g_Bullets for the module's parts.
extern BULLETS *const p_Bullets;

// Per-stage spawn tuning: rank bias, layer cycling and the slow-bullet
// alternation.
typedef struct {
    int8_t rank;
    int8_t speed_bias;
    uint8_t base_layers;
    uint8_t layers;
    uint8_t slow_cycle;
} BULLET_SPAWN;

// Writable view of the spawn module's private state, for tests.
extern BULLET_SPAWN *const p_Spawn;

// Clears the spawn state, once per MAIN process.
void BulletSpawn_Clear(void);

// Sets the speed bias for rank: -10 on Easy, +12 on Lunatic, else zero.
void BulletSpawn_Init(int8_t rank);

// Reports whether a spawn is outside X=24..416, Y=0..383, inclusive.
bool BulletSpawn_Outside(int16_t left, int16_t top);

// Tunes speed by performance and rank; returns true to skip the spawn, as
// Easy does every other slow bullet. Other slow speeds clamp to 16.
bool BulletSpawn_Tune(int16_t *speed);

// Reports whether to skip a spawn that is outside, during a bomb or
// rejected by tuning; gravity bullets skip tuning.
bool BulletSpawn_Skip(int16_t left, int16_t top, bool gravity, int16_t *speed);

// Replaces layers with base_layers*multiplier modulo 256.
void BulletSpawn_Layers(uint8_t multiplier);

// Steps to the next layer at half speed, +8 on Lunatic; returns false at
// the last layer, or below speed 16, which keeps the lowered speed.
bool BulletSpawn_NextLayer(int16_t *layer, int16_t *index, int16_t *speed);

// Returns bullet index's direction in a pattern, drawing random angle then
// speed and wrapping as retail does, even past the fan tables.
BULLET_DIRECTION BulletSpawn_Direction(
    int16_t pattern, int16_t index, uint8_t angle, int16_t speed);
