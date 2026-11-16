#pragma once

#include "th01/math.h"
#include "th01/video.h"

#define PARTICLES_COUNT 40

typedef enum {
    PARTICLES_TOP,
    PARTICLES_TOP_RIGHT,
    PARTICLES_RIGHT,
    PARTICLES_BOTTOM_RIGHT,
    PARTICLES_BOTTOM,
    PARTICLES_BOTTOM_LEFT,
    PARTICLES_LEFT,
    PARTICLES_TOP_LEFT,
} PARTICLES_ORIGIN;

typedef struct {
    int16_t spawn_interval, velocity_max;
    int16_t x[PARTICLES_COUNT], y[PARTICLES_COUNT];
    int16_t velocity_x[PARTICLES_COUNT], velocity_y[PARTICLES_COUNT];
    uint8_t alive[PARTICLES_COUNT], velocity_base[PARTICLES_COUNT];
    uint8_t cycle;
} PARTICLES;

// Zero-initialize once per main scene. Resets alive flags and spawn settings
// while retaining cycle, velocities, positions and velocity_base history.
// Stores flags as raw bytes because the slot-40 spill can write 255.
void Particles_Init(PARTICLES *particles);

// Spawns, restores old pixels, advances Q12.4 motion and draws surviving dots.
// Preserves the first cycle's slot-40 aliases through explicit field writes.
// Uses shared RNG and caller-owned state; allocates nothing and performs no IO.
// Requires initialization and an origin in 0..7. Keeps the drawing selector
// when no active particle reaches restoration; otherwise finishes on page 0.
void Particles_UpdateAndRender(
    PARTICLES *particles, VIDEO *video, uint32_t *rng, PARTICLES_ORIGIN origin,
    uint8_t color);
