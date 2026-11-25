#pragma once

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
extern PALETTE_STATE g_Palette;
extern PALETTE g_StagePalette;

// Sets the colors that a boss hit flash or a fade restores.
void Palette_SetStage(const PALETTE *colors);

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
