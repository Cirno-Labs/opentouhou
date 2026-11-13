// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/ascii/types.h"

#include "core/base/result.h"
#include "core/base/utils.h"

// Runs the ASCII text queue, score popups, pause menu and continue menu.
// Records atlas glyphs, popups and menu sprites into the supplied frame.
// Framebuffer capture and its background scripts are disabled headless.

extern ASCII_TEXT g_AsciiText;

// Adds the update element at priority 1 and draw elements at 15 and 10.
// Loads the ASCII fonts and capture sprites, then initializes the state.
RESULT AsciiText_Register(void);
// Removes the update and priority-15 draw elements and releases the fonts.
// Keeps the priority-10 draw element until the chain releases it.
void AsciiText_Remove(void);
// Clears strings, menus and popups, then resets the drawing style and glyphs.
// Keeps capture_background and highlight_font.
void AsciiText_Initialize(void);

void AsciiText_SetColor(uint32_t color);
void AsciiText_SetHighlight(bool highlight);
// Copies UTF-8 text with the current style into the queue. Drops strings
// after the first 256. The main draw callback clears the queue.
void AsciiText_AddString(VEC3 pos, const char *text);
PRINTF_FORMAT(2, 3)
void AsciiText_AddFormat(VEC3 pos, const char *format, ...);
// Starts a popup in the next of the 512 ordinary slots. Negative values
// show one minus sign. Popups move up for 61 unpaused calcs.
void AsciiText_AddPopup(VEC3 pos, int32_t value, uint32_t color);
// Starts a popup in the next of the three reserved slots.
void AsciiText_AddPopup2(VEC3 pos, int32_t value, uint32_t color);
