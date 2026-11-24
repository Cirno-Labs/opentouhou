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

extern SHOTS g_Shots;

// Zeroes the shots for a new world.
void Shots_Init(void);

// Validates stage sprite storage and image count before shot ticks.
RESULT Shots_ValidateSprites(const PTN *sprites);

// Zero-initialize once. Add selects the first idle, non-decaying slot and
// plays effect 1 only on success. Accepts X from 0 through 639.
bool Shots_Add(int16_t left, int16_t top);
void Shots_EraseAndReset(void);

// Called by the pellet composite after pellet motion/clipping and
// before pellet rendering. Player spawning precedes ordinary Orb motion;
// this update follows the Orb composite's wait. Do not also update shots
// during player update. Orb hits precede top clipping and move the Orb an
// extra time without advancing force age or rotation. Decay lasts frames
// 1..7. Uses supplied shared RNG through Orb_NewForce only. Draws with
// stage PTN image 1; requires at least two images.
void Shots_UpdateAndRender(const PTN *sprites);
bool Shots_HitTestOrb(int32_t slot);

// Tests inclusive overlap with an 8 by 8 pellet, with two-pixel margins.
// First hit restores the shot, starts decay at 1 and plays effect 16.
bool Shots_HitTestPellet(int16_t left, int16_t top);

// Tests shot top-left against inclusive boss bounds. Width and height are
// visual hitbox dimensions minus the 16-pixel shot dimensions.
bool Shots_HitTestBoss(
    int16_t left, int16_t top, int16_t width_minus_shot,
    int16_t height_minus_shot);
