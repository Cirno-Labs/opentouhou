#include "th02/background/private.h"
#include "th02/video.h"

#include "core/base/assert.h"

void Background_SetMap(
    const MAP *const map, const uint8_t *const order, const uint16_t count)
{
    p_Background->map = (TILE_SCROLL_MAP) { map, order, count };
}

void Background_ResetScroll(void)
{
    p_Background->scroll.map_row = 23;
    p_Background->scroll.line = 0;
}

bool Background_Scroll(const PLAYFIELD_PAGES pages, const uint8_t speed)
{
    const int16_t screen_line = pages.scroll;
    const uint8_t front = pages.front;
    const uint8_t back = pages.back;
    ASSERT(screen_line >= 0 && screen_line < 400);
    p_Background->scroll.line =
        (int8_t)((uint8_t)p_Background->scroll.line - (uint8_t)speed);
    if (p_Background->scroll.line < 0) {
        p_Background->scroll.line =
            (int8_t)(uint8_t)(p_Background->scroll.line + 16);
        p_Background->scroll.map_row++;
        const uint16_t section =
            p_Background->scroll.map_row / MAP_SECTION_ROWS;
        if (section >= p_Background->map.count) {
            return true;
        }
        const uint8_t index = p_Background->map.order[section];
        ASSERT(index < MAP_SECTION_COUNT);
        const size_t row = p_Background->scroll.map_row % MAP_SECTION_ROWS;
        for (size_t x = 0; x < TILE_COLUMNS; x++) {
            const uint8_t image =
                p_Background->map.map->sections[index].tiles[row][x];
            p_Background->scroll.row[x] =
                p_Background->cache.images[screen_line / 16][x] = image;
        }
    }
    p_Background->scroll.copy_top = p_Background->scroll.line;
    p_Background->scroll.copy_height = speed;
    uint16_t destination = screen_line * 80 + 4;
    Video_AccessPage(front);
    for (size_t x = 0; x < TILE_COLUMNS; x++) {
        Render_CopyLines(
            destination, p_Background->scroll.row[x],
            p_Background->scroll.copy_top, p_Background->scroll.copy_height,
            TILE_COPY_ALL);
        destination += 2;
    }
    Video_AccessPage(back);
    for (int x = TILE_COLUMNS - 1; x >= -1; x--) {
        destination -= 2;
        const uint8_t image = x >= 0
            ? p_Background->scroll.row[x]
            : (uint16_t)p_Background->scroll.copy_height >> 8;
        Render_CopyLines(
            destination, image, p_Background->scroll.copy_top,
            p_Background->scroll.copy_height, TILE_COPY_ALL);
    }
    return false;
}
