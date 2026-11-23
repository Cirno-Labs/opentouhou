#include "th01/particles.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"
#include "th01/sprite.h"

#include <string.h>

static void M_SetX(PARTICLES *const p, const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        p->y[0] = value;
    } else {
        p->x[i] = value;
    }
}

static void M_SetY(PARTICLES *const p, const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        p->velocity_x[0] = value;
    } else {
        p->y[i] = value;
    }
}

static void M_SetVelocityX(
    PARTICLES *const p, const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        p->velocity_y[0] = value;
    } else {
        p->velocity_x[i] = value;
    }
}

static void M_SetVelocityY(
    PARTICLES *const p, const uint32_t i, const int16_t value)
{
    if (i == PARTICLES_COUNT) {
        p->alive[0] = (uint8_t)value;
        p->alive[1] = (uint8_t)((uint16_t)value >> 8);
    } else {
        p->velocity_y[i] = value;
    }
}

static void M_Spawn(
    PARTICLES *const p, uint32_t *const rng, const PARTICLES_ORIGIN origin,
    const uint32_t i)
{
    uint8_t *const alive =
        i == PARTICLES_COUNT ? &p->velocity_base[0] : &p->alive[i];
    if (*alive != 0) {
        return;
    }
    *alive = 1;
    const uint8_t speed = (uint8_t)(MasterRng_Next(rng) % p->velocity_max + 1);
    if (i == PARTICLES_COUNT) {
        p->cycle = speed;
    } else {
        p->velocity_base[i] = speed;
    }
    if (i % 2 == 0) {
        if (origin == PARTICLES_RIGHT || origin == PARTICLES_LEFT) {
            M_SetX(p, i, origin == PARTICLES_RIGHT ? 639 * 16 : 0);
            M_SetY(p, i, (int16_t)(MasterRng_Next(rng) % 400 * 16));
        } else {
            M_SetX(p, i, (int16_t)(MasterRng_Next(rng) % 640 * 16));
            M_SetY(
                p, i,
                origin >= PARTICLES_BOTTOM_RIGHT
                        && origin <= PARTICLES_BOTTOM_LEFT
                    ? 399 * 16
                    : 0);
        }
    } else if (origin == PARTICLES_TOP || origin == PARTICLES_BOTTOM) {
        M_SetX(p, i, (int16_t)(MasterRng_Next(rng) % 640 * 16));
        M_SetY(p, i, origin == PARTICLES_TOP ? 0 : 399 * 16);
    } else {
        M_SetX(
            p, i,
            origin >= PARTICLES_TOP_RIGHT && origin <= PARTICLES_BOTTOM_RIGHT
                ? 639 * 16
                : 0);
        M_SetY(p, i, (int16_t)(MasterRng_Next(rng) % 400 * 16));
    }
    switch (origin) {
    case PARTICLES_TOP:
        M_SetVelocityY(p, i, (int16_t)(speed * 16));
        M_SetVelocityX(p, i, 0);
        break;
    case PARTICLES_TOP_RIGHT:
        M_SetVelocityX(p, i, (int16_t)(-speed * 13));
        M_SetVelocityY(p, i, (int16_t)(speed * 8));
        break;
    case PARTICLES_RIGHT:
        M_SetVelocityX(p, i, (int16_t)(-speed * 16));
        M_SetVelocityY(p, i, 0);
        break;
    case PARTICLES_BOTTOM_RIGHT:
        M_SetVelocityX(p, i, (int16_t)(-speed * 13));
        M_SetVelocityY(p, i, (int16_t)(-speed * 8));
        break;
    case PARTICLES_BOTTOM:
        M_SetVelocityY(p, i, (int16_t)(-speed * 16));
        M_SetVelocityX(p, i, 0);
        break;
    case PARTICLES_BOTTOM_LEFT:
        M_SetVelocityX(p, i, (int16_t)(speed * 13));
        M_SetVelocityY(p, i, (int16_t)(-speed * 8));
        break;
    case PARTICLES_LEFT:
        M_SetVelocityX(p, i, (int16_t)(speed * 16));
        M_SetVelocityY(p, i, 0);
        break;
    case PARTICLES_TOP_LEFT:
        M_SetVelocityX(p, i, (int16_t)(speed * 13));
        M_SetVelocityY(p, i, (int16_t)(speed * 8));
        break;
    }
}

void Particles_Init(PARTICLES *const p)
{
    memset(p->alive, 0, sizeof(p->alive));
    p->spawn_interval = 2;
    p->velocity_max = 10;
}

void Particles_UpdateAndRender(
    PARTICLES *const p, VIDEO *const video, uint32_t *const rng,
    const PARTICLES_ORIGIN origin, const uint8_t color)
{
    ASSERT(p->spawn_interval == 2 && p->velocity_max == 10);
    ASSERT(origin >= PARTICLES_TOP && origin <= PARTICLES_TOP_LEFT);
    p->cycle++;
    if (p->cycle > p->spawn_interval * PARTICLES_COUNT) {
        p->cycle = 0;
    }
    if (p->cycle % p->spawn_interval == 0) {
        M_Spawn(p, rng, origin, p->cycle / p->spawn_interval);
    }
    for (uint32_t i = 0; i < PARTICLES_COUNT; i++) {
        if (p->alive[i] == 0) {
            continue;
        }
        Sprite_Restore16(
            video, Math16_ToPixel(p->x[i]), Math16_ToPixel(p->y[i]), 8, 1);
        p->x[i] = Math16_Wrap(p->x[i] + p->velocity_x[i]);
        p->y[i] = Math16_Wrap(p->y[i] + p->velocity_y[i]);
        if (p->x[i] < 0 || p->x[i] > 639 * 16 || p->y[i] < 0
            || p->y[i] > 399 * 16) {
            p->alive[i] = 0;
        }
    }
    for (uint32_t i = 0; i < PARTICLES_COUNT; i++) {
        if (p->alive[i] != 0) {
            Video_Pixel(
                video, Math16_ToPixel(p->x[i]), Math16_ToPixel(p->y[i]), color);
        }
    }
}
