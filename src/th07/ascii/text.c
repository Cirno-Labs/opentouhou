#include "th07/ascii/text.h"

#include "th07/game_manager.h"

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
    popup->pos.x += g_GameManager.play_area.x;
    popup->pos.y += g_GameManager.play_area.y;
}

void AsciiText_Initialize(void)
{
    ASCII_TEXT *const text = &g_AsciiText;
    for (int32_t i = 0; i < text->string_count; i++) {
        Mem_Free(text->strings[i].text);
    }
    text->digit = (ANM_VM) {};
    text->glyph = (ANM_VM) {};
    for (int32_t i = 0; i < ASCII_STRING_COUNT; i++) {
        text->strings[i] = (ASCII_STRING) {};
    }
    text->pause_menu = (PAUSE_MENU) {};
    text->continue_menu = (CONTINUE_MENU) {};
    for (int32_t i = 0; i < ASCII_POPUP_COUNT; i++) {
        text->popups[i] = (ASCII_POPUP) {};
    }
    text->string_count = 0;
    text->in_arena = false;
    text->highlight = false;
    text->next_popup = 0;
    text->next_popup2 = 0;
    text->color = 0xffffffffu;
    text->scale_x = 1.0f;
    text->scale_y = 1.0f;
    // Retail anchors digit here, then InitVm clears the anchor again.
    Anm_InitVm(&text->digit);
    Anm_SetSprite(&text->digit, 0);
    Anm_InitVm(&text->glyph);
    Anm_SetSprite(&text->glyph, 0x20);
    text->digit.pos.z = 0.1f;
    text->glyph_width = 14;
    AsciiText_SetSummaryInterrupt(text->summary_interrupt);
}

void AsciiText_SetColor(const uint32_t color)
{
    g_AsciiText.color = color;
}

void AsciiText_SetHighlight(const bool highlight)
{
    g_AsciiText.highlight = highlight;
}

void AsciiText_SetSummaryInterrupt(const int16_t interrupt)
{
    g_AsciiText.summary.pending_interrupt = interrupt;
    g_AsciiText.summary_interrupt = interrupt;
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
