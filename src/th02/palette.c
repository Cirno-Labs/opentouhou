#include "th02/palette_private.h"

#include "core/base/assert.h"

#include <string.h>

static PALETTE_STATE m_Palette;
const PALETTE_STATE *const g_Palette = &m_Palette;
PALETTE_STATE *const p_Palette = &m_Palette;

static uint8_t M_Component(
    const uint8_t nibble, const int32_t factor, const uint8_t invert)
{
    return (uint8_t)(((nibble ^ invert) * factor / 100) ^ invert);
}

void Palette_DefaultColors(uint8_t colors[16][3])
{
    for (uint8_t i = 0; i < 16; i++) {
        const uint8_t value = i < 8 ? 255 : 170;
        colors[i][0] = i == 8 ? 119 : (i & 2) != 0 ? value : 0;
        colors[i][1] = i == 8 ? 119 : (i & 4) != 0 ? value : 0;
        colors[i][2] = i == 8 ? 119 : (i & 1) != 0 ? value : 0;
    }
}

void Palette_Init(void)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    Palette_DefaultColors(m_Palette.colors);
    m_Palette.tone = 100;
}

void Palette_Reset(void)
{
    Palette_DefaultColors(m_Palette.colors);
    m_Palette.tone = 100;
    Palette_Show();
}

void Palette_Import(const GFX_COLOR colors[16])
{
    for (size_t i = 0; i < 16; i++) {
        m_Palette.colors[i][0] = colors[i].red;
        m_Palette.colors[i][1] = colors[i].green;
        m_Palette.colors[i][2] = colors[i].blue;
    }
}

RESULT Palette_ReadRgb(READER *const reader, uint8_t colors[16][3])
{
    MUST(Reader_Result(reader));
    const size_t remaining = Reader_Remaining(reader);
    const size_t count = remaining < 48 ? remaining : 48;
    for (size_t i = 0; i < count; i++) {
        const uint8_t channel = Reader_ReadU8(reader);
        if (Reader_Failed(reader)) {
            break;
        }
        colors[i / 3][i % 3] = channel;
    }
    for (size_t i = 0; i < 16; i++) {
        for (size_t channel = 0; channel < 3; channel++) {
            const uint8_t value = colors[i][channel];
            colors[i][channel] = (uint8_t)(value | (value << 4));
        }
    }
    return Reader_Result(reader);
}

RESULT Palette_Recolor(READER *const reader)
{
    return Palette_ReadRgb(reader, m_Palette.colors);
}

void Palette_Show(void)
{
    const int32_t tone = m_Palette.tone < 0 ? 0
        : m_Palette.tone > 200              ? 200
                                            : m_Palette.tone;
    const uint8_t invert = tone > 100 ? 15 : 0;
    const int32_t factor = tone > 100 ? 200 - tone : tone;
    for (uint8_t i = 0; i < 16; i++) {
        Video_SetDac(
            i, M_Component(m_Palette.colors[i][0] >> 4, factor, invert),
            M_Component(m_Palette.colors[i][1] >> 4, factor, invert),
            M_Component(m_Palette.colors[i][2] >> 4, factor, invert));
    }
}

void Palette_SetTone(const int16_t tone)
{
    m_Palette.tone = tone;
    Palette_Show();
}

void Palette_SetToneQuiet(const int16_t tone)
{
    m_Palette.tone = tone;
}

void Palette_SetColor(
    const uint8_t index, const uint8_t r, const uint8_t g, const uint8_t b)
{
    m_Palette.colors[index][0] = r;
    m_Palette.colors[index][1] = g;
    m_Palette.colors[index][2] = b;
}

void Palette_BeginFade(
    PALETTE_FADE *const fade, const PALETTE_FADE_KIND kind, const int16_t speed)
{
    ASSERT(kind >= PALETTE_BLACK_IN && kind <= PALETTE_WHITE_OUT);
    const int16_t starts[] = { 0, 100, 200, 100 };
    const int16_t targets[] = { 100, 0, 100, 200 };
    memset(fade, 0, sizeof(*fade));
    m_Palette.tone = starts[kind];
    fade->speed = speed;
    fade->target = targets[kind];
    fade->delta = fade->target > m_Palette.tone ? 6 : -6;
    fade->wait = 1;
    fade->active = true;
}

bool Palette_FadeVSync(PALETTE_FADE *const fade)
{
    if (!fade->active) {
        return true;
    }
    fade->wait--;
    if (fade->wait != 0) {
        return false;
    }
    do {
        if (fade->shown) {
            const int32_t tone = m_Palette.tone + fade->delta;
            const bool done =
                fade->delta > 0 ? tone >= fade->target : tone <= fade->target;
            m_Palette.tone = done ? fade->target : (int16_t)tone;
            fade->active = !done;
        }
        Palette_Show();
        fade->shown = true;
        if (fade->active && fade->speed > 0) {
            fade->wait = fade->speed;
            return false;
        }
    } while (fade->active);
    return true;
}
