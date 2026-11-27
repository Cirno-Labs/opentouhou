#pragma once

// What the main menu and the options menu share: cursor movement and the
// difficulty rank names.

#include "th02/tram.h"

#include "core/base/types.h"

typedef struct {
    int8_t selection;
    bool in_option;
    bool extra_unlocked;
} MENU_NAV;

// Moves the cursor with signed-byte wrap; redraw the old and new choice.
void Menu_Move(MENU_NAV *nav, int8_t maximum, int8_t direction);

void Menu_Clear(TRAM_CONSOLE *console, uint8_t clear_attribute);

// Returns the glyphs spelling a difficulty rank, 0 (Easy) to 3 (Lunatic).
const uint8_t *Menu_DifficultyName(int8_t difficulty);
