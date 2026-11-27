#include "th02/background/private.h"
#include "th02/clip.h"
#include "th02/draw.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static BACKGROUND m_Background;
const BACKGROUND *const g_Background = &m_Background;
BACKGROUND *const p_Background = &m_Background;

static int16_t M_Row(const int16_t value)
{
    return value / 16 - (value % 16 < 0);
}

static void M_Mark(const int index)
{
    const int cells = TILE_COLUMNS * TILE_ROWS;
    ASSERT(index >= -cells - 2 && index <= cells + TILE_COLUMNS);
    if (index == -cells - 2) {
        p_Background->cache.mode = TILE_IMAGES;
    } else if (index < -1) {
        const int image = index + cells + 1;
        p_Background->cache.images[image / TILE_COLUMNS][image % TILE_COLUMNS] =
            1;
    } else if (index == -1) {
        p_Background->cache.reserved = 1;
    } else if (index < TILE_COLUMNS * TILE_ROWS) {
        p_Background->cache.dirty[index / TILE_ROWS][index % TILE_ROWS] = 1;
    } else if (index < TILE_COLUMNS * TILE_ROWS + TILE_COLUMNS) {
        p_Background->cache.columns[index - TILE_COLUMNS * TILE_ROWS] = 1;
    } else {
        p_Background->cache.redraw_all = 1;
    }
}

static void M_Flush(const TILE_COPY_MODE mode)
{
    const bool black = p_Background->cache.mode == TILE_BLACK;
    if (black) {
        Grcg_SetColor(0);
    }
    if (black || p_Background->cache.mode == TILE_IMAGES) {
        for (size_t x = 0; x < TILE_COLUMNS; x++) {
            if (p_Background->cache.redraw_all != 1
                && p_Background->cache.columns[x] == 0) {
                continue;
            }
            for (size_t y = 0; y < TILE_ROWS; y++) {
                if (p_Background->cache.redraw_all != 1
                    && p_Background->cache.dirty[x][y] == 0) {
                    continue;
                }
                const uint16_t destination = 4 + x * 2 + y * 1280;
                if (black) {
                    Render_Black(destination);
                } else {
                    Render_CopyLines(
                        destination, p_Background->cache.images[y][x], 0, 16,
                        mode);
                }
            }
        }
    }
    if (black) {
        Grcg_Off();
    }
    memset(p_Background->cache.dirty, 0, sizeof(p_Background->cache.dirty));
    memset(p_Background->cache.columns, 0, sizeof(p_Background->cache.columns));
    p_Background->cache.redraw_all = 0;
}

void Background_SetMode(const TILE_MODE mode)
{
    p_Background->cache.mode = (uint8_t)mode;
}

void Background_MarkAll(void)
{
    p_Background->cache.redraw_all = 1;
}

void Background_Reset(void)
{
    p_Background->cache.mode = TILE_IMAGES;
    p_Background->cache.redraw_all = 0;
}

void Background_Mark(
    const int16_t scroll, const int16_t left, const int16_t top,
    const int16_t width, const int16_t height)
{
    if (p_Background->cache.mode == TILE_DISABLED) {
        return;
    }
    const int tile_left = M_Row(left) - 2;
    const int tile_right = M_Row(Math16_Wrap((int32_t)width - 1 + left)) - 2;
    int16_t screen_top = Math16_Wrap((int32_t)top + scroll);
    if (screen_top >= 400) {
        screen_top -= 400;
    }
    const int16_t bottom = Math16_Wrap((int32_t)height - 1 + screen_top);
    const int16_t aligned = Math16_Wrap((uint16_t)screen_top & 0xfff0u);
    for (int x = tile_left; x <= tile_right; x++) {
        if (x < 0 || x >= TILE_COLUMNS) {
            continue;
        }
        ASSERT(bottom < 32752);
        int16_t y = aligned;
        do {
            const int16_t row = M_Row(y >= 400 ? y - 400 : y);
            M_Mark(x * TILE_ROWS + row);
            y = Math16_Wrap((int32_t)y + 16);
        } while (y <= bottom);
        p_Background->cache.columns[x] = 1;
    }
}

