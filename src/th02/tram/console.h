#pragma once

// The DOS console as the game leaves it in text RAM: its attribute, cursor
// and system line, and the clear it does.

#include "th02/tram/cells.h"

#define TRAM_CONSOLE_DEFAULT_ATTRIBUTE TRAM_WHITE

typedef struct {
    uint8_t attribute;
    uint8_t cursor_column;
    uint8_t cursor_row;
    bool cursor_visible;
    bool system_line_visible;
} TRAM_CONSOLE;

void TramConsole_Init(TRAM_CONSOLE *console, uint8_t attribute);

// Clears the visible cells.
void TramConsole_Clear(TRAM_CONSOLE *console);
