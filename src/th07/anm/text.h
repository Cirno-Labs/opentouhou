#pragma once

#include "th07/anm/types.h"

#include "core/base/result.h"

// Opens the text font outside game ticks, as GameFont_Open
// (stg/game_font.h) opens it for family. Without a font, logs a warning
// and draws no text, and still updates shown, so headless play needs no
// font setup; reports failure only when font rasterization does not
// initialize.
RESULT Anm_InitText(const char *family);
void Anm_ShutdownText(void);

// Replaces rows y to y + dest_height of texture with UTF-8 text at x, drawn
// at twice font_height and scaled down. Colors are COLORREFs, red in the
// low byte; a shadow of 0xffffffff thins the black outline.
RESULT Anm_RenderTextToTextureIndex(
    int32_t texture, int32_t x, int32_t y, int32_t width, int32_t font_height,
    int32_t dest_height, uint32_t color, uint32_t shadow, const char *text,
    float scale_x, float scale_y);

// Formats text into the rows of the VM's sprite and sets shown. Right and
// centered text align by the measured width times h / (h - 1), for font
// height h.
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderText(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderTextRightAligned(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
PRINTF_FORMAT(4, 5)
RESULT Anm_RenderTextCentered(
    ANM_VM *vm, uint32_t color, uint32_t shadow, const char *format, ...);
