#pragma once

#include "th01/stage.h"

// Clears all state for a new gameplay session.
void Items_Init(void);

// Caches all popup characters from rom in caller-owned asset storage before
// ticks.
RESULT ItemsAssets_LoadFont(
    const FONT_ROM *rom, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);

// Validates immutable sprites and every required glyph before updates.
RESULT Items_ValidateAssets(const PTN *sprites, const GAME_TEXT_FONT *font);

// Adds at the first free slot using current card coordinates, without RNG.
// A full pool ignores the request.
void Items_Add(int16_t left, int16_t top, bool bomb);

// Clears the item flags, keeping the shared point value.
void Items_Reset(void);

// Draws visible falling/bouncing sprites, bombs before points.
void Items_Render(const PTN *sprites);

// Updates and draws all bomb items, then all point items; run it after the
// player update. Popups show the live point value, as in the original.
void Items_UpdateAndRender(const PTN *sprites, const GAME_TEXT_FONT *font);
