#pragma once

#include "th01/player.h"
#include "pc98/zun/math16.h"

#define MISSILES_COUNT 50

typedef enum {
    MISSILE_FREE,
    MISSILE_MOVING,
    MISSILE_HIT,
    MISSILE_HIT_LAST,
} MISSILE_FLAG;

typedef struct {
    int16_t left, top, previous_left, previous_top;
    MATH16_VECTOR velocity;
    int16_t unused;
    int8_t reserved;
    uint8_t flag;
} MISSILE;

typedef struct {
    MISSILE slots[MISSILES_COUNT];
} MISSILES;

extern MISSILES g_Missiles;

// Validates boss3_m.ptn loaded with Ptn_Load into the caller's asset arena.
// Maps the source PTN bank to local images 0..3 and explosion image 4.
RESULT Missiles_ValidateSprites(const PTN *sprites);

// Zero-initialize once. Selects the first free slot. Converts pixels to
// signed Q12.4 and truncates velocity*16 toward zero with signed wrapping.
// Requires finite velocities with scaled magnitude below 2^63. Retains
// previous_top and unused history; only previous_left becomes -1.
bool Missiles_Add(
    int16_t left, int16_t top, double velocity_x, double velocity_y,
    int8_t reserved);

// Clears flags only, without restoring pixels or clearing history.
void Missiles_Reset(void);

// Runs restore/motion/explosion, rendering, then lethal tests in pool order.
// Moves and tests all slots; frame_rand bit 0 selects only restore/render
// and explosion advancement. Explosions remain lethal at current coordinates.
// Restores old history immediately on boundary crossing, even for new slots;
// clamps only crossed previous axes. Bottom explosions use previous Y 392.
// Draws with validated local sprites; plays effect 7 on removal. Keeps
// unaligned quarter positions and 32-pixel-wide word-rounded restores.
// Reproduces unsigned planar offsets, GRCG clears, raw cross-plane OR and
// x86 shift counts for negative first-use explosion positions. Hidden VRAM
// has no native storage. Normal sprite positions are X 0..624, Y 64..399.
// Stops after the first row whose next planar offset exceeds 32000. Defines
// physical linear byte accesses across a segment boundary instead of the
// original machine's possible protection fault, as a presentation extension.
// Allocates nothing and reads no devices. Gameplay scene integration
// remains external.
void Missiles_UpdateAndRender(uint16_t frame_rand, const PTN *sprites);
