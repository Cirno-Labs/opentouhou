#pragma once

// The 16-color analog palette and the tone it shows at: palette files,
// the stock colors, and the fades that step the tone toward black or white.

#include "core/io/reader.h"
#include "th02/video.h"

typedef struct {
    uint8_t colors[16][3];
    int16_t tone;
} PALETTE_STATE;

extern const PALETTE_STATE *const g_Palette;

typedef enum {
    PALETTE_BLACK_IN,
    PALETTE_BLACK_OUT,
    PALETTE_WHITE_IN,
    PALETTE_WHITE_OUT,
} PALETTE_FADE_KIND;

typedef struct {
    int16_t speed;
    int16_t target;
    int16_t delta;
    uint16_t wait;
    bool shown;
    bool active;
} PALETTE_FADE;

// Sets the stock colors and tone without showing them.
void Palette_Init(void);
void Palette_DefaultColors(uint8_t colors[16][3]);

// Shows the stock colors at tone 100.
void Palette_Reset(void);

// Imports all eight channel bits and ignores alpha.
void Palette_Import(const GFX_COLOR colors[16]);

// Reads 4-bit RGB as value | (value << 4); as in retail, a short read keeps
// the unread channels and expands them too.
RESULT Palette_ReadRgb(READER *reader, uint8_t colors[16][3]);

// Recolors the current palette in place from the reader's bytes.
RESULT Palette_Recolor(READER *reader);

// Shows the palette at tone clamped to 0..200.
void Palette_Show(void);
void Palette_SetTone(int16_t tone);

// Sets tone without showing it.
void Palette_SetToneQuiet(int16_t tone);

// Sets one color's channels without showing them.
void Palette_SetColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

// Starts a fade, showing its first tone at the next edge.
void Palette_BeginFade(
    PALETTE_FADE *fade, PALETTE_FADE_KIND kind, int16_t speed);

// Steps the fade, 18 palettes six tones apart, speed edges each; returns
// true when done. Nonpositive speed finishes at once.
bool Palette_FadeVSync(PALETTE_FADE *fade);