void Background_RepaintDirty(void)
{
    M_Flush(TILE_COPY_ALL);
}

void Background_RepaintAll(void)
{
    p_Background->scroll.copy_top = 0;
    p_Background->scroll.copy_height = 16;
    if (p_Background->cache.mode == TILE_IMAGES) {
        for (size_t y = 0; y < TILE_ROWS; y++) {
            for (size_t x = 0; x < TILE_COLUMNS; x++) {
                Render_CopyLines(
                    4 + y * 1280 + x * 2, p_Background->cache.images[y][x],
                    p_Background->scroll.copy_top,
                    p_Background->scroll.copy_height, TILE_COPY_ALL);
            }
        }
    } else {
        const CLIP vertical = { 0, g_Clip->top, 639, g_Clip->bottom };
        Grcg_SetColor(0);
        Draw_BoxFill(&vertical, 32, 16, 423, 384);
    }
    Grcg_Off();
}

void Background_RestoreIntensity(void)
{
    ASSERT(p_Background->cache.mode != TILE_BLACK);
    M_Flush(TILE_COPY_INTENSITY);
}

static void M_Put(
    const uint8_t *const pixels, const size_t left, const size_t top)
{
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t row = 0; row < 16; row++) {
        memcpy(page + (top + row) * VIDEO_WIDTH + left, pixels + row * 16, 16);
    }
}

void Background_SetSources(const MPN *const tiles)
{
    ASSERT(g_Grcg->mode == 0);
    for (size_t image = 0; image < TILE_SOURCE_COUNT; image++) {
        const size_t left = 576 + image / 25 * 16;
        const size_t top = image % 25 * 16;
        static const uint8_t blank[256];
        const uint8_t *const pixels =
            image < tiles->count ? tiles->pixels + image * 256 : blank;
        Video_AccessPage(1);
        M_Put(pixels, left, top);
        Video_AccessPage(0);
        M_Put(pixels, left, top);
        p_Background->sources[image] = left / 8 + top * 80;
    }
    p_Background->last_source = (uint8_t)(tiles->count - 1);
    Video_AccessPage(0);
}

void Background_DrawInitial(const MPN *const tiles)
{
    ASSERT(g_Grcg->mode == 0 && p_Background->map.count >= 3);
    for (size_t y = 0; y < 24; y++) {
        const uint8_t section = p_Background->map.order[y / MAP_SECTION_ROWS];
        ASSERT(section < MAP_SECTION_COUNT);
        const uint8_t *const row = p_Background->map.map->sections[section]
                                       .tiles[y % MAP_SECTION_ROWS];
        for (size_t x = 0; x < TILE_COLUMNS; x++) {
            ASSERT(row[x] < tiles->count);
            M_Put(
                tiles->pixels + (size_t)row[x] * 256, 32 + x * 16,
                368 - y * 16);
        }
        memcpy(p_Background->cache.images[23 - y], row, TILE_COLUMNS);
    }
}

void Background_ShowStage(void)
{
    for (size_t y = 0; y < 24; y++) {
        const uint8_t section = p_Background->map.order[y / MAP_SECTION_ROWS];
        ASSERT(section < MAP_SECTION_COUNT);
        memcpy(
            p_Background->cache.images[23 - y],
            p_Background->map.map->sections[section]
                .tiles[y % MAP_SECTION_ROWS],
            TILE_COLUMNS);
    }
    Video_AccessPage(1);
    for (size_t y = 0; y < 24; y++) {
        for (size_t x = 0; x < TILE_COLUMNS; x++) {
            Render_CopyLines(
                (uint16_t)(4 + y * 1280 + x * 2),
                p_Background->cache.images[y][x], 0, 16, TILE_COPY_ALL);
        }
    }
    Video_AccessPage(0);
    Background_RepaintAll();
}
