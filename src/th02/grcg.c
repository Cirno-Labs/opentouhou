#include "th02/grcg.h"

#include <string.h>

static GRCG m_Grcg;
const GRCG *const g_Grcg = &m_Grcg;

void Grcg_WriteByte(const uint16_t offset, const uint8_t mask)
{
    for (uint8_t plane = 0; plane < 4; plane++) {
        if (m_Grcg.mode == 0) {
            if (plane == 0) {
                Video_WritePlaneByte(plane, offset, mask);
            }
        } else if ((m_Grcg.mode & (1u << plane)) == 0) {
            const uint8_t previous =
                Video_ReadPlaneByte(g_Video.drawing_page, plane, offset);
            const uint8_t bits = (m_Grcg.mode & 0x40) != 0
                ? (previous & ~mask) | (m_Grcg.tiles[plane] & mask)
                : m_Grcg.tiles[plane];
            Video_WritePlaneByte(plane, offset, bits);
        }
    }
}

void Grcg_WriteWord(const uint16_t offset, const uint16_t mask)
{
    Grcg_WriteByte(offset, mask >> 8);
    Grcg_WriteByte(offset + 1, mask & 255);
}

void Grcg_Init(void)
{
    memset(&m_Grcg, 0, sizeof(m_Grcg));
}

void Grcg_LoadTile(const uint8_t plane, const uint8_t byte)
{
    m_Grcg.tiles[plane] = byte;
}

void Grcg_SetTiles(const uint8_t tile)
{
    memset(m_Grcg.tiles, tile, sizeof(m_Grcg.tiles));
}

void Grcg_SetColor(const uint8_t color)
{
    m_Grcg.mode = 0xc0;
    for (size_t plane = 0; plane < 4; plane++) {
        m_Grcg.tiles[plane] = (color & (1u << plane)) != 0 ? 255 : 0;
    }
}

void Grcg_SetPlanes(const uint8_t planes)
{
    m_Grcg.mode = 0xc0 | (~planes & 15);
}

void Grcg_Off(void)
{
    m_Grcg.mode = 0;
}
