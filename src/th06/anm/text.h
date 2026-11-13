// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/anm/types.h"

#include "core/base/result.h"

// Loads the text font outside game ticks: the first of the count font files
// in paths whose family is family, or the bundled font when family is
// nullptr or no file has it. The bundled font draws the characters that the
// patch font lacks. Text is bold, as the original's GDI font is, so a
// Regular patch font is emboldened. Reports failure only when the bundled
// font does not open. Rendering still updates shown while text is
// uninitialized, so headless play needs no font setup.
RESULT Anm_InitText(
    const char *family, const char *const *paths, int32_t count);
void Anm_ShutdownText(void);

// Replaces a 16-pixel texture row with text in thcrap's layout markup, as
// Text_RenderMarkup draws it. Uses UTF-8 and COLORREF colors
// with red in the low byte. Disables the shadow for 0xffffffff. Defaults
// font_height to 15 when it is not positive. Draws the glyphs at double
// size and scales them to the row, as the game does. The game scales a
// region 2 pixels narrower and lower than the doubled row, which stretches
// the text vertically. With nmlgc/base_tsa, the region is twice the row
// width by 32 pixels, as its executable patches make it, so the text keeps
// its proportions. Shows the translator note of text, as Note_Show does.
RESULT Anm_RenderTextToTextureIndex(
    int32_t texture, int32_t x, int32_t y, int32_t width, int32_t font_height,
    uint32_t color, uint32_t shadow, const char *text);

// Measures text in thcrap's layout markup as a row of font_height draws it,
// in texture pixels. Gives 0 while text is uninitialized.
RESULT Anm_MeasureText(const char *text, int32_t font_height, float *width);

// Formats text into the row of the VM's sprite, as wide as the sprite, and
// sets shown. Right and centered text align by the measured width scaled
// by (h + 1) / h for the font height h. With the bundled font, this is the
// original's rule of h + 1 pixels for each CP932 byte.
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderText(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderTextRightAligned(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderTextCentered(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
