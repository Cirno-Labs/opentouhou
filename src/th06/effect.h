#pragma once

#include "th06/anm.h"
#include "th06/timer.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include <stdint.h>

// Runs short-lived effects drawn with ANM scripts: explosions, particles,
// shockwaves, charges and the orbits around enemies. Particles and charges
// draw from the gameplay generator when they start, so the effects spawned
// are part of the simulation. Drawing comes with graphics.

#define EFFECT_COUNT 0x200

// Selects what an effect shows and how it moves. Enemy scripts pick types
// by number too, so the numbers are fixed.
typedef enum {
    EFFECT_EXPLOSION = 0,
    EFFECT_EXPLOSION_SPIN = 1,
    EFFECT_EXPLOSION_LARGE = 2,
    EFFECT_BOMB_SPARK = 3, // where a bomb hits an enemy
    EFFECT_PARTICLE = 4, // plus a color from 0 to 3; drifts to a stop
    EFFECT_SMALL_PARTICLE = 8, // plus a color from 0 to 3
    EFFECT_SHOCKWAVE = 12,
    EFFECT_ORBIT = 13, // circles its owner; 14 and 15 are the same
    EFFECT_SPELL_BACKGROUND = 16, // the stage's spell card background
    EFFECT_CHARGE = 17, // converges over 60 frames
    EFFECT_CHARGE_LONG = 18, // converges over 240 frames
    EFFECT_SPARK = 19, // a particle its owner moves
} EFFECT_TYPE;

typedef struct {
    ANM_VM vm;
    VEC3 pos;
    VEC3 velocity;
    VEC3 acceleration;
    VEC3 origin; // the orbit's centre; where a converging effect ends
    VEC3 axis; // the orbit's axis; a converging effect's direction
    VEC4 rotation; // the orbit's quaternion
    float radius; // the orbit's radius and angle, which the owner moves
    float angle;
    TIMER timer;
    int8_t type;
    bool active;
    bool fade_out; // set by the owner to fade an orbit out over 16 frames
    int8_t fade_frames;
} EFFECT;

// Adds the update element (priority 10) and the draw element (priority
// 8). Adding loads the effect sprites of g_GameManager.run.stage.
RESULT Effect_Register(void);
// Removes both elements and releases the effect sprites.
void Effect_Remove(void);
// Starts count effects of type at pos in color, in free slots from the
// one after the last used. Returns the last effect started, or a spare
// effect that never runs when no slot was free.
EFFECT *Effect_Spawn(EFFECT_TYPE type, VEC3 pos, int32_t count, uint32_t color);
