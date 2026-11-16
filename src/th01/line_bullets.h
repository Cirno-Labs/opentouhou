#pragma once

#include "th01/player.h"

// Draws a source solid or 16-bit patterned line, then tests only the supplied
// right/bottom endpoint. Sets is_hit when right lies strictly inside the
// player's 32-pixel width and bottom > 384, unless invincible. Ignores top,
// left, sliding and any upper Y bound; clipping never changes the hit test.
// Uses Video_Line cursor and address semantics. Allocates nothing. Boss
// pattern scheduling and gameplay scene integration remain external.
void LineBullets_PutAndHitTest(
    VIDEO *video, PLAYER *player, int16_t left, int16_t top, int16_t right,
    int16_t bottom, uint8_t color);
void LineBullets_PutPatternedAndHitTest(
    VIDEO *video, PLAYER *player, int16_t left, int16_t top, int16_t right,
    int16_t bottom, uint8_t color, uint16_t pattern);

// Restores with Video_LineErase without testing or changing the player.
void LineBullets_Erase(
    VIDEO *video, int16_t left, int16_t top, int16_t right, int16_t bottom);
