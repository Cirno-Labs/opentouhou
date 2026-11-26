#pragma once

// The pause menu's labels: five lines of text baked into MAIN.EXE,
// loaded once as tram text.

#include "th02/tram.h"

typedef struct {
    TRAM_TEXT labels[5];
    ALLOCATOR allocator;
} PAUSE_MENU_ASSETS;

// Loads the pause labels; the allocator must free individually.
RESULT PauseMenuAssets_Load(const ALLOCATOR *allocator, PAUSE_MENU_ASSETS *out);

// Frees the labels once the pause menu closes; accepts zeroed storage.
void PauseMenuAssets_Free(PAUSE_MENU_ASSETS *assets);
