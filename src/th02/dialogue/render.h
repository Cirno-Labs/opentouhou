#pragma once

#include "th02/dialogue/assets.h"
#include "th02/dialogue/box.h"

typedef struct {
    const DIALOGUE_ASSETS *assets;
    TRAM_TEXT continued_line;
    TRAM_CHARACTER continued_characters[36];
    uint8_t continued_box;
    bool show_continues;
} DIALOGUE_RENDER;

// Returns the view drawing into render, which must outlive it; face 255
// draws a black rectangle.
DIALOGUE_VIEW DialogueRender_View(DIALOGUE_RENDER *render);

// Writes continues 1..99 into the box; single digits keep the tens field,
// as in retail.
void DialogueRender_Continues(
    DIALOGUE_RENDER *render, uint8_t box, const TRAM_CHARACTER digits[10],
    uint16_t continues);
