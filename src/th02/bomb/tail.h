#pragma once

#include "th02/tram.h"

#define BOMB_TAIL_SIZE 72

// Fills out with the original's 16th bomb mask, read past bombs.bft's 15:
// block slack, the next block's header (its segment as in the harness's
// DOSBox-X, 876 paragraphs lower in a demo) and two gaiji backup glyphs.
void BombTail_Build(
    const TRAM_FONT *font, bool demo, uint8_t out[BOMB_TAIL_SIZE]);
