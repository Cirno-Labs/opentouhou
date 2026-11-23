#pragma once

#include "th01/grf.h"
#include "th01/stage.h"

#define HUD_RESOURCE_CAPACITY 32

typedef struct {
    uint8_t *lives, *bombs, *numbers, *stage, *timer;
    uint32_t score_previous, high_previous;
    int16_t combo_previous, max_previous;
    uint8_t maximum_combo;
    bool force_numbers;
} HUD;

typedef struct {
    const GRF *mask;
    const PTN *sprites;
    const GAME_TEXT_FONT *font;
    STAGE_GAMEPLAY *gameplay;
    uint32_t *high_score;
    int16_t *remaining_lives;
    uint16_t stage_number;
    SETTINGS_DIFFICULTY rank;
} HUD_CONTEXT;

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
void Hud_Init(HUD *hud, const ALLOCATOR *allocator);
void Hud_PutMask(const HUD_CONTEXT *context);
void Hud_SnapResources(HUD *hud, const HUD_CONTEXT *context);
void Hud_LivesPut(HUD *hud, const HUD_CONTEXT *context, int16_t previous);
void Hud_BombsPut(HUD *hud, const HUD_CONTEXT *context, int16_t previous);

// Draws both pages in source order, with persistent dirty-digit caches.
// Borrows the real gameplay score/combo, resident high score and life fields.
// maximum_combo is the separate source uint8 HUD maximum, not gameplay max.
void Hud_ScoreAndCombo(HUD *hud, const HUD_CONTEXT *context);

// Snapshots the stage label from the current page, draws it, then copies
// the 16-pixel row from page 0 to page 1 and finishes on drawing page 0.
void Hud_SnapStage(HUD *hud, const HUD_CONTEXT *context);

// Draws rank and restores/redraws the stage label on page 1 then page 0.
void Hud_PutStage(HUD *hud, const HUD_CONTEXT *context);

// Snapshots score/combo backgrounds from page 1 on first_stage. Otherwise
// restores combo backgrounds on the currently selected page. Draws score
// and high score only on first_stage, resets the displayed combo and HUD
// maximum, then copies both numeric rows from page 0 to page 1. Previous
// digit caches persist. Timer initialization/snapshots and forced numeric
// rendering are separate operations; StageTimer_Redraw supplies their full
// order.
void Hud_SnapNumbers(HUD *hud, const HUD_CONTEXT *context, bool first_stage);

// Preserves the duplicate first-quarter snapshot at x=512 for both halves.
void Hud_SnapTimer(HUD *hud, const HUD_CONTEXT *context, uint16_t time);
void Hud_PutTimer(HUD *hud, const HUD_CONTEXT *context, uint16_t time);
