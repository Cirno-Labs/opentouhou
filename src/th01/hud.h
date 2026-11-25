#pragma once

#include "th01/grf.h"
#include "th01/stage.h"

// Caches fullwidth numerals and separate ASCII stage, rank and timer glyphs
// from rom.
RESULT Hud_LoadFont(
    const FONT_ROM *rom, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);
RESULT Hud_ValidateAssets(
    const GRF *mask, const PTN *sprites, const GAME_TEXT_FONT *font);

// Allocates mutable raw snapshots in a caller-owned asset arena. Initialize
// once per main scene, preserving snapshots and digit caches between stages.
// Stage numbers must be 0..99. Resource counts must be 0..32; normal play uses
// at most six. Coordinates outside the visible page are clipped for debug
// counts. No immutable PTN asset is changed. Updates allocate nothing and read
// no external devices.
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

// Draws both pages in source order, with persistent dirty-digit caches.
// Borrows the real gameplay score/combo, resident high score and life fields.
// maximum_combo is the separate source uint8 HUD maximum, not gameplay max.
void Hud_ScoreAndCombo(void);

// Snapshots the stage label from the current page, draws it, then copies
// the 16-pixel row from page 0 to page 1 and finishes on drawing page 0.
void Hud_SnapStage(void);

// Draws rank and restores/redraws the stage label on page 1 then page 0.
void Hud_PutStage(void);

// Snapshots score/combo backgrounds from page 1 on first_stage. Otherwise
// restores combo backgrounds on the currently selected page. Draws score
// and high score only on first_stage, resets the displayed combo and HUD
// maximum, then copies both numeric rows from page 0 to page 1. Previous
// digit caches persist. Timer initialization/snapshots and forced numeric
// rendering are separate operations; StageTimer_Redraw supplies their full
// order.
void Hud_SnapNumbers(bool first_stage);

// Preserves the duplicate first-quarter snapshot at x=512 for both halves.
void Hud_SnapTimer(uint16_t time);
void Hud_PutTimer(uint16_t time);

// Restores the stage HUD and redraws all numeric values.
void Hud_Redraw(bool first_stage, uint16_t time);

// Draws a centered red text line of at most five characters.
void Hud_PutMessage(const char *text);
