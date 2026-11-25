#include "th01/missiles.h"
#include "th01/sound.h"

#include "pc98/zun/math16.h"

#include "th01/finite.h"

#include <string.h>

#define MISSILES_COUNT 50

typedef enum {
    MISSILE_FREE,
    MISSILE_MOVING,
    MISSILE_HIT,
    MISSILE_HIT_LAST,
} MISSILE_FLAG;

typedef struct {
    int16_t left, top, previous_left, previous_top;
    MATH16_VECTOR velocity;
    int16_t unused;
    int8_t reserved;
    uint8_t flag;
} MISSILE;

typedef struct {
    MISSILE slots[MISSILES_COUNT];
} MISSILES;

static MISSILES m_Missiles;

static void M_Put(
    const PTN *const sprites, int16_t left, int16_t top, int16_t image,
    int16_t quarter);

static void M_Restore(const MISSILE *const missile)
{
    Sprite_Restore16(
        Math16_ToPixel(missile->previous_left),
        Math16_ToPixel(missile->previous_top), 32, 16);
}

void Missiles_Init(void)
{
    memset(&m_Missiles, 0, sizeof(m_Missiles));
}

RESULT Missiles_ValidateSprites(const PTN *const sprites)
{
    FAIL_IF(
        sprites->count < 5 || sprites->pixels == nullptr,
        "Incomplete missile PTN sprites");
    return OK;
}

bool Missiles_Add(
    const int16_t left, const int16_t top, const double velocity_x,
    const double velocity_y, const int8_t reserved)
{
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        MISSILE *const m = &m_Missiles.slots[i];
        if (m->flag != MISSILE_FREE) {
            continue;
        }
        m->left = Math16_FromPixel(left);
        m->top = Math16_FromPixel(top);
        m->previous_left = -1;
        m->velocity = (MATH16_VECTOR) {
            Finite_ToPixel(Finite_Multiply(
                Finite_FromDouble(velocity_x), Finite_FromDouble(16))),
            Finite_ToPixel(Finite_Multiply(
                Finite_FromDouble(velocity_y), Finite_FromDouble(16))),
        };
        m->reserved = reserved;
        m->flag = MISSILE_MOVING;
        return true;
    }
    return false;
}

void Missiles_Reset(void)
{
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        m_Missiles.slots[i].flag = MISSILE_FREE;
    }
}

void Missiles_UpdateAndRender(
    const uint16_t frame_rand, const PTN *const sprites)
{
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        MISSILE *const m = &m_Missiles.slots[i];
        if (m->flag == MISSILE_FREE) {
            continue;
        }
        const bool field = (i & 1) == (frame_rand & 1);
        if (field && m->previous_left != -1) {
            M_Restore(m);
        }
        if (m->flag == MISSILE_MOVING) {
            m->left = Math16_Wrap(m->left + m->velocity.x);
            m->top = Math16_Wrap(m->top + m->velocity.y);
            const int16_t left = Math16_ToPixel(m->left);
            const int16_t top = Math16_ToPixel(m->top);
            if (left < 0 || left > 624 || top < 64 || top > 399) {
                m->flag = MISSILE_HIT;
                M_Restore(m);
                if (left < 0 || left > 624) {
                    m->previous_left = Math16_FromPixel(left < 0 ? 0 : 624);
                }
                if (top < 64 || top > 399) {
                    m->previous_top = Math16_FromPixel(top < 64 ? 64 : 392);
                }
            }
        } else {
            m->flag += field;
            if (m->flag > MISSILE_HIT_LAST) {
                m->flag = MISSILE_FREE;
                M_Restore(m);
                Sound_Effect(7);
            }
        }
    }
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        MISSILE *const m = &m_Missiles.slots[i];
        if (m->flag == MISSILE_FREE || (i & 1) != (frame_rand & 1)) {
            continue;
        }
        if (m->flag == MISSILE_MOVING) {
            const uint8_t angle = Math16_Angle(m->velocity.y, m->velocity.x);
            M_Put(
                sprites, Math16_ToPixel(m->left), Math16_ToPixel(m->top),
                angle / 64, (angle % 64) / 16);
            m->previous_left = m->left;
            m->previous_top = m->top;
        } else {
            M_Put(
                sprites, Math16_ToPixel(m->previous_left),
                Math16_ToPixel(m->previous_top), 4,
                (int16_t)(m->flag - MISSILE_HIT));
        }
    }
    if (g_Player.invincible) {
        return;
    }
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        const MISSILE *const m = &m_Missiles.slots[i];
        if (m->flag != MISSILE_FREE
            && Math16_ToPixel(m->left) > g_Player.left - 8
            && Math16_ToPixel(m->left) < g_Player.left + 24
            && Math16_ToPixel(m->top) > 360 && Math16_ToPixel(m->top) < 400) {
            g_Player.is_hit = true;
            return;
        }
    }
}

