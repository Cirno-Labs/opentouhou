#include "th02/music_room/polygons.h"

#include "pc98/zun/master_rng.h"

static void M_InitPolygon(
    MUSIC_POLYGON *const polygon, uint32_t *const seed, const bool respawn)
{
    polygon->x = MasterRng_Next(seed) % 640;
    polygon->y_subpixel = respawn ? -1600 : MasterRng_Next(seed) % 6400;
    polygon->velocity_x = respawn ? 8 - (MasterRng_Next(seed) & 15)
                                  : 4 - (MasterRng_Next(seed) & 7);
    if (polygon->velocity_x == 0) {
        polygon->velocity_x = 1;
    }
    polygon->velocity_y_subpixel = 32 + (MasterRng_Next(seed) & 3) * 16;
    polygon->angle = (uint8_t)MasterRng_Next(seed);
    polygon->angular_velocity = (int8_t)(4 - (MasterRng_Next(seed) & 7));
    if (polygon->angular_velocity == 0) {
        polygon->angular_velocity = 4;
    }
}

void MusicPolygons_UpdateAndRender(
    MUSIC_POLYGONS *const state, uint32_t *const seed,
    const MUSIC_POLYGON_DRAW draw, void *const context)
{
    if (!state->initialized) {
        for (size_t i = 0; i < MUSIC_POLYGON_COUNT; i++) {
            M_InitPolygon(&state->polygons[i], seed, false);
        }
        state->initialized = true;
    }
    for (size_t i = 0; i < MUSIC_POLYGON_COUNT; i++) {
        MUSIC_POLYGON *const polygon = &state->polygons[i];
        MATH16_VECTOR vertices[7];
        const uint8_t count = i / 4 + 3;
        const int16_t radius = (i & 3) * 16 + 64;
        const int16_t y = Math16_ToPixel(polygon->y_subpixel);
        for (uint8_t point = 0; point < count; point++) {
            const uint8_t angle =
                (uint8_t)((point * 256 / count) + polygon->angle);
            vertices[point] =
                (MATH16_VECTOR) { Math16_PolarX(polygon->x, radius, angle),
                                  Math16_PolarY(y, radius, angle) };
        }
        vertices[count] = vertices[0];
        polygon->x = Math16_Wrap((int32_t)polygon->x + polygon->velocity_x);
        polygon->y_subpixel = Math16_Wrap(
            (int32_t)polygon->y_subpixel + polygon->velocity_y_subpixel);
        polygon->angle = (uint8_t)(polygon->angle + polygon->angular_velocity);
        if (polygon->x <= 0 || polygon->x >= 639) {
            polygon->velocity_x = Math16_Wrap(-(int32_t)polygon->velocity_x);
        }
        if (polygon->y_subpixel >= 8000) {
            M_InitPolygon(polygon, seed, true);
        }
        if (draw != nullptr) {
            draw(context, vertices, count);
        }
    }
}
