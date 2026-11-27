#include "th02/point_numbers.h"

#include "th02/background.h"
#include "th02/draw.h"
#include "th02/grcg.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

static POINT_NUMBERS m_PointNumbers;
const POINT_NUMBERS *const g_PointNumbers = &m_PointNumbers;

static void M_Glyph(
    const POINT_NUMBER_FONT *const font, const int16_t left, const int16_t top,
    const uint8_t glyph, const uint8_t color)
{
    ASSERT(glyph < ARRAY_SIZE(font->rows));
    const int32_t column = left / 8 - (left % 8 < 0);
    const uint16_t scaled = (uint16_t)((uint16_t)top << 6);
    uint16_t offset = (uint16_t)(column + scaled + (scaled >> 2));
    const uint32_t shift = (uint16_t)left & 7;
    for (size_t row = 0; row < 8; row++) {
        const uint8_t bits = font->rows[glyph][row];
        Draw_MaskByte(offset, (uint8_t)(bits >> shift), color);
        if (shift != 0) {
            Draw_MaskByte(
                (uint32_t)offset + 1, (uint8_t)((uint32_t)bits << (8 - shift)),
                color);
        }
        offset = (uint16_t)(offset + 80);
        if ((int16_t)offset >= 32000) {
            offset -= 32000;
        }
    }
}

static void M_Number(
    const POINT_NUMBER_FONT *const font, int16_t left, const int16_t top,
    uint16_t value)
{
    static const uint16_t powers[] = { 1000, 100, 10, 1 };
    bool started = false;
    for (size_t digit = 0; digit < ARRAY_SIZE(powers); digit++) {
        const uint8_t glyph = (uint8_t)(value / powers[digit]);
        value %= powers[digit];
        if (glyph != 0 || started) {
            started = true;
            M_Glyph(font, left, top, glyph, m_PointNumbers.color);
        }
        left = Math16_Wrap((int32_t)left + 8);
    }
    M_Glyph(font, left, top, 0, m_PointNumbers.color);
    M_Glyph(
        font, Math16_Wrap((int32_t)left + 8), top, m_PointNumbers.operation,
        m_PointNumbers.color);
    M_Glyph(
        font, Math16_Wrap((int32_t)left + 16), top, m_PointNumbers.operand,
        m_PointNumbers.color);
}

RESULT PointNumberFont_Load(VFS *const files, POINT_NUMBER_FONT *const out)
{
    memset(out, 0, sizeof(*out));
    uint8_t *glyphs;
    size_t glyphs_size;
    MUST(SpriteSheet_Load(
        files, "pointnum.bmp", 8, 8, &g_HeapAllocator, &glyphs, &glyphs_size));
    RESULT result = OK;
    if (glyphs_size != sizeof(out->rows)) {
        result = FAIL("Invalid pointnum.bmp size");
    } else {
        memcpy(out->rows, glyphs, sizeof(out->rows));
    }
    Mem_Free(glyphs);
    return result;
}

void PointNumbers_Init(void)
{
    memset(&m_PointNumbers, 0, sizeof(m_PointNumbers));
}

void PointNumbers_ResetStage(const uint8_t rank)
{
    for (size_t i = 0; i < POINT_NUMBER_COUNT; i++) {
        m_PointNumbers.slots[i].state = ENTITY_FREE;
    }
    m_PointNumbers.color = 15;
    switch (rank) {
    case 0:
    case 1:
        m_PointNumbers.operation = m_PointNumbers.operand = 12;
        break;
    case 2:
    case 3:
    case 4:
        m_PointNumbers.operation = 10;
        m_PointNumbers.operand = (uint8_t)(1u << (rank - 1));
        break;
    }
}

void PointNumbers_Add(
    const int16_t left, const int16_t top, const uint16_t value)
{
    for (size_t i = 0; i < POINT_NUMBER_COUNT; i++) {
        POINT_NUMBER *const number = &m_PointNumbers.slots[i];
        if (number->state == ENTITY_FREE) {
            number->state = ENTITY_ACTIVE;
            number->age = 0;
            number->value = value;
            number->left = left;
            number->top[0] = number->top[1] = top;
            return;
        }
    }
}

void PointNumbers_Invalidate(const PLAYFIELD_PAGES pages)
{
    ASSERT(pages.back < 2 && pages.front < 2);
    for (size_t i = 0; i < POINT_NUMBER_COUNT; i++) {
        POINT_NUMBER *const number = &m_PointNumbers.slots[i];
        if (number->state == ENTITY_FREE) {
            continue;
        }
        Background_Mark(
            pages.scroll, number->left, number->top[pages.back], 64, 8);
        if (number->state == ENTITY_REMOVE) {
            number->state = ENTITY_FREE;
        } else {
            number->top[pages.back] = number->top[pages.front];
        }
    }
}

void PointNumbers_Update(
    const PLAYFIELD_PAGES pages, const POINT_NUMBER_FONT *const font)
{
    ASSERT(pages.back < 2);
    Grcg_SetColor(m_PointNumbers.color);
    for (size_t i = 0; i < POINT_NUMBER_COUNT; i++) {
        POINT_NUMBER *const number = &m_PointNumbers.slots[i];
        if (number->state != ENTITY_ACTIVE) {
            continue;
        }
        if (number->age > 6) {
            number->top[pages.back] =
                Math16_Wrap((int32_t)number->top[pages.back] - 1);
            if (number->age > 24) {
                number->state = ENTITY_REMOVE;
                continue;
            }
        }
        number->age++;
        int16_t top =
            Math16_Wrap((int32_t)number->top[pages.back] + pages.scroll);
        if (top >= 400) {
            top -= 400;
        }
        M_Number(font, number->left, top, number->value);
    }
    Grcg_Off();
}
