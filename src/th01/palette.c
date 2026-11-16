#include "th01/palette.h"

#include "core/base/assert.h"

static const PALETTE m_Stock = { .colors = {
                                     { 0, 0, 0 },
                                     { 0, 0, 15 },
                                     { 0, 15, 0 },
                                     { 0, 15, 15 },
                                     { 15, 0, 0 },
                                     { 15, 0, 15 },
                                     { 15, 15, 0 },
                                     { 15, 15, 15 },
                                     { 8, 8, 8 },
                                     { 0, 0, 10 },
                                     { 0, 10, 0 },
                                     { 0, 10, 10 },
                                     { 10, 0, 0 },
                                     { 10, 0, 10 },
                                     { 10, 10, 0 },
                                     { 12, 12, 12 },
                                 } };

static int32_t M_Clamp(const int32_t value, const int32_t maximum)
{
    return value < 0 ? 0 : value > maximum ? maximum : value;
}

static void M_Show(const PALETTE *const colors, VIDEO *const video)
{
    for (int32_t i = 0; i < 16; i++) {
        Video_SetDac(
            video, i, colors->colors[i][0], colors->colors[i][1],
            colors->colors[i][2]);
    }
}

void Palette_Init(PALETTE_STATE *const state)
{
    *state = (PALETTE_STATE) { .saved = m_Stock, .tone = 100 };
}

void Palette_Import(PALETTE *const out, const GFX_COLOR colors[16])
{
    for (int32_t i = 0; i < 16; i++) {
        out->colors[i][0] = (int8_t)(colors[i].red >> 4);
        out->colors[i][1] = (int8_t)(colors[i].green >> 4);
        out->colors[i][2] = (int8_t)(colors[i].blue >> 4);
    }
}

void Palette_SetColor(
    PALETTE_STATE *const state, VIDEO *const video, const int32_t index,
    const int32_t red, const int32_t green, const int32_t blue)
{
    ASSERT(index >= 0 && index < 16);
    state->saved.colors[index][0] = (int8_t)M_Clamp(red, 15);
    state->saved.colors[index][1] = (int8_t)M_Clamp(green, 15);
    state->saved.colors[index][2] = (int8_t)M_Clamp(blue, 15);
    Video_SetDac(
        video, index, state->saved.colors[index][0],
        state->saved.colors[index][1], state->saved.colors[index][2]);
}

void Palette_Set(
    PALETTE_STATE *const state, VIDEO *const video, const PALETTE *const colors)
{
    for (int32_t i = 0; i < 16; i++) {
        Palette_SetColor(
            state, video, i, colors->colors[i][0], colors->colors[i][1],
            colors->colors[i][2]);
    }
}

void Palette_SetGray(
    PALETTE_STATE *const state, VIDEO *const video, const int32_t value)
{
    for (int32_t i = 0; i < 16; i++) {
        Palette_SetColor(state, video, i, value, value, value);
    }
}

void Palette_Show(const PALETTE_STATE *const state, VIDEO *const video)
{
    M_Show(&state->saved, video);
}

void Palette_ShowSolid(
    VIDEO *const video, const int32_t red, const int32_t green,
    const int32_t blue)
{
    for (int32_t i = 0; i < 16; i++) {
        Video_SetDac(video, i, red, green, blue);
    }
}

void Palette_StepTo(
    PALETTE_STATE *const state, VIDEO *const video, const PALETTE *const target,
    const bool only_brighten)
{
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            int32_t value = state->saved.colors[i][c];
            if (value < target->colors[i][c]) {
                value++;
            } else if (!only_brighten && value > target->colors[i][c]) {
                value--;
            }
            state->saved.colors[i][c] = (int8_t)value;
        }
    }
    Palette_Set(state, video, &state->saved);
}

void Palette_SetTone(
    PALETTE_STATE *const state, VIDEO *const video, int32_t tone)
{
    tone = M_Clamp(tone, 200);
    for (int32_t i = 1; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            const int32_t value = state->resource.colors[i][c];
            const int32_t result = tone > 100
                ? value + (15 - value) * (tone - 100) / 100
                : value - value * (100 - tone) / 100;
            state->saved.colors[i][c] = (int8_t)result;
        }
    }
    state->tone = tone;
    Palette_Set(state, video, &state->saved);
}

void Palette_ShowResource(
    PALETTE_STATE *const state, VIDEO *const video, const bool use_tone)
{
    if (use_tone) {
        Palette_SetTone(state, video, state->tone);
    } else {
        Palette_Set(state, video, &state->resource);
    }
}

void Palette_BeginFade(
    PALETTE_FADE *const fade, const PALETTE_STATE *const state,
    const PALETTE_FADE_KIND kind)
{
    ASSERT(kind >= PALETTE_FADE_BLACK_IN && kind <= PALETTE_FADE_WHITE_OUT);
    *fade = (PALETTE_FADE) { .kind = kind, .remaining = 16 };
    if (kind == PALETTE_FADE_BLACK_OUT || kind == PALETTE_FADE_WHITE_OUT) {
        fade->colors = state->saved;
    } else if (kind == PALETTE_FADE_WHITE_IN) {
        for (int32_t i = 0; i < 16; i++) {
            for (int32_t c = 0; c < 3; c++) {
                fade->colors.colors[i][c] = 15;
            }
        }
    }
}

void Palette_BeginFrom(
    PALETTE_FADE *const fade, const PALETTE_STATE *const state,
    const int8_t red, const int8_t green, const int8_t blue,
    const uint16_t keep)
{
    *fade = (PALETTE_FADE) { .kind = PALETTE_FADE_FROM, .remaining = 16 };
    const int8_t color[3] = { red, green, blue };
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            fade->colors.colors[i][c] =
                (keep & (1u << i)) != 0 ? state->saved.colors[i][c] : color[c];
        }
    }
}

void Palette_BeginToneFade(
    PALETTE_FADE *const fade, const PALETTE_FADE_KIND kind)
{
    ASSERT(kind >= PALETTE_FADE_BLACK_IN && kind <= PALETTE_FADE_WHITE_OUT);
    *fade = (PALETTE_FADE) {
        .kind = kind,
        .remaining = 20,
        .tone_fade = true,
        .tone = kind == PALETTE_FADE_BLACK_IN ? 0
            : kind == PALETTE_FADE_WHITE_IN   ? 200
                                              : 100,
    };
}

bool Palette_FadeStep(
    PALETTE_FADE *const fade, PALETTE_STATE *const state, VIDEO *const video)
{
    if (fade->remaining == 0) {
        return false;
    }
    fade->remaining--;
    if (fade->tone_fade) {
        fade->tone += fade->kind == PALETTE_FADE_BLACK_IN
                || fade->kind == PALETTE_FADE_WHITE_OUT
            ? 5
            : -5;
        Palette_SetTone(state, video, fade->tone);
        return true;
    }
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            int32_t value = fade->colors.colors[i][c];
            const int32_t target = state->saved.colors[i][c];
            if (((fade->kind == PALETTE_FADE_BLACK_IN
                  || fade->kind == PALETTE_FADE_FROM)
                 && value < target)
                || (fade->kind == PALETTE_FADE_WHITE_OUT && value < 15)) {
                value++;
            } else if (
                ((fade->kind == PALETTE_FADE_WHITE_IN
                  || fade->kind == PALETTE_FADE_FROM)
                 && value > target)
                || (fade->kind == PALETTE_FADE_BLACK_OUT && value > 0)) {
                value--;
            }
            fade->colors.colors[i][c] = (int8_t)value;
        }
    }
    M_Show(&fade->colors, video);
    return true;
}
