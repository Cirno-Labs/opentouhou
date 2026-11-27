#include "th02/boss/scene.h"
#include "th02/stage.h"

#include "th02/player.h"
#include "th02/draw.h"
#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static const PATTERN *M_Pattern(
    const PATTERN_BANK *const patterns, const int16_t id)
{
    ASSERT(id >= 0 && id < PATTERN_LIMIT);
    const PATTERN *const pattern = &patterns->entries[id];
    ASSERT(pattern->data != nullptr);
    return pattern;
}

static int16_t M_Roll(const int16_t top)
{
    const int16_t result = Math16_Wrap((int32_t)top + g_Stage->scroll.line);
    return result >= 400 ? result - 400 : result;
}

void BossScene_DrawPlayer(const PATTERN_BANK *const patterns)
{
    Draw_SpriteRoll(
        M_Pattern(patterns, 0), g_Player->current.x,
        M_Roll(g_Player->current.y));
    const ENTITY_POINT option = g_Player->option[g_Stage->scroll.back];
    Draw_SpriteRollTiny(M_Pattern(patterns, 72), option.x, M_Roll(option.y));
    Draw_SpriteRollTiny(
        M_Pattern(patterns, 72), Math16_Wrap((int32_t)option.x + 48),
        M_Roll(option.y));
}

void BossScene_DrawBorder(const PATTERN_BANK *const patterns)
{
    const int16_t first = M_Roll(320);
    const int16_t second = first + 32 >= 400 ? first + 32 - 400 : first + 32;
    for (int i = 0; i < 12; i++) {
        const int16_t sprite = i < 3 ? 34 + i : i == 11 ? 38 : 37;
        Draw_SpriteRoll(M_Pattern(patterns, sprite), 32 + i * 32, first);
        Draw_SpriteRoll(M_Pattern(patterns, sprite + 8), 32 + i * 32, second);
    }
}

void BossScene_DrawSummon(const PATTERN_BANK *const patterns)
{
    for (int i = 0; i < 64; i++) {
        Draw_Sprite(
            M_Pattern(patterns, 128 + i), 96 + (i % 8) * 32, (i / 8) * 32);
    }
    BossScene_DrawPlayer(patterns);
    BossScene_DrawBorder(patterns);
}

void BossScene_DrawFlash(
    const PATTERN_BANK *const patterns, const int16_t sprite,
    const ENTITY_POINT position, const uint8_t parts, const uint8_t height)
{
    const unsigned shift = (uint16_t)position.x & 7;
    const uint16_t start = (uint16_t)(position.x / 8 - (position.x % 8 < 0)
                                      + (int32_t)position.y * 80);
    Grcg_SetColor(4);
    for (int part = 0; part < parts; part++) {
        const PATTERN *const pattern = M_Pattern(patterns, sprite + part);
        ASSERT(
            pattern->size == (0x0600 | height)
            && pattern->storage_size >= (size_t)height * 30);
        uint16_t offset = (uint16_t)(start + part * 6);
        for (int row = 0; row < height; row++, offset += 80) {
            const int16_t y = Math16_Wrap((int32_t)position.y + row);
            if (y >= 384) {
                break;
            }
            for (int col = 0; col < 6; col++) {
                const int16_t x =
                    Math16_Wrap((int32_t)position.x + part * 48 + col * 8);
                if (x <= 0 || x >= 416 || y < 16) {
                    continue;
                }
                const uint16_t bits = pattern->data[row * 6 + col];
                const uint16_t dots =
                    (uint16_t)((bits >> shift) + (bits << (16 - shift)));
                for (unsigned byte = 0; byte < 2; byte++) {
                    const uint16_t address = (uint16_t)(offset + col + byte);
                    const uint8_t mask = (uint8_t)(dots >> (byte * 8));
                    for (uint8_t plane = 0; plane < 4; plane++) {
                        const uint8_t old = Video_ReadPlaneByte(
                            g_Video.drawing_page, plane, address);
                        Video_WritePlaneByte(
                            plane, address,
                            (old & ~mask) | (plane == 2 ? mask : 0));
                    }
                }
            }
        }
    }
    Grcg_Off();
}
