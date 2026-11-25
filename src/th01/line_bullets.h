#pragma once

#include "th01/player.h"

// Draws a line as Video_Line does and sets is_hit when its right/bottom
// endpoint lies in the player; the clipped line never changes the test.
void LineBullets_PutAndHitTest(
    int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color);
void LineBullets_PutPatternedAndHitTest(
    int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color,
    uint16_t pattern);

// Restores the line with Video_LineErase, without a hit test.
void LineBullets_Erase(
    int16_t left, int16_t top, int16_t right, int16_t bottom);
