#pragma once

#include "th01/video.h"

// Stores signed RGB components in the source's four-bit domain, in R, G, B
// order. Saved and resource colors normally contain values from 0 to 15.
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

// Holds one cooperative fade. Its fields belong to the Palette_ functions.
typedef struct {
    PALETTE colors;
    PALETTE_FADE_KIND kind;
    int32_t tone;
    uint8_t remaining;
    bool tone_fade;
} PALETTE_FADE;

// Initializes the stock saved palette, an empty resource palette and tone
// 100. Leaves the displayed DAC unchanged. Keep one state per game program.
void Palette_Init(PALETTE_STATE *state);

// Quantizes eight-bit resource channels to their upper nibble. Ignores alpha.
void Palette_Import(PALETTE *out, const GFX_COLOR colors[16]);

// Clamp signed components to 0 through 15, save them, and show them on video.
// Set accepts a palette that aliases state->saved or state->resource.
void Palette_SetColor(
    PALETTE_STATE *state, VIDEO *video, int32_t index, int32_t red,
    int32_t green, int32_t blue);
void Palette_Set(PALETTE_STATE *state, VIDEO *video, const PALETTE *colors);
void Palette_SetGray(PALETTE_STATE *state, VIDEO *video, int32_t value);

// Show saved colors, or a raw solid RGB color, without changing saved or
// resource colors. Hardware writes mask components rather than clamp them.
void Palette_Show(const PALETTE_STATE *state, VIDEO *video);
void Palette_ShowSolid(VIDEO *video, int32_t red, int32_t green, int32_t blue);

// Moves saved components one unit toward target, then clamps and shows them.
// With only_brighten, components greater than target stay unchanged. target
// may alias saved or resource; resource and tone remain unchanged.
void Palette_StepTo(
    PALETTE_STATE *state, VIDEO *video, const PALETTE *target,
    bool only_brighten);

// Applies the ending's integer tone from resource colors to saved entries
// 1 through 15 and shows all saved colors. Clamps tone to 0 through 200:
// 0 is black, 100 is unchanged and 200 is white. Preserves saved entry 0.
void Palette_SetTone(PALETTE_STATE *state, VIDEO *video, int32_t tone);

// Shows the resource palette directly, or applies its persistent ending tone.
void Palette_ShowResource(PALETTE_STATE *state, VIDEO *video, bool use_tone);

// Begin functions do not change the DAC. BeginFade starts a sixteen-step
// shared fade; kind must be one of the four black/white kinds. OUT copies
// saved colors at the start. IN moves only toward the live saved colors in
// its direction, leaving saved colors unchanged.
void Palette_BeginFade(
    PALETTE_FADE *fade, const PALETTE_STATE *state, PALETTE_FADE_KIND kind);

// Starts a sixteen-step fade toward live saved colors. A set bit in keep
// initializes that color from saved rather than the supplied signed RGB.
// Later saved changes affect every color, including initially kept colors.
void Palette_BeginFrom(
    PALETTE_FADE *fade, const PALETTE_STATE *state, int8_t red, int8_t green,
    int8_t blue, uint16_t keep);

// Starts an ending fade with twenty steps of five tone units. Uses the
// kind's fixed endpoint, independent of the current persistent tone.
void Palette_BeginToneFade(PALETTE_FADE *fade, PALETTE_FADE_KIND kind);

// Applies one step before showing it; returns whether a step was applied.
// After the last step, later calls leave state and DAC unchanged. Shared
// fades leave saved/resource/tone unchanged; tone fades update saved and
// tone through SetTone, using live resource colors and preserving entry 0.
// Callers supply VSync/delay timing. No operation reads a renderer or clock.
bool Palette_FadeStep(PALETTE_FADE *fade, PALETTE_STATE *state, VIDEO *video);
