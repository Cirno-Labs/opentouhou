#pragma once

#include "th07/anm.h"

#include "core/base/result.h"
#include "core/math/vec.h"
#include "stg/timer.h"

#include <stdint.h>

// Runs short-lived effects drawn with ANM scripts: explosions, particles,
// charges, orbits, auras and the stage's falling petals. Effects draw from
// the gameplay generator when they start and move at the game speed, so the
// effects spawned are part of the simulation. Petals live in the stage's
// 3D space and are drawn by the stage, under the effects quality.

#define EFFECT_COUNT 400
#define EFFECT_RESERVED_COUNT 8

// Selects what an effect shows and how it moves. Enemy scripts pick types
// by number too, so the numbers are fixed.
typedef enum {
    EFFECT_EXPLOSION = 0,
    EFFECT_EXPLOSION_SPIN = 1,
    EFFECT_EXPLOSION_LARGE = 2,
    EFFECT_BOMB_SPARK = 3, // where a bomb hits an enemy
    EFFECT_PARTICLE = 4, // plus a color from 0 to 7; drifts to a stop
    EFFECT_SHOCKWAVE = 12,
    EFFECT_ORBIT = 13, // circles its owner; 14 and 15 are the same
    EFFECT_SPELL_BACKGROUND = 16, // the stage's spell card background
    EFFECT_CHARGE = 17, // converges over 60 frames
    EFFECT_CHARGE_LONG = 18, // converges over 240 frames
    EFFECT_SPARK = 19, // a sprite its owner moves
    EFFECT_PETAL = 20, // falls around the stage camera
    EFFECT_KNIFE_BOMB = 21, // starts Sakuya's bombs
    EFFECT_BURST = 22, // bursts out at angle velocity.x; 32 is the same
    EFFECT_AT_CAMERA = 23, // stays where the stage camera looks
    EFFECT_PLAYER_AURA = 24, // follows the player while its script runs
    EFFECT_SPELL_CIRCLE = 25, // shrinks around a bomb or a spell card
    EFFECT_PETAL_RISING = 26, // circles the camera's view, rising
    EFFECT_PETAL_SINKING = 27, // circles the camera's view, sinking
    EFFECT_BORDER = 28, // the Supernatural Border around the player
    EFFECT_DIVERGE = 29, // goes out along axis, 256 every 30 frames
    EFFECT_PETAL_DRIFT = 30, // drifts until Effect_StartPetalsFalling
    EFFECT_PETAL_FALLING = 31,
    EFFECT_CHARGE_ALT = 33, // EFFECT_CHARGE with another script
} EFFECT_TYPE;

typedef struct {
    ANM_VM vm;
    VEC3 pos;
    VEC3 velocity; // as spawned; petals and bursts start from it
    VEC3 step; // added to pos, or to true_pos for petals, each frame
    VEC3 acceleration; // added to step each frame
    VEC3 true_pos; // a petal's position in the stage
    VEC3 origin; // the orbit's center; where a converging effect ends
    VEC3 axis; // the orbit's axis; a converging effect's direction
    VEC4 rotation; // the orbit's quaternion
    float radius; // the orbit's radius and angle, which the owner moves
    float angle;
    TIMER timer;
    int8_t type;
    bool active;
    bool fade_out; // set by the owner to fade an orbit out over 16 frames
    int8_t fade_frames;
    // 0 draws in the play area, 2 projected through the current camera, 1
    // and 3 with the petals: 1 projected, 3 in 3D.
    int8_t draw_list;
} EFFECT;

// Adds the update element (priority 11) and the draw element (priority
// 9). Adding loads the effect sprites of g_GameManager.run.stage, from
// data/eff01.anm to eff08.anm, and sets the stage's spell background count.
RESULT Effect_Register(void);

// Removes both elements and releases the effect sprites.
void Effect_Remove(void);

// Starts count effects of type at pos in color, in free slots from the
// one after the last used, with depth writes off. Returns the last effect
// started, or a spare effect that never runs when no slot was free.
EFFECT *Effect_Spawn(EFFECT_TYPE type, VEC3 pos, int32_t count, uint32_t color);

// Effect_Spawn with a starting velocity, keeping the depth writes of the
// script.
EFFECT *Effect_SpawnMoving(
    EFFECT_TYPE type, VEC3 pos, VEC3 velocity, int32_t count, uint32_t color);

// Starts an effect of type at pos in color in reserved slot slot, from 0
// to 7, with depth writes off, and returns it.
EFFECT *Effect_SpawnReserved(
    EFFECT_TYPE type, VEC3 pos, int32_t slot, uint32_t color);

// Moves the petals of EFFECT_PETAL and EFFECT_PETAL_FALLING by offset.
void Effect_OffsetPetals(VEC3 offset);

// Makes the petals of EFFECT_PETAL_DRIFT start falling.
void Effect_StartPetalsFalling(void);

// Sets the scale of each channel of the effect colors (opcode 140).
void Effect_SetColorScale(float red, float green, float blue, float alpha);

// Draws the petals, as the stage's draw does after the stage. Effects
// quality 0 and 1 draw none.
void Effect_DrawPetals(void);
