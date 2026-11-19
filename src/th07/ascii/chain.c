#include "th07/ascii/text.h"

#include "th07/ascii/pause_menu.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"

#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/math/x87.h"

#include <math.h>
#include <string.h>

static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;
static CHAIN_ELEM m_DrawElem2;

static void M_SetViewport(const bool in_arena)
{
    VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    viewport->x = in_arena ? (float)(uint32_t)g_GameManager.play_area.x : 0.0f;
    viewport->y = in_arena ? (float)(uint32_t)g_GameManager.play_area.y : 0.0f;
    viewport->width =
        in_arena ? (float)(uint32_t)g_GameManager.play_area.width : 640.0f;
    viewport->height =
        in_arena ? (float)(uint32_t)g_GameManager.play_area.height : 480.0f;
}

static ANM_DRAW M_ScreenDraw(const ANM_VIEW view)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = Anm_ScreenView(g_GameManager.camera.viewport, view);
    return draw;
}

static void M_SetRgb(
    ANM_VM *const vm, const uint8_t red, const uint8_t green,
    const uint8_t blue)
{
    vm->color = (vm->color & 0xff000000u) | (uint32_t)red << 16
        | (uint32_t)green << 8 | blue;
}

static void M_SetAlpha(ANM_VM *const vm, const uint8_t alpha)
{
    vm->color = (vm->color & 0x00ffffffu) | (uint32_t)alpha << 24;
}

static void M_RunScripts(ASCII_TEXT *const text)
{
    Anm_Step(&text->summary);
    Anm_Step(&text->summary_digit);
    for (int32_t i = 0; i < ASCII_BOSS_MARKER_COUNT; i++) {
        Anm_Step(&text->boss_markers[i]);
    }
    Anm_Step(&text->summary_bomb_icon);
}