static uint8_t M_ShiftByte(
    const uint16_t dots, const int32_t first_bit, const int32_t byte)
{
    const uint32_t low = dots & 255u;
    const uint32_t high = dots & 0xff00u;
    if (byte == 0) {
        return (uint8_t)(low >> ((uint32_t)first_bit & 31u));
    }
    if (byte == 1) {
        return (uint8_t)((low << ((uint32_t)(8 - first_bit) & 31u))
                         | (high >> ((uint32_t)(8 + first_bit) & 31u)));
    }
    return byte == 2 ? (uint8_t)(high >> ((uint32_t)first_bit & 31u)) : 0;
}

static void M_RawOr(
    const uint32_t offset, const int32_t plane, const uint8_t dots)
{
    static const uint32_t bases[4] = { 0xa8000, 0xb0000, 0xb8000, 0xe0000 };
    const uint32_t address = bases[plane] + offset;
    for (int32_t target = 0; target < 4; target++) {
        if (address >= bases[target] && address < bases[target] + 32000) {
            const uint32_t pixel = (address - bases[target]) * 8;
            for (int32_t bit = 0; bit < 8; bit++) {
                if ((dots & (0x80u >> bit)) != 0) {
                    g_Video.pages[g_Video.drawing_page][pixel + bit] |=
                        (uint8_t)(1u << target);
                }
            }
            return;
        }
    }
}

static void M_Put(
    const PTN *const sprites, const int16_t left, const int16_t top,
    const int16_t image, const int16_t quarter)
{
    const int32_t sx = (quarter & 1) * 16;
    const int32_t sy = (quarter & 2) * 8;
    const uint8_t *const pixels = sprites->pixels + image * 1024;
    const int32_t first_bit = left % 8;
    const int32_t column = left / 8 - (left % 8 < 0);
    uint16_t offset = (uint16_t)(top * 80 + column);
    for (int32_t y = 0; y < 16; y++) {
        uint16_t planes[4] = {};
        uint16_t mask = 0;
        for (int32_t x = 0; x < 16; x++) {
            const uint8_t color = pixels[(sy + y) * 32 + sx + x];
            const uint16_t dot =
                (uint16_t)((0x80u >> (x & 7)) << ((x / 8) * 8));
            if (color != PTN_TRANSPARENT) {
                mask |= dot;
            }
            for (int32_t plane = 0; plane < 4; plane++) {
                if ((color & (1u << plane)) != 0) {
                    planes[plane] |= dot;
                }
            }
        }
        if (mask != 0) {
            const int32_t bytes = first_bit == 0 ? 2 : 4;
            for (int32_t byte = 0; byte < bytes; byte++) {
                const uint8_t shifted_mask = first_bit == 0
                    ? (uint8_t)(mask >> (byte * 8))
                    : M_ShiftByte(mask, first_bit, byte);
                const uint32_t clear_offset =
                    ((uint32_t)offset + byte) & 0x7fffu;
                if (clear_offset < 32000) {
                    for (int32_t bit = 0; bit < 8; bit++) {
                        if ((shifted_mask & (0x80u >> bit)) != 0) {
                            g_Video.pages[g_Video.drawing_page]
                                         [clear_offset * 8 + bit] = 0;
                        }
                    }
                }
                for (int32_t plane = 0; plane < 4; plane++) {
                    const uint8_t dots = first_bit == 0
                        ? (uint8_t)(planes[plane] >> (byte * 8))
                        : M_ShiftByte(planes[plane], first_bit, byte);
                    M_RawOr(
                        (uint32_t)offset + byte, plane, dots & shifted_mask);
                }
            }
        }
        offset = (uint16_t)(offset + 80);
        if (offset > 32000) {
            break;
        }
    }
}
