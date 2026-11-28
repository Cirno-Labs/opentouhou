#pragma once

#include "th02/bomb.h"
#include "th02/bomb/assets.h"
#include "th02/draw.h"
#include "th02/tram.h"

// Writable view of g_Bomb for tests.
extern BOMB *const p_Bomb;

// Draws one circle point without clipping; the origin must be in
// X=25..415, Y=9..383. Requires RMW charging.
void BombRender_CirclePoint(
    const BOMB_ASSETS *assets, int16_t scroll, int16_t left, int16_t top);

// Draws one particle at X floored to eight pixels, without clipping.
// Requires RMW charging.
void BombRender_Particle(
    const BOMB_ASSETS *assets, int16_t scroll, int16_t left, int16_t top,
    uint8_t image);

// Draws the smear down to bottom<=384, at least one row even when
// bottom<=16. Requires RMW charging.
void BombRender_Smear(
    const BOMB_ASSETS *assets, int16_t scroll, int16_t left, int16_t bottom);

// Draws eight high-bit-first tile blocks at X floored to eight pixels.
void BombRender_Tiles(int16_t scroll, int16_t left, int16_t top, uint8_t bits);

// Draws one 72-byte transition frame; requires Tiles' RMW state.
void BombRender_Transition(int16_t scroll, const uint8_t frame[72]);

// Draws text RAM frame 0..17 of shot type 1 inside the playfield.
void BombRender_Text(const BOMB_ASSETS *assets, uint8_t frame);

// Draws the illustration at top plus scroll, which wraps to a signed word
// and drops 400 only once. Requires disabled charging.
void BombRender_Illustration(
    const BOMB_ASSETS *assets, int16_t scroll, int16_t left, int16_t top);
