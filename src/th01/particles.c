#include "th01/particles.h"
#include "th01/rng.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"
#include "th01/sprite.h"

#include <string.h>

PARTICLES g_Particles;

static void M_SetX(const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        g_Particles.y[0] = value;
    } else {
        g_Particles.x[i] = value;
    }
}

static void M_SetY(const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        g_Particles.velocity_x[0] = value;
    } else {
        g_Particles.y[i] = value;
    }
}

static void M_SetVelocityX(const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        g_Particles.velocity_y[0] = value;
    } else {
        g_Particles.velocity_x[i] = value;
    }
}

static void M_SetVelocityY(const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        g_Particles.alive[0] = (uint8_t)value;
        g_Particles.alive[1] = (uint8_t)((uint16_t)value >> 8);
    } else {
        g_Particles.velocity_y[i] = value;
    }
}

static void M_Spawn(const PARTICLES_ORIGIN origin, const uint32_t i)
{
    uint8_t *const alive = i == PARTICLES_COUNT ? &g_Particles.velocity_base[0]
                                                : &g_Particles.alive[i];
    if (*alive != 0) {
        return;
    }
    *alive = 1;
    const uint8_t speed =
        (uint8_t)(MasterRng_Next(&g_Rng) % g_Particles.velocity_max + 1);
    if (i == PARTICLES_COUNT) {
        g_Particles.cycle = speed;
    } else {
        g_Particles.velocity_base[i] = speed;
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
    memset(g_Particles.alive, 0, sizeof(g_Particles.alive));
    g_Particles.spawn_interval = 2;
    g_Particles.velocity_max = 10;
}

void Particles_UpdateAndRender(
    const PARTICLES_ORIGIN origin, const uint8_t color)
{
    ASSERT(g_Particles.spawn_interval == 2 && g_Particles.velocity_max == 10);
    ASSERT(origin >= PARTICLES_TOP && origin <= PARTICLES_TOP_LEFT);
    g_Particles.cycle++;
    if (g_Particles.cycle > g_Particles.spawn_interval * PARTICLES_COUNT) {
        g_Particles.cycle = 0;
    }
    if (g_Particles.cycle % g_Particles.spawn_interval == 0) {
        M_Spawn(origin, g_Particles.cycle / g_Particles.spawn_interval);
    }
    for (uint32_t i = 0; i < PARTICLES_COUNT; i++) {
        if (g_Particles.alive[i] == 0) {
            continue;
        }
        Sprite_Restore16(
            Math16_ToPixel(g_Particles.x[i]), Math16_ToPixel(g_Particles.y[i]),
            8, 1);
        g_Particles.x[i] =
            Math16_Wrap(g_Particles.x[i] + g_Particles.velocity_x[i]);
        g_Particles.y[i] =
            Math16_Wrap(g_Particles.y[i] + g_Particles.velocity_y[i]);
        if (g_Particles.x[i] < 0 || g_Particles.x[i] > 639 * 16
            || g_Particles.y[i] < 0 || g_Particles.y[i] > 399 * 16) {
            g_Particles.alive[i] = 0;
        }
    }
    for (uint32_t i = 0; i < PARTICLES_COUNT; i++) {
        if (g_Particles.alive[i] != 0) {
            Video_Pixel(
                Math16_ToPixel(g_Particles.x[i]),
                Math16_ToPixel(g_Particles.y[i]), color);
        }
    }
}
