#pragma once

// Numbers in gaiji digits, two cells per digit, as the HUD, the bonus
// screen, the score tables and the options draw them.

#include "core/base/types.h"

// Draws the last digits of value, up to eight, without leading zeros; the
// last digit shows even when zero if show_zero is set. As in retail,
// negative values pick raw gaiji.
void Hud_Number(
    uint16_t left, uint16_t top, uint8_t digits, int32_t value,
    uint8_t attribute, bool show_zero);
