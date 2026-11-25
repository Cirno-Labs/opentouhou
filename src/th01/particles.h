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

// Resets the alive flags and spawn settings, keeping motion history.
void Particles_Init(void);

// Spawns, moves and draws the particles; origin is 0..7.
// Keeps the original's first-cycle writes past slot 39.
void Particles_UpdateAndRender(PARTICLES_ORIGIN origin, uint8_t color);
