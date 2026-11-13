// SPDX-License-Identifier: GPL-3.0-only

#include "th06/ascii/text.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include <stdarg.h>

ASCII_TEXT g_AsciiText;

static void M_AddPopup(
    ASCII_POPUP *const popup, const VEC3 pos, int32_t value,
    const uint32_t color)
{
    popup->active = true;
    Anm_RenewPresentation(&g_AsciiText.digit);
    popup->presentation_generation = g_AsciiText.digit.presentation_generation;
    uint8_t count = 0;
    if (value >= 0) {
        while (value != 0) {
            popup->digits[count++] = (uint8_t)(value % 10);
            value /= 10;
        }
    } else {
        popup->digits[count++] = 10;
    }
    if (count == 0) {
        popup->digits[count++] = 0;
    }
    popup->digit_count = count;
    popup->color = color;
    Timer_Set(&popup->timer, 0);
    popup->pos = pos;
}

void AsciiText_Initialize(void)
{
    for (int32_t i = 0; i < g_AsciiText.string_count; i++) {
        Mem_Free(g_AsciiText.strings[i].text);
    }
    const bool capture_background = g_AsciiText.capture_background;
    const bool highlight_font = g_AsciiText.highlight_font;
    g_AsciiText = (ASCII_TEXT) {
        .color = 0xffffffffu,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
        .capture_background = capture_background,
        .highlight_font = highlight_font,
    };
    g_AsciiText.digit.anchor = 3;
    Anm_InitVm(&g_AsciiText.digit);
    Anm_SetSprite(&g_AsciiText.digit, 0);
    Anm_InitVm(&g_AsciiText.glyph);
    Anm_SetSprite(&g_AsciiText.glyph, 0x20);
    g_AsciiText.digit.pos.z = 0.1f;
}

void AsciiText_SetColor(const uint32_t color)
{
    g_AsciiText.color = color;
}

void AsciiText_SetHighlight(const bool highlight)
{
    g_AsciiText.highlight = highlight;
}

void AsciiText_AddString(const VEC3 pos, const char *const text)
{
    if (g_AsciiText.string_count >= ASCII_STRING_COUNT) {
        return;
    }
    ASCII_STRING *const string =
        &g_AsciiText.strings[g_AsciiText.string_count++];
    *string = (ASCII_STRING) {
        .text = Str_Dup(text),
        .pos = pos,
        .color = g_AsciiText.color,
        .scale_x = g_AsciiText.scale_x,
        .scale_y = g_AsciiText.scale_y,
        .in_arena = g_AsciiText.in_arena,
        .highlight = g_AsciiText.highlight_font && g_AsciiText.highlight,
    };
}

void AsciiText_AddFormat(const VEC3 pos, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    char *const text = Str_FormatV(format, args);
    va_end(args);
    AsciiText_AddString(pos, text);
    Mem_Free(text);
}

void AsciiText_AddPopup(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
    if (g_AsciiText.next_popup >= ASCII_NORMAL_POPUP_COUNT) {
        g_AsciiText.next_popup = 0;
    }
    M_AddPopup(&g_AsciiText.popups[g_AsciiText.next_popup], pos, value, color);
    g_AsciiText.next_popup++;
}

void AsciiText_AddPopup2(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
    if (g_AsciiText.next_popup2
        >= ASCII_POPUP_COUNT - ASCII_NORMAL_POPUP_COUNT) {
        g_AsciiText.next_popup2 = 0;
    }
    M_AddPopup(
        &g_AsciiText.popups[ASCII_NORMAL_POPUP_COUNT + g_AsciiText.next_popup2],
        pos, value, color);
    g_AsciiText.next_popup2++;
}
