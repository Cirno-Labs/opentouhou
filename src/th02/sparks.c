#include "th02/background.h"
#include "th02/clip.h"
#include "th02/draw.h"
#include "th02/grcg.h"
#include "th02/random.h"
#include "th02/sparks_private.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

static SPARKS m_Sparks;
const SPARKS *const g_Sparks = &m_Sparks;
SPARKS *const p_Sparks = &m_Sparks;

static void M_Put(
    const SPARK_IMAGES *const images, const int16_t left, const uint16_t top,
    const uint8_t image)
{
    const uint16_t scaled = (uint16_t)(top << 6);
    uint16_t offset = (uint16_t)(left / 8 + scaled + (scaled >> 2));
    const uint32_t shift = (uint16_t)left & 7;
    for (size_t row = 0; row < 8; row++) {
        const uint16_t bits = images->rows[shift][image][row];
        Draw_MaskByte(offset, (uint8_t)bits, 14);
        Draw_MaskByte((uint32_t)offset + 1, (uint8_t)(bits >> 8), 14);
        offset = (uint16_t)(offset + 80);
        if ((int16_t)offset >= 32000) {
            offset -= 32000;
        }
    }
}

void Sparks_Init(void)
{
    memset(&m_Sparks, 0, sizeof(m_Sparks));
}

void Sparks_ResetStage(const bool reduce_effects)
{
    m_Sparks.sprite_interval = reduce_effects ? 7 : 1;
    m_Sparks.age_limit = reduce_effects ? 24 : 48;
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        SPARK *const spark = &m_Sparks.slots[i];
        spark->state = ENTITY_FREE;
        spark->angle = (uint8_t)Random_U15();
        spark->base_speed = Random_U15() & 63;
        spark->preferred = (i & m_Sparks.sprite_interval) == 0 ? 14 : 0;
    }
}

void Sparks_SetDrift(const int16_t acceleration_x)
{
    m_Sparks.acceleration_x = acceleration_x;
}

void Sparks_Add(
    const uint8_t back, const int16_t left, uint16_t top, const int16_t speed,
    const int16_t count, const bool as_sprite)
{
    ASSERT(back < 2 && m_Sparks.next < SPARK_COUNT);
    if (top >= 400) {
        top -= 400;
    }
    const ENTITY_POINT position = { Math16_FromPixel(left),
                                    Math16_FromPixel((int16_t)top) };
    uint16_t slot = m_Sparks.next;
    for (int32_t i = 0; i < count; i++) {
        SPARK *const spark = &m_Sparks.slots[slot];
        spark->state = ENTITY_ACTIVE;
        spark->age = 0;
        spark->position[back] = position;
        spark->drawing = as_sprite ? 14 : spark->preferred;
        const int16_t length =
            as_sprite ? speed : Math16_Wrap((int32_t)speed + spark->base_speed);
        const int32_t angle =
            as_sprite ? Math16_Wrap(i * 256) / count : spark->angle;
        const MATH16_VECTOR velocity = Math16_Vector(length, angle);
        spark->velocity = (ENTITY_POINT) { velocity.x, velocity.y };
        slot = (slot + 1) % SPARK_COUNT;
    }
    m_Sparks.next = (uint8_t)(m_Sparks.next + count) & (SPARK_COUNT - 1);
}

static RESULT M_Fill(
    SPARK_IMAGES *const out, const uint8_t *const sparks,
    const size_t sparks_size)
{
    FAIL_IF(sparks_size != 8 * 8, "Invalid sparks.bmp size");
    for (size_t shift = 0; shift < 8; shift++) {
        for (size_t image = 0; image < 8; image++) {
            for (size_t row = 0; row < 8; row++) {
                out->rows[shift][image][row] =
                    (uint16_t)((sparks[image * 8 + row] >> shift)
                               | ((unsigned)sparks[image * 8 + row]
                                  << (16 - shift)));
            }
        }
    }
    return OK;
}

RESULT SparkImages_Load(VFS *const files, SPARK_IMAGES *const out)
{
    memset(out, 0, sizeof(*out));
    uint8_t *sparks;
    size_t sparks_size;
    MUST(SpriteSheet_Load(
        files, "sparks.bmp", 8, 8, &g_HeapAllocator, &sparks, &sparks_size));
    const RESULT result = M_Fill(out, sparks, sparks_size);
    Mem_Free(sparks);
    return result;
}

void Sparks_Invalidate(const PLAYFIELD_PAGES pages)
{
    ASSERT(pages.back < 2 && pages.front < 2);
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        SPARK *const spark = &m_Sparks.slots[i];
        if (spark->state == ENTITY_FREE) {
            continue;
        }
        const ENTITY_POINT position = spark->position[pages.back];
        const int16_t size = spark->drawing == 0 ? 1 : 8;
        Background_Mark(
            pages.scroll, Math16_ToPixel(position.x),
            Math16_ToPixel(position.y), size, size);
        if (spark->state == ENTITY_REMOVE) {
            spark->state = ENTITY_FREE;
        } else {
            spark->position[pages.front ^ 1] = spark->position[pages.front];
        }
    }
}

void Sparks_Update(
    const PLAYFIELD_PAGES pages, const SPARK_IMAGES *const images)
{
    ASSERT(pages.back < 2);
    Grcg_SetColor(14);
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        SPARK *const spark = &m_Sparks.slots[i];
        if (spark->state != ENTITY_ACTIVE) {
            continue;
        }
        ENTITY_POINT *const position = &spark->position[pages.back];
        position->x = Math16_Wrap((int32_t)position->x + spark->velocity.x);
        spark->velocity.x =
            Math16_Wrap((int32_t)spark->velocity.x + m_Sparks.acceleration_x);
        position->y = Math16_Wrap((int32_t)position->y + spark->velocity.y);
        const int16_t left = Math16_ToPixel(position->x);
        const int16_t top = Math16_ToPixel(position->y);
        spark->velocity.y = Math16_Wrap((int32_t)spark->velocity.y + 1);
        spark->age++;
        if (left < 32 || left >= 408 || top <= 8 || top >= 384
            || spark->age > m_Sparks.age_limit) {
            spark->state = ENTITY_REMOVE;
            continue;
        }
        uint16_t rolled = (uint16_t)((int32_t)top + pages.scroll);
        if (rolled >= 400) {
            rolled -= 400;
        }
        if (spark->drawing == 0) {
            const int16_t y = Math16_Wrap(rolled);
            Draw_BoxFill(g_Clip, left, y, left, y);
        } else {
            M_Put(images, left, rolled, spark->age & 7);
        }
    }
    Grcg_Off();
}
