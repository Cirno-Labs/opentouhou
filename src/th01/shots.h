#pragma once

#include "th01/orb.h"

#define SHOTS_COUNT 8
#define SHOTS_PTN 1

typedef struct {
    int16_t left, top;
    bool moving;
    uint8_t decay_frame;
} SHOT;

typedef struct {
    SHOT slots[SHOTS_COUNT];
} SHOTS;

typedef enum {
    SHOTS_ACTION_RESTORE,
    SHOTS_ACTION_PUT,
    SHOTS_ACTION_ERASE,
    SHOTS_ACTION_EFFECT,
} SHOTS_ACTION_KIND;

typedef struct {
    SHOTS_ACTION_KIND kind;
    int16_t left, top, quarter, effect;
} SHOTS_ACTION;

// Executes each action synchronously before the next state change. Required
// callbacks may draw or post effects, but must not modify shot or Orb state.
// Effect numbers are one-based. Successful updates allocate nothing.
typedef struct {
    void (*action)(void *user, SHOTS_ACTION action);
    void *user;
} SHOTS_OUTPUT;

// Validates stage sprite storage and image count before shot ticks.
RESULT Shots_ValidateSprites(const PTN *sprites);

// Zero-initialize once. Add selects the first idle, non-decaying slot and
// emits effect 1 only on success. Accepts X from 0 through 639.
bool Shots_Add(SHOTS *shots, int16_t left, int16_t top, SHOTS_OUTPUT output);
void Shots_EraseAndReset(SHOTS *shots, SHOTS_OUTPUT output);

// Call inside the future pellet composite, after pellet motion/clipping and
// before pellet rendering. Player spawning precedes ordinary Orb motion;
// this update follows the Orb composite's wait. Do not also update shots
// during player update. Orb hits precede top clipping and move the Orb an
// extra time without advancing force age or rotation. Decay lasts frames
// 1..7. Uses supplied shared RNG through Orb_NewForce only.
void Shots_UpdateAndRender(
    SHOTS *shots, ORB *orb, uint32_t *rng, SHOTS_OUTPUT output);
bool Shots_HitTestOrb(SHOTS *shots, int32_t slot, ORB *orb, uint32_t *rng);

// Tests inclusive overlap with an 8 by 8 pellet, with two-pixel margins.
// First hit restores the shot, starts decay at 1 and emits effect 16.
bool Shots_HitTestPellet(
    SHOTS *shots, int16_t left, int16_t top, SHOTS_OUTPUT output);

// Tests shot top-left against inclusive boss bounds. Width and height are
// visual hitbox dimensions minus the 16-pixel shot dimensions.
bool Shots_HitTestBoss(
    SHOTS *shots, int16_t left, int16_t top, int16_t width_minus_shot,
    int16_t height_minus_shot, SHOTS_OUTPUT output);

// Executes drawing actions with stage PTN image 1; requires at least two
// images. Effect actions belong to the audio event consumer and do no drawing.
void Shots_RenderAction(VIDEO *video, const PTN *sprites, SHOTS_ACTION action);
