#pragma once

#include "th02/draw.h"
#include "th02/palette.h"

// The explosion's dot draw, set by BossExplosion_Init: Draw_Dot by
// default, an observer in tests.
typedef struct {
    void (*dot)(int16_t x, int16_t y, uint8_t size);
} BOSS_EXPLOSION_DOT;

extern const BOSS_EXPLOSION_DOT g_BossExplosionDot;

// Swaps the dot draw hook; defaults to g_BossExplosionDot, so only
// tests need to call this.
void BossExplosion_Init(const BOSS_EXPLOSION_DOT *dot);

// Draws explosion frame 0..57; others draw nothing. Frame zero's side
// clearing ignores X clipping, as retail does.
void BossExplosion_Draw(
    const PATTERN_BANK *patterns, uint8_t angle, int16_t center_x,
    int16_t center_y, int16_t frame);
