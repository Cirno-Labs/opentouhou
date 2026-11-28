#pragma once

#include "th01/grf.h"
#include "th01/stage.h"

// Caches fullwidth numerals and separate ASCII stage, rank and timer glyphs
// from rom.
RESULT HudAssets_LoadFont(
    const FONT_ROM *rom, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);
RESULT Hud_ValidateAssets(
    const GRF *mask, const PTN *sprites, const GAME_TEXT_FONT *font);

// Allocates the HUD's background snapshots in a caller-owned arena.
// Initialize once per main scene.
void Hud_Init(const ALLOCATOR *allocator);

// Selects the HUD mask, the stage sprites for the life and bomb icons and
// the font. The selection persists across Hud_Reset.
void Hud_Load(const GRF *mask, const PTN *sprites, const GAME_TEXT_FONT *font);

// Forgets the previously drawn values and keeps the snapshot storage.
void Hud_Reset(void);
void Hud_PutMask(void);
void Hud_SnapResources(void);
void Hud_LivesPut(int16_t previous);
void Hud_BombsPut(int16_t previous);

// Redraws the changed digits of score, high score and combo on both pages.
void Hud_ScoreAndCombo(void);

// Snapshots the stage label from the current page, draws it, then copies
// the 16-pixel row from page 0 to page 1 and finishes on drawing page 0.
void Hud_SnapStage(void);

// Draws rank and restores/redraws the stage label on page 1 then page 0.
void Hud_PutStage(void);

// Snapshots the number backgrounds on first_stage, else restores the combo;
// resets the displayed combo and copies the rows to page 1.
void Hud_SnapNumbers(bool first_stage);

// Preserves the duplicate first-quarter snapshot at x=512 for both halves.
void Hud_SnapTimer(uint16_t time);
void Hud_PutTimer(uint16_t time);

// Restores the stage HUD and redraws all numeric values.
void Hud_Redraw(bool first_stage, uint16_t time);

// Draws a centered red text line of at most five characters.
void Hud_PutMessage(const char *text);
