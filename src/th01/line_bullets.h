#pragma once

#include "th01/player.h"

// Draws a solid or 16-bit patterned line as Video_Line does, then tests only
// the right/bottom endpoint: sets is_hit when right lies strictly inside the
// player's 32-pixel width and bottom > 384, unless the player is invincible.
// Clipping never changes the hit test.
void LineBullets_PutAndHitTest(
    VIDEO *video, PLAYER *player, int16_t left, int16_t top, int16_t right,
    int16_t bottom, uint8_t color);
void LineBullets_PutPatternedAndHitTest(
    VIDEO *video, PLAYER *player, int16_t left, int16_t top, int16_t right,
    int16_t bottom, uint8_t color, uint16_t pattern);

// Restores the line with Video_LineErase, without a hit test.
void LineBullets_Erase(
    VIDEO *video, int16_t left, int16_t top, int16_t right, int16_t bottom);
