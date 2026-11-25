#pragma once

#include "th01/video.h"

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

// Clears all state for a new gameplay session.
void Particles_Reset(void);

// Resets alive flags and spawn settings
// while retaining cycle, velocities, positions and velocity_base history.
// Stores flags as raw bytes because the slot-40 spill can write 255.
void Particles_Init(void);

// Spawns, restores old pixels, advances Q12.4 motion and draws surviving dots.
// Preserves the first cycle's slot-40 aliases through explicit field writes.
// Uses shared RNG and module state; allocates nothing and performs no IO.
// Requires initialization and an origin in 0..7. Keeps the drawing selector
// when no active particle reaches restoration; otherwise finishes on page 0.
void Particles_UpdateAndRender(PARTICLES_ORIGIN origin, uint8_t color);
