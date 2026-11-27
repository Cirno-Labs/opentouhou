#include "th02/bullets/private.h"
#include "th02/bomb.h"
#include "th02/performance.h"
#include "th02/random.h"

#include "pc98/zun/math16.h"

#include <string.h>

typedef struct {
    int8_t rank;
    int8_t speed_bias;
    uint8_t base_layers;
    uint8_t layers;
    uint8_t slow_cycle;
} BULLET_SPAWN;

static BULLET_SPAWN m_Spawn;

static int M_Fan(const int count, const int width, const int16_t index)
{
    int offset = width * (count == 2 ? 3 : count == 3 ? 8 : 6);
    if (count == 2) {
        if (index == 0) {
            offset += 3;
        } else if (index == 1) {
            offset = -3 - offset;
        }
    } else if (count == 3) {
        if (index == 0) {
            offset = 0;
        } else if (index == 1) {
            offset += 8;
        } else if (index == 2) {
            offset = -8 - offset;
        }
    } else if (count == 4) {
        if (index == 0) {
            offset += 3;
        } else if (index == 1) {
            offset = -3 - offset;
        } else if (index == 2) {
            offset = 9 + offset * 3;
        } else if (index == 3) {
            offset = -9 - offset * 3;
        }
    } else {
        if (index == 0) {
            offset = 0;
        } else if (index == 1) {
            offset += 6;
        } else if (index == 2) {
            offset = -6 - offset;
        } else if (index == 3) {
            offset = 12 + offset * 2;
        } else if (index == 4) {
            offset = -12 - offset * 2;
        }
    }
    return offset;
}

void BulletSpawn_Clear(void)
{
    memset(&m_Spawn, 0, sizeof(m_Spawn));
}

void BulletSpawn_Init(const int8_t rank)
{
    m_Spawn.rank = rank;
    m_Spawn.speed_bias = rank == 0 ? -10 : rank == 3 ? 12 : 0;
    m_Spawn.base_layers = rank == 2 || rank == 3 ? 1 : 0;
    m_Spawn.layers = m_Spawn.base_layers;
}

bool BulletSpawn_Outside(const int16_t left, const int16_t top)
{
    return left < 24 || left > 416 || top < 0 || top > 383;
}

bool BulletSpawn_Tune(int16_t *const speed)
{
    const int16_t product = Math16_Wrap((int32_t)*speed * g_Performance->value);
    *speed = Math16_Wrap((int32_t)*speed + product / 48 + m_Spawn.speed_bias);
    if (*speed >= 16) {
        return false;
    }
    m_Spawn.slow_cycle++;
    if (m_Spawn.rank == 0 && (m_Spawn.slow_cycle & 1) != 0) {
        return true;
    }
    *speed = 16;
    return false;
}

bool BulletSpawn_Skip(
    const int16_t left, const int16_t top, const bool gravity,
    int16_t *const speed)
{
    return BulletSpawn_Outside(left, top) || g_Bomb->active != 0
        || (!gravity && BulletSpawn_Tune(speed));
}

void BulletSpawn_Layers(const uint8_t multiplier)
{
    m_Spawn.layers = (uint8_t)(m_Spawn.base_layers * multiplier);
}

bool BulletSpawn_NextLayer(
    int16_t *const layer, int16_t *const index, int16_t *const speed)
{
    if (*layer >= m_Spawn.layers) {
        return false;
    }
    *speed = *speed / 2 - (*speed < 0 && *speed % 2 != 0);
    if (m_Spawn.rank == 3) {
        *speed += 8;
    }
    if (*speed < 16) {
        return false;
    }
    (*layer)++;
    *index = 0;
    return true;
}

BULLET_DIRECTION BulletSpawn_Direction(
    const int16_t pattern, const int16_t index, const uint8_t angle,
    const int16_t speed)
{
    BULLET_DIRECTION result = { .speed = speed,
                                .next_index = Math16_Wrap((int32_t)index + 1),
                                .angle = angle,
                                .aimed = true };
    int offset = 0;
    if (pattern >= BULLET_FAN2 && pattern <= BULLET_FAN5_WIDE_AIMED) {
        const int code = pattern - 1;
        const int count = code / 6 + 2;
        offset = M_Fan(count, (code % 6) % 3, index);
        result.last = index == count - 1;
        result.aimed = code % 6 >= 3;
    } else {
        switch (pattern) {
        case BULLET_FAN2_EXTRA_WIDE_AIMED:
            offset = index == 0 ? 14 : index == 1 ? -14 : 11;
            result.last = index == 1;
            break;
        case BULLET_AIMED:
            result.last = true;
            break;
        case BULLET_SINGLE:
            result.aimed = false;
            result.last = true;
            break;
        case BULLET_RING2:
            offset = index == 0 ? 0 : 128;
            result.aimed = false;
            result.last = index != 0;
            break;
        case BULLET_RING4:
        case BULLET_RING8:
        case BULLET_RING16:
        case BULLET_RING32: {
            const int count = pattern == BULLET_RING4 ? 4
                : pattern == BULLET_RING8             ? 8
                : pattern == BULLET_RING16            ? 16
                                                      : 32;
            offset = index * (256 / count);
            result.aimed = false;
            result.last = index >= count - 1;
            break;
        }
        case BULLET_MIRROR:
            offset = index == 0 ? 0 : 128 - angle * 2;
            result.aimed = false;
            result.last = index != 0;
            break;
        case BULLET_RANDOM_ONE:
        case BULLET_RANDOM:
        case BULLET_RANDOM_SPEED:
            offset = Random_RingU8();
            result.aimed = false;
            result.last = pattern == BULLET_RANDOM_ONE || index >= angle;
            if (pattern == BULLET_RANDOM_SPEED) {
                result.speed =
                    Math16_Wrap((int32_t)speed + Random_RingU8And(31));
            }
            break;
        default:
            break;
        }
    }
    result.angle = (uint8_t)(angle + offset);
    return result;
}
