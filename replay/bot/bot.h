#pragma once

#include "core/input/schema.h"
#include "core/math/vec.h"

#include "stg/rng.h"

#include <stdint.h>

// Plays a stage deterministically: holds shoot, dodges along a potential
// field of the threats near the player, with a weak pull toward a spot low
// in the centre of the play area, and focuses and bombs by the nearest
// threat's distance. Uses an RNG of its own, so that one seed always gives
// the same inputs and the gameplay RNG is never touched. Writes the input
// struct of a game whose schema has the buttons shoot, bomb, focus, up,
// down, left and right.
//
// A threat offset with x or y exactly 0 is one of the four play area edges,
// which the game lists beside its bullets, lasers and enemies.

#define BOT_MAX_THREATS 32

typedef struct {
    const INPUT_SCHEMA *schema;
    size_t shoot, bomb, focus, up, down, left, right;
    RNG rng;
    // Makes the bot wander in place of dodging, focusing and bombing.
    bool passive;
    int32_t wander_frames;
    bool wander_up, wander_down, wander_left, wander_right;
    int32_t bomb_cooldown;
} BOT;

void Bot_Init(
    BOT *bot, const INPUT_SCHEMA *schema, uint16_t seed, bool passive);

// Writes the next input into out, a struct of schema's size. threats holds
// the offsets from the player to count threats and the edges, nearest
// first.
void Bot_NextInput(BOT *bot, const VEC2 *threats, int32_t count, void *out);
