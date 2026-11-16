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
    VIDEO *video;
    const GRF *mask;
    const PTN *sprites;
    const GAME_TEXT_FONT *font;
    STAGE_GAMEPLAY *gameplay;
    PLAYER *player;
    uint32_t *high_score;
    int16_t *remaining_lives;
    uint16_t stage_number;
    SETTINGS_DIFFICULTY rank;
} HUD_CONTEXT;

// Caches fullwidth numerals and separate ASCII stage, rank and timer glyphs.
// Uses the portable font substitute; no ROM pixel equivalence is claimed.
RESULT Hud_LoadFont(
    FONT *font, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);
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

// Snaps stage and resources only on first_stage. Resets the displayed combo
// and its HUD maximum on every call. Draws rank and stage on both pages.
// Timer initialization, timer snapshots and final forced numeric rendering
// are separate operations supplied by the gameplay owner.
void Hud_SnapStage(HUD *hud, const HUD_CONTEXT *context);
void Hud_PutStage(HUD *hud, const HUD_CONTEXT *context);
void Hud_SnapNumbers(HUD *hud, const HUD_CONTEXT *context, bool first_stage);

// Preserves the duplicate first-quarter snapshot at x=512 for both halves.
void Hud_SnapTimer(HUD *hud, const HUD_CONTEXT *context, uint16_t time);
void Hud_PutTimer(HUD *hud, const HUD_CONTEXT *context, uint16_t time);
