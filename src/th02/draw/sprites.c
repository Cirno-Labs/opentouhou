#include "th02/draw/sprites.h"
#include "th02/draw/private.h"

#include "core/base/assert.h"

static void M_Tiles(const uint8_t tile)
{
    Grcg_Off();
    Grcg_SetTiles(tile);
}

void Draw_Sprite(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    Blit_Put(pattern, left, top);
    M_Tiles(255);
}

void Draw_SpriteRoll(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    Blit_RollPut(pattern, left, top);
    M_Tiles(255);
}

void Draw_SpriteRect(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    if (Blit_PutRect(pattern, g_Clip, left, top)) {
        M_Tiles(255);
    }
}

void Draw_SpriteRollTiny(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    Blit_RollPutTiny(pattern, left, top);
    size_t offset = 0;
    while (pattern->data[offset] == 0x80) {
        Grcg_SetColor(pattern->data[offset + 1]);
        Grcg_Off();
        offset += 34;
    }
}

void Draw_SpritePlane(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const uint8_t source_plane, const uint16_t mode)
{
    Blit_PutPlane(pattern, left, top, source_plane, mode);
    M_Tiles(mode >> 8);
}

void Draw_SpriteRollPlane(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const uint8_t source_plane, const uint16_t mode)
{
    Blit_RollPutPlane(pattern, left, top, source_plane, mode);
    M_Tiles(mode >> 8);
}

void Draw_SpriteZoom(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const int16_t scale)
{
    Blit_Zoom(pattern, g_Clip, left, top, scale);
}

void Draw_SpriteOpaque(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    ASSERT(g_Grcg->mode == 0);
    Blit_OverPut8(pattern, left, top);
}
