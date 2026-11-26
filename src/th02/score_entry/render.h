#pragma once

// Drawing for name entry: the score table, the alphabet grid and the name
// being typed.

#include "th02/score_entry/logic.h"
#include "th02/tram.h"

// Draws the score table, names ending within seven bytes; place -1 leaves
// the alphabet untouched.
void ScoreEntryRender_Table(
    const SCORE_SECTION *section, bool ending, int8_t place);
void ScoreEntryRender_Alphabet(uint8_t col, uint8_t row, uint16_t attribute);

// Draws the name and highlights its selected slot, including a zero glyph.
void ScoreEntryRender_Name(
    const SCORE_SECTION *section, int8_t place, uint8_t position);
