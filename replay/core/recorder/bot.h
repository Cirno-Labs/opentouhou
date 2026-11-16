#pragma once

#include "core/input/schema.h"
#include "core/math/vec.h"

#include "stg/rng.h"

#include <stdint.h>

// Drives a practice recording: holds shoot, dodges with a potential
// field built from the offsets list_threats gives (repulsion from every
// threat within BOT_THREAT_RADIUS, plus a weak pull toward a home spot
// low in the centre of the play area), and focuses and bombs by the
// nearest non-edge threat's distance. Uses its own RNG, which never
// touches the gameplay one, so the same seed always types the same
// inputs. Writes a game's input struct through its schema's action
// offsets, by name, so the bot stays the same for every game whose input
// has shoot, bomb, focus, up, down, left and right buttons.
//
// An offset from list_threats with x or y exactly 0 is one of the 4 play
// area edges that RECORDER_GAME.list_threats adds beside the real
// threats (bullets, lasers, enemies), by construction; the bot reads
// that one coordinate to steer toward the centre and tells edges apart
// from real threats for focus and bomb.
#define BOT_MAX_THREATS 32
#define BOT_THREAT_RADIUS 140.0f
#define BOT_FOCUS_DISTANCE 40.0f
#define BOT_BOMB_DISTANCE 10.0f
#define BOT_BOMB_COOLDOWN_FRAMES 300
#define BOT_DEAD_ZONE 3.0f
#define BOT_JITTER 6.0f
#define BOT_REPULSION_SCALE 2.0e6f
#define BOT_HOME_PULL_X 0.015f
#define BOT_HOME_PULL_Y 0.01f

typedef struct {
    const INPUT_SCHEMA *schema;
    size_t shoot, bomb, focus, up, down, left, right; // offsets
    RNG rng;
    bool passive; // wanders only, never dodges, focuses or bombs
    int32_t direction_frames; // frames left on the current wander, passive only
    bool holds_up, holds_down, holds_left, holds_right; // passive wander only
    int32_t bomb_cooldown_frames;
} BOT;

void Bot_Init(
    BOT *bot, const INPUT_SCHEMA *schema, uint16_t seed, bool passive);

// Zeroes out, the size of schema's input struct, and sets its next
// input. threats holds the offsets from the player to count threats
// (bullets, lasers, enemies and the 4 play area edges), nearest first, as
// list_threats writes them; passive ignores threats and wanders instead.
void Bot_NextInput(BOT *bot, const VEC2 *threats, int32_t count, void *out);
