#pragma once

#include "th01/particles.h"

#include <stdint.h>

#define PARTICLES_COUNT 40

typedef struct {
    int16_t spawn_interval;
    int16_t velocity_max;
    int16_t x[PARTICLES_COUNT];
    int16_t y[PARTICLES_COUNT];
    int16_t velocity_x[PARTICLES_COUNT];
    int16_t velocity_y[PARTICLES_COUNT];
    uint8_t alive[PARTICLES_COUNT];
    uint8_t velocity_base[PARTICLES_COUNT];
    uint8_t cycle;
} PARTICLES;

// Read-only view of the particle pool.
extern const PARTICLES *const g_Particles;

// Writable view of the particle pool for tests.
extern PARTICLES *const p_Particles;
