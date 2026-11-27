#pragma once

#include "th02/hud/counters.h"
#include "th02/score.h"

// Draws the whole HUD for rank 0..4 and returns the shot level.
uint8_t Hud_Put(
    const SCORE *score, uint16_t continues, uint8_t rank, uint8_t power);
