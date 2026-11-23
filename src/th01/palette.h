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
    PALETTE_FADE_FROM,
} PALETTE_FADE_KIND;

// Holds one cooperative fade.
typedef struct {
    PALETTE colors;
    PALETTE_FADE_KIND kind;
    int32_t tone;
    uint8_t remaining;
    bool tone_fade;
} PALETTE_FADE;

// Sets the stock saved palette, an empty resource palette and tone 100,
// leaving the DAC unchanged.
void Palette_Init(PALETTE_STATE *state);

// Quantizes eight-bit resource channels to their upper nibble. Ignores alpha.
void Palette_Import(PALETTE *out, const GFX_COLOR colors[16]);

// Clamps signed components to 0 through 15, saves them and shows them.
// colors may alias state->saved or state->resource.
void Palette_SetColor(
    PALETTE_STATE *state, VIDEO *video, int32_t index, int32_t red,
    int32_t green, int32_t blue);
void Palette_Set(PALETTE_STATE *state, VIDEO *video, const PALETTE *colors);
void Palette_SetGray(PALETTE_STATE *state, VIDEO *video, int32_t value);

// Shows the saved colors, or a raw solid color, without changing saved or
// resource colors. Masks components to four bits instead of clamping.
void Palette_Show(const PALETTE_STATE *state, VIDEO *video);
void Palette_ShowSolid(VIDEO *video, int32_t red, int32_t green, int32_t blue);

// Moves saved components one unit toward target, then clamps and shows them.
// With only_brighten, components above target stay unchanged. target may
// alias saved or resource.
void Palette_StepTo(
    PALETTE_STATE *state, VIDEO *video, const PALETTE *target,
    bool only_brighten);

// Applies the ending's integer tone from the resource colors to saved
// entries 1 through 15 and shows all saved colors. Clamps tone to 0 through
// 200: 0 is black, 100 is unchanged and 200 is white. Keeps saved entry 0.
void Palette_SetTone(PALETTE_STATE *state, VIDEO *video, int32_t tone);

// Shows the resource palette directly, or applies its persistent ending tone.
void Palette_ShowResource(PALETTE_STATE *state, VIDEO *video, bool use_tone);

// Starts a sixteen-step fade without changing the DAC; kind must be one of
// the four black/white kinds. OUT copies the saved colors at the start. IN
// moves toward the live saved colors, and only in its own direction.
void Palette_BeginFade(
    PALETTE_FADE *fade, const PALETTE_STATE *state, PALETTE_FADE_KIND kind);

// Starts a sixteen-step fade from a solid color toward the live saved
// colors. A set bit in keep starts that color from saved instead.
void Palette_BeginFrom(
    PALETTE_FADE *fade, const PALETTE_STATE *state, int8_t red, int8_t green,
    int8_t blue, uint16_t keep);

// Starts an ending fade of twenty steps of five tone units from the kind's
// fixed endpoint, whatever the current tone.
void Palette_BeginToneFade(PALETTE_FADE *fade, PALETTE_FADE_KIND kind);

// Applies and shows one step of fade, and returns false once all steps are
// done. Tone fades update saved and tone through Palette_SetTone.
bool Palette_FadeStep(PALETTE_FADE *fade, PALETTE_STATE *state, VIDEO *video);
