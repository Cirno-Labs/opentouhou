#pragma once

#include "th02/draw.h"
#include "th02/palette.h"

// Draws explosion frame 0..57; others draw nothing. Frame zero's side
// clearing ignores X clipping, as retail does.
void BossExplosion_Draw(
    const PATTERN_BANK *patterns, uint8_t angle, int16_t center_x,
    int16_t center_y, int16_t frame);
