#pragma once

// The HUD column's counters: score, continues, lives, bombs and power, each
// drawn from the value it is given, with the original's overflow quirks.

#include "core/base/types.h"

// Draws the score; as in retail, negative or oversized scores pick raw
// gaiji, and the continues cell stays separate.
void Hud_Score(uint16_t y, int32_t value);

// Draws continues capped at nine; negative values pick raw gaiji.
void Hud_Continues(uint16_t y, int16_t value);

// Draws the lives; as in retail, negative counts blank the cells before
// and counts above five spill into the cells after.
void Hud_Lives(int8_t value);
void Hud_Bombs(int8_t value);

// Draws power 0..83 and returns its shot level; zero shows a full first
// segment and 80..83 show MAX.
uint8_t Hud_Power(uint8_t power);
