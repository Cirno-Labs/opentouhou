#include "th01/particles.h"

#include "th01/rng.h"
#include "th01/sprite.h"

#include "core/base/assert.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"

#include <string.h>

#define PARTICLES_COUNT 40

typedef struct {
    int16_t spawn_interval;
    int16_t velocity_max;
    int16_t x[PARTICLES_COUNT];
    int16_t y[PARTICLES_COUNT];
    int16_t velocity_x[PARTICLES_COUNT];
    int16_t velocity_y[PARTICLES_COUNT];
    uint8_t alive[PARTICLES_COUNT];
    uint8_t velocity_base[PARTICLES_COUNT];
    uint8_t cycle;
} PARTICLES;

static PARTICLES m_Particles;

static void M_SetX(const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        m_Particles.y[0] = value;
    } else {
        m_Particles.x[i] = value;
    }
}

static void M_SetY(const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        m_Particles.velocity_x[0] = value;
    } else {
        m_Particles.y[i] = value;
    }
}

static void M_SetVelocityX(const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        m_Particles.velocity_y[0] = value;
    } else {
        m_Particles.velocity_x[i] = value;
    }
}

static void M_SetVelocityY(const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        m_Particles.alive[0] = (uint8_t)value;
        m_Particles.alive[1] = (uint8_t)((uint16_t)value >> 8);
    } else {
        m_Particles.velocity_y[i] = value;
    }
}

static void M_Spawn(const PARTICLES_ORIGIN origin, const uint32_t i)
{
    uint8_t *const alive = i == PARTICLES_COUNT ? &m_Particles.velocity_base[0]
                                                : &m_Particles.alive[i];
    if (*alive != 0) {
        return;
    }
    *alive = 1;
    const uint8_t speed =
        (uint8_t)(MasterRng_Next(&g_Rng) % m_Particles.velocity_max + 1);
    if (i == PARTICLES_COUNT) {
        m_Particles.cycle = speed;
    } else {
        m_Particles.velocity_base[i] = speed;
    }
    if (i % 2 == 0) {
        if (origin == PARTICLES_RIGHT || origin == PARTICLES_LEFT) {
            M_SetX(i, origin == PARTICLES_RIGHT ? 639 * 16 : 0);
            M_SetY(i, (int16_t)(MasterRng_Next(&g_Rng) % 400 * 16));
        } else {
            M_SetX(i, (int16_t)(MasterRng_Next(&g_Rng) % 640 * 16));
            M_SetY(
                i,
                origin >= PARTICLES_BOTTOM_RIGHT
                        && origin <= PARTICLES_BOTTOM_LEFT
                    ? 399 * 16
                    : 0);
        }
    } else if (origin == PARTICLES_TOP || origin == PARTICLES_BOTTOM) {
        M_SetX(i, (int16_t)(MasterRng_Next(&g_Rng) % 640 * 16));
        M_SetY(i, origin == PARTICLES_TOP ? 0 : 399 * 16);
    } else {
        M_SetX(
            i,
            origin >= PARTICLES_TOP_RIGHT && origin <= PARTICLES_BOTTOM_RIGHT
                ? 639 * 16
                : 0);
        M_SetY(i, (int16_t)(MasterRng_Next(&g_Rng) % 400 * 16));
    }
    switch (origin) {
    case PARTICLES_TOP:
        M_SetVelocityY(i, (int16_t)(speed * 16));
        M_SetVelocityX(i, 0);
        break;
    case PARTICLES_TOP_RIGHT:
        M_SetVelocityX(i, (int16_t)(-speed * 13));
        M_SetVelocityY(i, (int16_t)(speed * 8));
        break;
    case PARTICLES_RIGHT:
        M_SetVelocityX(i, (int16_t)(-speed * 16));
        M_SetVelocityY(i, 0);
        break;
    case PARTICLES_BOTTOM_RIGHT:
        M_SetVelocityX(i, (int16_t)(-speed * 13));
        M_SetVelocityY(i, (int16_t)(-speed * 8));
        break;
    case PARTICLES_BOTTOM:
        M_SetVelocityY(i, (int16_t)(-speed * 16));
        M_SetVelocityX(i, 0);
        break;
    case PARTICLES_BOTTOM_LEFT:
        M_SetVelocityX(i, (int16_t)(speed * 13));
        M_SetVelocityY(i, (int16_t)(-speed * 8));
        break;
    case PARTICLES_LEFT:
        M_SetVelocityX(i, (int16_t)(speed * 16));
        M_SetVelocityY(i, 0);
        break;
    case PARTICLES_TOP_LEFT:
        M_SetVelocityX(i, (int16_t)(speed * 13));
        M_SetVelocityY(i, (int16_t)(speed * 8));
        break;
    }
}

void Particles_Init(void)
{
    memset(&m_Particles, 0, sizeof(m_Particles));
}

void Particles_ResetStage(void)
{
    memset(m_Particles.alive, 0, sizeof(m_Particles.alive));
    m_Particles.spawn_interval = 2;
    m_Particles.velocity_max = 10;
}

void Particles_Update(const PARTICLES_ORIGIN origin, const uint8_t color)
{
    ASSERT(m_Particles.spawn_interval == 2 && m_Particles.velocity_max == 10);
    ASSERT(origin >= PARTICLES_TOP && origin <= PARTICLES_TOP_LEFT);
    m_Particles.cycle++;
    if (m_Particles.cycle > m_Particles.spawn_interval * PARTICLES_COUNT) {
        m_Particles.cycle = 0;
    }
    if (m_Particles.cycle % m_Particles.spawn_interval == 0) {
        M_Spawn(origin, m_Particles.cycle / m_Particles.spawn_interval);
    }
    for (uint32_t i = 0; i < PARTICLES_COUNT; i++) {
        if (m_Particles.alive[i] == 0) {
            continue;
        }
        Sprite_Restore16(
            Math16_ToPixel(m_Particles.x[i]), Math16_ToPixel(m_Particles.y[i]),
            8, 1);
        m_Particles.x[i] =
            Math16_Wrap(m_Particles.x[i] + m_Particles.velocity_x[i]);
        m_Particles.y[i] =
            Math16_Wrap(m_Particles.y[i] + m_Particles.velocity_y[i]);
        if (m_Particles.x[i] < 0 || m_Particles.x[i] > 639 * 16
            || m_Particles.y[i] < 0 || m_Particles.y[i] > 399 * 16) {
            m_Particles.alive[i] = 0;
        }
    }
    for (uint32_t i = 0; i < PARTICLES_COUNT; i++) {
        if (m_Particles.alive[i] != 0) {
            Video_Pixel(
                Math16_ToPixel(m_Particles.x[i]),
                Math16_ToPixel(m_Particles.y[i]), color);
        }
    }
}