static void M_StartScripts(ASCII_TEXT *const text)
{
    Anm_StartScript(&text->summary, 4);
    Anm_StartScript(&text->summary_digit, 3);
    Anm_StartScript(&text->summary_bomb_icon, 5);
    for (int32_t i = 0; i < ASCII_BOSS_MARKER_COUNT; i++) {
        Anm_StartScript(&text->boss_markers[i], 6);
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    ASCII_TEXT *const text = arg;
    if (g_GameManager.mode.paused == 0
        && !g_GameManager.mode.in_continue_menu) {
        for (int32_t i = 0; i < ASCII_POPUP_COUNT; i++) {
            ASCII_POPUP *const popup = &text->popups[i];
            if (!popup->active) {
                continue;
            }
            popup->pos.y -= 0.5f * g_GameSpeed;
            Timer_TickAt(&popup->timer, g_GameSpeed);
            if (popup->timer.current > 60) {
                popup->active = false;
            }
        }
    } else if (g_GameManager.mode.paused != 0) {
        PauseMenu_UpdatePauseMenu(&text->pause_menu);
    }
    if (g_GameManager.mode.in_continue_menu) {
        PauseMenu_UpdateContinueMenu(&text->continue_menu);
    }
    M_RunScripts(text);
    if (g_GameManager.mode.in_demo) {
        if (text->demo_icon.script_index == 0) {
            Anm_StartScript(&text->demo_icon, 7);
        }
        Anm_Step(&text->demo_icon);
    } else {
        text->demo_icon.script_index = 0;
    }
    return CHAIN_CONTINUE;
}

static void M_DrawBossMarkers(ASCII_TEXT *const text, const ANM_DRAW *draw)
{
    for (int32_t i = 0; i < ASCII_BOSS_MARKER_COUNT; i++) {
        ANM_VM *const marker = &text->boss_markers[i];
        if (marker->pos.x < 56.0f || marker->pos.x > 392.0f) {
            continue;
        }
        const float distance = fabsf(marker->pos.x - 32.0f - g_Player.pos.x);
        if (distance < 64.0f) {
            M_SetAlpha(
                marker, (uint8_t)X87_Ftol(distance * 128.0f / 64.0f + 48.0f));
        } else {
            M_SetAlpha(marker, 0xb0);
        }
        if (text->boss_marker_blue[i]) {
            M_SetAlpha(marker, 0x80);
            M_SetRgb(marker, 0x40, 0x40, 0xff);
        } else {
            M_SetRgb(marker, 0xff, 0xff, 0xff);
        }
        ANM_DRAW marker_draw = *draw;
        marker_draw.interpolate = true;
        Anm_DrawUnrotated(marker, &marker_draw);
    }
}

static void M_DrawStrings(ASCII_TEXT *const text)
{
    bool clipped = true;
    text->glyph.shown = true;
    text->glyph.anchor = 3;
    for (int32_t i = 0; i < text->string_count; i++) {
        const ASCII_STRING *const string = &text->strings[i];
        text->glyph.pos = string->pos;
        text->glyph.scale_x = string->scale_x;
        text->glyph.scale_y = string->scale_y;
        const float advance = (float)text->glyph_width * string->scale_x;
        if (clipped != string->in_arena) {
            clipped = string->in_arena;
            M_SetViewport(clipped);
        }
        const ANM_DRAW draw =
            M_ScreenDraw(clipped ? ANM_VIEW_PLAY_AREA : ANM_VIEW_HUD);
        for (const uint8_t *c = (const uint8_t *)string->text; *c != 0; c++) {
            if (*c == '\n') {
                text->glyph.pos.y += 16.0f * string->scale_y;
                text->glyph.pos.x = string->pos.x;
            } else if (*c == ' ') {
                text->glyph.pos.x += advance;
            } else {
                if (!string->highlight) {
                    Anm_SetSprite(&text->glyph, *c - 1);
                    text->glyph.color = string->color;
                } else {
                    Anm_SetSprite(&text->glyph, *c + 0x7c);
                    text->glyph.color = COLOR_WHITE;
                }
                Anm_DrawUnrotated(&text->glyph, &draw);
                text->glyph.pos.x += advance;
            }
        }
    }
    const ANM_DRAW draw =
        M_ScreenDraw(clipped ? ANM_VIEW_PLAY_AREA : ANM_VIEW_HUD);
    M_DrawBossMarkers(text, &draw);
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    ASCII_TEXT *const text = arg;
    M_DrawStrings(text);
    for (int32_t i = 0; i < text->string_count; i++) {
        Mem_Free(text->strings[i].text);
        text->strings[i].text = nullptr;
    }
    text->string_count = 0;
    PauseMenu_DrawPauseMenu(&text->pause_menu);
    PauseMenu_DrawContinueMenu(&text->continue_menu);
    if (text->demo_icon.script_index != 0) {
        ANM_DRAW draw = M_ScreenDraw(ANM_VIEW_HUD);
        draw.interpolate = true;
        Anm_DrawUnrotated(&text->demo_icon, &draw);
    }
    return CHAIN_CONTINUE;
}

static uint8_t M_PopupAlpha(const ASCII_POPUP *const popup)
{
    const float dx = g_Player.pos.x - popup->pos.x;
    const float dy = g_Player.pos.y - popup->pos.y;
    const int32_t distance = X87_Ftol(dx * dx + dy * dy);
    if (distance > 0x1000) {
        return 0xd0;
    }
    if (distance > 0x400) {
        return (uint8_t)((distance - 0x400) * 0x80 / 0xc00 + 0x50);
    }
    return 0x50;
}

static void M_DrawPopup(
    ASCII_TEXT *const text, const ASCII_POPUP *const popup, ANM_DRAW *draw)
{
    text->digit.pos.x = popup->pos.x - (float)(popup->digit_count << 2);
    text->digit.pos.y = popup->pos.y;
    text->digit.color = popup->color;
    const uint8_t alpha = M_PopupAlpha(popup);
    for (int32_t i = popup->digit_count - 1; i >= 0; i--) {
        const uint8_t digit = popup->digits[i];
        if (popup->timer.current < 52 || digit == 10) {
            Anm_SetSprite(&text->digit, digit);
        } else if (popup->timer.current < 56) {
            Anm_SetSprite(&text->digit, digit + 0xb);
        } else {
            Anm_SetSprite(&text->digit, digit + 0x15);
        }
        M_SetAlpha(&text->digit, alpha);
        ANM_VM vm = text->digit;
        vm.presentation_generation = popup->presentation_generation;
        draw->part = (uint32_t)i;
        Anm_DrawUnrotated(&vm, draw);
        text->digit.pos.x += 8.0f;
    }
}

// Draws count digits of value from the place of div, leading zeros
// blank, step pixels apart.
static void M_DrawSummaryNumber(
    ASCII_TEXT *const text, int32_t value, int32_t div, const int32_t count,
    const float step, ANM_DRAW *const draw)
{
    bool nonzero = false;
    for (int32_t i = 0; i < count; i++, div /= 10) {
        const int32_t digit = value / div;
        value = value % div;
        if (digit != 0) {
            nonzero = true;
        }
        if (nonzero || div == 1) {
            Anm_SetSprite(&text->summary_digit, digit + 0x84);
            Anm_DrawUnrotated(&text->summary_digit, draw);
            draw->part++;
        }
        text->summary_digit.pos.x += step;
    }
}

static void M_DrawSummary(ASCII_TEXT *const text, ANM_DRAW *const draw)
{
    ANM_VM *const digit = &text->summary_digit;
    const int32_t offset = g_GameManager.cherry.offset;
    draw->part = 0;
    Anm_DrawUnrotated(&text->summary, draw);
    draw->part++;
    digit->pos.x = text->summary.pos.x + 40.0f + 6.0f;
    digit->pos.y = text->summary.pos.y + 11.0f;
    digit->pos.z = text->summary.pos.z;
    M_SetAlpha(digit, (uint8_t)(text->summary.color >> 24));
    const int32_t cherry = g_GameManager.cherry.value - offset;
    if (g_GameManager.cherry.value >= g_GameManager.cherry.max) {
        M_SetRgb(digit, 0x80, 0xd0, 0xff);
    } else if (cherry >= 50000) {
        M_SetRgb(digit, 0x80, 0xff, 0xff);
    } else {
        M_SetRgb(digit, 0xff, 0xff, 0xff);
    }
    M_DrawSummaryNumber(text, cherry, 100000, 6, 7.0f, draw);

    const int32_t max = g_GameManager.cherry.max - offset;
    M_SetRgb(digit, 0xe0, 0xd0, 0xf0);
    digit->pos.x += 9.0f;
    if (max < 1000000) {
        M_DrawSummaryNumber(text, max, 100000, 6, 7.0f, draw);
    } else {
        M_DrawSummaryNumber(text, max, 1000000, 7, 7.0f, draw);
    }
    digit->scale_x = 1.0f;
    digit->scale_y = 1.0f;

    digit->pos.x = text->summary.pos.x + 40.0f + 6.0f + 7.0f;
    digit->pos.y = text->summary.pos.y + 2.0f;
    const int32_t plus = g_GameManager.cherry.plus - offset;
    float step;
    if (g_Player.border_state != PLAYER_BORDER_NONE) {
        int32_t pulse = plus % 4000;
        if (pulse >= 2000) {
            pulse = 4000 - pulse;
        }
        const uint8_t level =
            (uint8_t)(plus * 0xc0 / 50000 + pulse * 0x40 / 2000);
        M_SetRgb(digit, 0xff, level, level);
        digit->scale_x = 1.41f;
        digit->scale_y = 1.41f;
        step = 10.0f;
        digit->pos.x += 2.0f;
        digit->pos.y -= 2.0f;
    } else {
        M_SetRgb(digit, 0xc0, 0x80, 0xb0);
        step = 7.0f;
    }
    M_DrawSummaryNumber(text, plus, 10000, 5, step, draw);
    digit->scale_x = 1.0f;
    digit->scale_y = 1.0f;
    if (g_Player.border_state == PLAYER_BORDER_RUNNING) {
        text->summary_bomb_icon.pos = text->summary.pos;
        text->summary_bomb_icon.pos.x += 24.0f;
        text->summary_bomb_icon.pos.y += 8.0f;
        text->summary_bomb_icon.color = text->summary.color;
        Anm_DrawUnrotated(&text->summary_bomb_icon, draw);
    }
}

static CHAIN_STATUS M_Draw2(void *const arg)
{
    ASCII_TEXT *const text = arg;
    ANM_DRAW draw = M_ScreenDraw(ANM_VIEW_PLAY_AREA);
    draw.depth_test = GFX_DEPTH_ALWAYS;
    draw.interpolate = true;
    for (int32_t i = 0; i < ASCII_POPUP_COUNT; i++) {
        const ASCII_POPUP *const popup = &text->popups[i];
        if (popup->active) {
            M_DrawPopup(text, popup, &draw);
        }
    }
    if (text->summary.shown) {
        M_DrawSummary(text, &draw);
    }
    return CHAIN_CONTINUE;
}

static RESULT M_Added(void *const arg)
{
    const bool capture_background = g_AsciiText.capture_background;
    const bool highlight_font = g_AsciiText.highlight_font;
    memset(&g_AsciiText, 0, sizeof(g_AsciiText));
    g_AsciiText.capture_background = capture_background;
    g_AsciiText.highlight_font = highlight_font;
    MUST(Anm_Load(1, "data/ascii.anm", 0));
    MUST(Anm_Load(4, "data/capture.anm", 0x724));
    AsciiText_Initialize();
    M_StartScripts(&g_AsciiText);
    return OK;
}

static void M_Deleted(void *const arg)
{
    Anm_Release(1);
    Anm_Release(2);
    Anm_Release(4);
    Anm_Release(3);
    for (int32_t i = 0; i < g_AsciiText.string_count; i++) {
        Mem_Free(g_AsciiText.strings[i].text);
        g_AsciiText.strings[i].text = nullptr;
    }
    g_AsciiText.string_count = 0;
}

RESULT AsciiText_Register(void)
{
    Chain_InitElem(&m_CalcElem, M_Update, &g_AsciiText);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, 1));
    Chain_InitElem(&m_DrawElem, M_Draw, &g_AsciiText);
    MUST(Chain_AddDraw(&g_Chain, &m_DrawElem, 16));
    Chain_InitElem(&m_DrawElem2, M_Draw2, &g_AsciiText);
    return Chain_AddDraw(&g_Chain, &m_DrawElem2, 11);
}

void AsciiText_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
}
