#pragma once

#include "th01/interleave.h"
#include "th01/palette.h"

// Requires live palettes, page 1 and assets through completion, allowing
// saved-palette aliases and synchronous completion for zero delay.
bool Transition_BeginDiagonal(
    PALETTE_STATE *palette, PALETTE *current, const PALETTE *target,
    const INTERLEAVE_ASSETS *assets, uint16_t step_ms, uint64_t host_tick);

// Uses monotonic 60 Hz host serials and fractional credit so 130 delays of
// 40 ms finish after 312 ticks, with repeated serials inert.
bool Transition_Advance(
    PALETTE_STATE *palette, PALETTE *current, const PALETTE *target,
    const INTERLEAVE_ASSETS *assets, uint64_t host_tick);

void Transition_Init(void);
