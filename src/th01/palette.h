#pragma once

#include "th01/interleave.h"
#include "th01/video.h"

// Holds signed RGB components in the original's four-bit domain, in R, G, B
// order. Saved and resource colors normally hold 0 through 15.
typedef struct {
    int8_t colors[16][3];
} PALETTE;

typedef struct {
    PALETTE saved;
    PALETTE resource;
    int32_t tone;
} PALETTE_STATE;

typedef enum {
    PALETTE_FADE_BLACK_IN,
    PALETTE_FADE_BLACK_OUT,
    PALETTE_FADE_WHITE_IN,
    PALETTE_FADE_WHITE_OUT,
} PALETTE_FADE_KIND;

// Holds one cooperative fade.
typedef struct {
    PALETTE colors;
    PALETTE_FADE_KIND kind;
    uint8_t remaining;
} PALETTE_FADE;

// The one live gameplay palette, and the color it is restored to after a
// boss hit flash or a fade.
extern const PALETTE_STATE *const g_Palette;
extern const PALETTE *const g_StagePalette;

// Sets the colors that a boss hit flash or a fade restores.
void Palette_SetStage(const PALETTE *colors);

// Clears the stage-restore color, e.g. when leaving gameplay.
void Palette_ResetStage(void);

// Replaces the live resource palette outright, without showing it.
void Palette_SetResource(const PALETTE *colors);

// Sets the stock saved palette, an empty resource palette and tone 100,
// leaving the DAC unchanged.
void Palette_Init(PALETTE_STATE *state);

// Quantizes eight-bit resource channels to their upper nibble. Ignores alpha.
void Palette_Import(PALETTE *out, const GFX_COLOR colors[16]);

// Clamps signed components to 0 through 15, saves them and shows them.
// colors may alias state->saved or state->resource.
void Palette_SetColor(
    PALETTE_STATE *state, int32_t index, int32_t red, int32_t green,
    int32_t blue);
void Palette_Set(PALETTE_STATE *state, const PALETTE *colors);
void Palette_SetGray(PALETTE_STATE *state, int32_t value);

// Shows a raw solid color without changing saved or resource colors. Masks
// components to four bits instead of clamping.
void Palette_ShowSolid(int32_t red, int32_t green, int32_t blue);

// Moves saved components one unit toward target, then clamps and shows them.
// With only_brighten, components above target stay unchanged.
void Palette_StepTo(
    PALETTE_STATE *state, const PALETTE *target, bool only_brighten);

// Applies tone 0 (black) to 200 (white), 100 unchanged, to entries 1..15
// and shows all saved colors.
void Palette_SetTone(PALETTE_STATE *state, int32_t tone);

// Shows the resource palette directly, or applies its persistent ending tone.
void Palette_ShowResource(PALETTE_STATE *state, bool use_tone);

// Starts a sixteen-step black or white fade without changing the DAC.
// OUT fades from the saved colors at the start; IN toward the live ones.
void Palette_BeginFade(
    PALETTE_FADE *fade, const PALETTE_STATE *state, PALETTE_FADE_KIND kind);

// Applies and shows one step of fade, and returns false once all steps are
// done.
bool Palette_FadeStep(PALETTE_FADE *fade, PALETTE_STATE *state);

// The functions below act on the one live gameplay palette (g_Palette and
// g_StagePalette) for callers outside palette.c, in place of a writable
// view of those globals.

// Clamps a single saved component to 0 through 15, saves it and shows it.
void Palette_SetGlobalColor(
    int32_t index, int32_t red, int32_t green, int32_t blue);

// Sets all sixteen saved colors from colors (may alias g_Palette's own).
void Palette_SetGlobal(const PALETTE *colors);

void Palette_SetGlobalGray(int32_t value);

// Sets the stock saved palette, an empty resource palette and tone 100.
void Palette_ResetGlobal(void);

// Moves the saved components one unit toward target, then shows them.
void Palette_StepGlobalTo(const PALETTE *target, bool only_brighten);

void Palette_BeginGlobalFade(PALETTE_FADE *fade, PALETTE_FADE_KIND kind);
bool Palette_GlobalFadeStep(PALETTE_FADE *fade);

// Overwrites one saved (resp. stage-restore) color component directly, for
// the per-boss decay/step loops that touch a single channel at a time.
void Palette_SetGlobalSavedComponent(
    int32_t index, int32_t component, int8_t value);
void Palette_SetStageComponent(int32_t index, int32_t component, int8_t value);
void Palette_SetGlobalResourceComponent(
    int32_t index, int32_t component, int8_t value);

// Quantizes colors (as Palette_Import) into the live resource palette,
// resp. the stage-restore palette, without showing it.
void Palette_ImportResource(const GFX_COLOR colors[16]);
void Palette_ImportStage(const GFX_COLOR colors[16]);

// Drives one interleaved diagonal transition (as Transition_BeginDiagonal /
// Transition_Advance) of the live saved palette toward target.
bool Palette_BeginGlobalDiagonalTransition(
    const PALETTE *target, const INTERLEAVE_ASSETS *assets, uint16_t step_ms,
    uint64_t host_tick);
bool Palette_AdvanceGlobalDiagonalTransition(
    const PALETTE *target, const INTERLEAVE_ASSETS *assets, uint64_t host_tick);
