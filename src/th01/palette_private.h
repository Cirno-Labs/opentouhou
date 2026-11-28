#pragma once

#include "th01/palette.h"

// Writable view of g_Palette and g_StagePalette, for the module's own
// files and for the boss and gameplay code whose named functions already
// describe the mutation (M_WhiteIn, M_BlackOut, Palette_ResetStage, ...).
extern PALETTE_STATE *const p_Palette;
extern PALETTE *const p_StagePalette;
