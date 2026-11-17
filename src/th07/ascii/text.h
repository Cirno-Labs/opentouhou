#pragma once

#include "th07/ascii/types.h"

#include "core/base/result.h"
#include "core/base/utils.h"

// Runs the ASCII text queue, score popups, boss markers, the cherry summary,
// the demo icon, the pause menu and the continue menu. Records atlas glyphs,
// popups and menu sprites into the supplied frame.

extern ASCII_TEXT g_AsciiText;

// Adds the update element at priority 1 and draw elements at 16 and 11.
// Loads the ASCII font and the capture sprites, then initializes the state
// and starts the summary, boss marker and bomb icon scripts.
RESULT AsciiText_Register(void);

// Removes the update and priority-16 draw elements and releases the fonts.
// Keeps the priority-11 draw element until the chain releases it.
void AsciiText_Remove(void);

// Clears strings, menus and popups, then resets the drawing style and glyphs.
// Keeps the summary, boss markers, demo icon, capture_background and
// highlight_font, and sends summary_interrupt to the summary again.
void AsciiText_Initialize(void);

// Updates the frame rate shown at the bottom of the screen.
void AsciiText_SetFrameRate(float rate);

void AsciiText_SetColor(uint32_t color);
void AsciiText_SetHighlight(bool highlight);

// Copies UTF-8 text with the current style into the queue. Drops strings
// after the first 256. The main draw callback clears the queue.
void AsciiText_AddString(VEC3 pos, const char *text);
PRINTF_FORMAT(2, 3)
void AsciiText_AddFormat(VEC3 pos, const char *format, ...);

// Starts a popup at pos in play-area pixels in the next of the 720
// ordinary slots. Negative values show one minus sign. Popups move up for
// 61 frames of unpaused calcs at the game speed.
void AsciiText_AddPopup(VEC3 pos, int32_t value, uint32_t color);

// Starts a popup in the next of the three reserved slots.
void AsciiText_AddPopup2(VEC3 pos, int32_t value, uint32_t color);

// Sends interrupt to the summary and keeps it in summary_interrupt.
void AsciiText_SetSummaryInterrupt(int16_t interrupt);
