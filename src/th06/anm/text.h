#pragma once

#include "th06/anm/types.h"

#include "core/base/result.h"

// Loads the text font outside game ticks. Rendering still updates shown
// while text is uninitialized, so headless play needs no font setup.
RESULT Anm_InitText(void);
void Anm_ShutdownText(void);

// Replaces a 16-pixel texture row with text. Uses UTF-8 and COLORREF colors
// with red in the low byte. Disables the shadow for 0xffffffff. Defaults
// font_height to 15 when it is not positive. Scales double-size glyphs
// through the text surface before replacing the row, as the game does.
RESULT Anm_RenderTextToTextureIndex(
    int32_t texture, int32_t x, int32_t y, int32_t width, int32_t font_height,
    uint32_t color, uint32_t shadow, const char *text);

// Formats text into the VM's texture and sets shown. Computes alignment
// from the CP932 byte count, so Japanese characters count as two bytes.
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderText(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderTextRightAligned(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderTextCentered(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
