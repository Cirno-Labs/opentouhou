#include "th06/ascii/text.h"

#include "th06/ascii/pause_menu.h"
#include "th06/game.h"
#include "th06/game_manager.h"

#include "core/base/memory.h"

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
            popup->pos.y -= 0.5f;
            Timer_Tick(&popup->timer);
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
    return CHAIN_CONTINUE;
}

static void M_DrawStrings(void)
{
    ASCII_TEXT *const text = &g_AsciiText;
    bool clipped = true;
    text->glyph.shown = true;
    text->glyph.anchor = 3;
    for (int32_t i = 0; i < text->string_count; i++) {
        const ASCII_STRING *const string = &text->strings[i];
        text->glyph.pos = string->pos;
        text->glyph.scale_x = string->scale_x;
        text->glyph.scale_y = string->scale_y;
        const float advance = 14.0f * string->scale_x;
        if (clipped != string->in_arena) {
            clipped = string->in_arena;
            M_SetViewport(clipped);
        }
        for (const uint8_t *c = (const uint8_t *)string->text; *c != 0; c++) {
            if (*c == '\n') {
                text->glyph.pos.y += 16.0f * string->scale_y;
                text->glyph.pos.x = string->pos.x;
            } else if (*c == ' ') {
                text->glyph.pos.x += advance;
            } else {
                if (!string->highlight) {
                    Anm_SetSprite(&text->glyph, (int32_t)*c - 0x15);
                    text->glyph.color = string->color;
                } else {
                    Anm_SetSprite(&text->glyph, *c + 0x61);
                    text->glyph.color = 0xffffffffu;
                }
                ANM_DRAW draw = Anm_ScreenDraw();
                draw.view = Anm_ScreenView(
                    g_GameManager.camera.viewport,
                    clipped ? ANM_VIEW_PLAY_AREA : ANM_VIEW_HUD);
                Anm_DrawUnrotated(&text->glyph, &draw);
                text->glyph.pos.x += advance;
            }
        }
    }
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    ASCII_TEXT *const text = arg;
    M_DrawStrings();
    for (int32_t i = 0; i < text->string_count; i++) {
        Mem_Free(text->strings[i].text);
        text->strings[i].text = nullptr;
    }
    text->string_count = 0;
    PauseMenu_DrawPauseMenu(&text->pause_menu);
    PauseMenu_DrawContinueMenu(&text->continue_menu);
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw2(void *const arg)
{
    ASCII_TEXT *const text = arg;
    M_SetViewport(true);
    for (int32_t i = 0; i < ASCII_POPUP_COUNT; i++) {
        const ASCII_POPUP *const popup = &text->popups[i];
        if (!popup->active) {
            continue;
        }
        text->digit.pos.x = popup->pos.x - (float)(popup->digit_count << 2);
        text->digit.pos.y = popup->pos.y;
        text->digit.color = popup->color;
        for (int32_t digit = popup->digit_count - 1; digit >= 0; digit--) {
            Anm_SetSprite(&text->digit, popup->digits[digit]);
            ANM_DRAW draw = GameManager_GetDrawContext();
            draw.interpolate = true;
            draw.part = (uint32_t)digit;
            ANM_VM vm = text->digit;
            vm.presentation_generation = popup->presentation_generation;
            if (popup->digits[digit] >= 10) {
                text->digit.uv_scale_x = 0.1875f;
                text->digit.uv_scale_y = 0.03125f;
                vm.uv_scale_x = text->digit.uv_scale_x;
                vm.uv_scale_y = text->digit.uv_scale_y;
                Anm_Draw3DRounded(&vm, &draw);
                text->digit.uv_scale_x = 0.03125f;
                text->digit.uv_scale_y = 0.03125f;
            } else {
                Anm_Draw3DRounded(&vm, &draw);
            }
            text->digit.pos.x += 8.0f;
        }
    }
    return CHAIN_CONTINUE;
}

static RESULT M_Added(void *const arg)
{
    MUST(Anm_Load(1, "data/ascii.anm", 0));
    MUST(Anm_Load(2, "data/asciis.anm", 0x77));
    MUST(Anm_Load(3, "data/capture.anm", 0x718));
    AsciiText_Initialize();
    return OK;
}

static void M_Deleted(void *const arg)
{
    Anm_Release(1);
    Anm_Release(2);
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
    MUST(Chain_AddDraw(&g_Chain, &m_DrawElem, 15));
    Chain_InitElem(&m_DrawElem2, M_Draw2, &g_AsciiText);
    return Chain_AddDraw(&g_Chain, &m_DrawElem2, 10);
}

void AsciiText_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
}
