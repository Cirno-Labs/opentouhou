#pragma once

#include "th02/draw.h"
#include "th02/entity.h"

// Draws a body or tiny option sprite at position plus scroll, which wraps
// to a word and drops 400 once.
void PlayerRender_Put(
    const PATTERN_BANK *patterns, ENTITY_POINT position, int16_t scroll,
    uint16_t sprite, bool tiny);
