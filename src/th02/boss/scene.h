#pragma once

#include "th02/entity.h"
#include "th02/patterns.h"

// Draws the player with both type-A options, whatever the shot and power.
void BossScene_DrawPlayer(const PATTERN_BANK *patterns);

// Draws the stationary two-row dialogue border using cached startup patterns.
void BossScene_DrawBorder(const PATTERN_BANK *patterns);

// Draws the summon scene from patterns 128..191.
void BossScene_DrawSummon(const PATTERN_BANK *patterns);

// Draws the body's hit flash from 48-pixel masks, ignoring clipping.
void BossScene_DrawFlash(
    const PATTERN_BANK *patterns, int16_t sprite, ENTITY_POINT position,
    uint8_t parts, uint8_t height);
