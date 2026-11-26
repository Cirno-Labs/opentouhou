#pragma once

// Scanout: text RAM drawn as pixels the way the display shows it, gaiji
// from their bank and other characters from a font.

#include "th02/gaiji.h"
#include "th02/tram/cells.h"

// Returns a row of a character cell, leftmost dot in bit 15.
typedef struct {
    uint16_t (*row)(const void *context, uint16_t code, bool wide, uint8_t y);
    const void *context;
} TRAM_FONT;

// Renders text RAM as transparent 0 and digital colors 1..8; as in
// hardware, underlines reach four pixels into the next cell.
void Tram_Scanout(
    const GAIJI *bank, const TRAM_FONT *font, uint16_t first_cell,
    bool blink_visible, uint8_t *pixels, size_t stride);
