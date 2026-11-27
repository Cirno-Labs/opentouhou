#include "th02/music_room/render.h"

#include "th02/draw.h"
#include "th02/music_room/private.h"

#include "core/base/assert.h"

#include <string.h>

static MUSIC_ROOM_RENDER_STATE m_Render;

static void M_Clear(void)
{
    Grcg_SetColor(0);
    Grcg_Off();
    Video_Clear();
}

static void M_Image(const TITLE_IMAGE *const image)
{
    Palette_Import(image->palette);
    Palette_Show();
    Video_PutImage8(
        g_Clip, image->view.pixels, image->view.width, image->view.height, 0,
        0);
}

static void M_Blue(const bool capture)
{
    for (uint16_t offset = 0; offset < MUSIC_ROOM_BLUE_SIZE; offset++) {
        if (capture) {
            m_Render.buffers.blue[offset] =
                Video_ReadPlaneByte(g_Video.drawing_page, 0, offset);
        } else {
            Video_WritePlaneByte(0, offset, m_Render.buffers.blue[offset]);
        }
    }
}

static void M_CommentBackground(const bool capture)
{
    size_t position = 0;
    for (int region = 0; region < 2; region++) {
        const int16_t left = region == 0 ? 20 : 38;
        const int16_t top = region == 0 ? 64 : 80;
        const int16_t bottom = region == 0 ? 80 : 384;
        for (int16_t y = top; y < bottom; y++) {
            for (int16_t x = left; x < left + 40; x++) {
                const uint16_t offset = y * 80 + x;
                for (uint8_t plane = 0; plane < 4; plane++) {
                    uint8_t *const byte =
                        &m_Render.buffers.comment[plane][position];
                    if (capture) {
                        *byte = Video_ReadPlaneByte(
                            g_Video.drawing_page, plane, offset);
                    } else {
                        Video_WritePlaneByte(plane, offset, *byte);
                    }
                }
                position++;
            }
        }
    }
}

static void M_Polygon(
    void *const context, const MATH16_VECTOR *const vertices,
    const uint8_t count)
{
    Draw_PolygonFill(g_Clip, vertices, count);
}

void MusicRoomRender_Track(const uint8_t selection, const uint8_t color)
{
    if (m_Render.assets == nullptr) {
        return;
    }
    ASSERT(selection <= MUSIC_ROOM_QUIT);
    const DRAW_TEXT *const text = &m_Render.assets->choices[selection];
    Video_AccessPage(1 - m_Render.logical_page);
    Draw_Text(
        text->glyphs, text->count, 16, (selection + 6) * 16, color | 0x20);
    Video_AccessPage(m_Render.logical_page);
    Draw_Text(
        text->glyphs, text->count, 16, (selection + 6) * 16, color | 0x20);
}

void MusicRoomRender_Comment(const uint8_t selection)
{
    if (m_Render.assets == nullptr) {
        return;
    }
    ASSERT(selection < MUSIC_ROOM_TRACK_COUNT);
    M_Blue(false);
    M_CommentBackground(false);
    for (int line = 0; line < MUSIC_ROOM_COMMENT_LINES; line++) {
        const DRAW_TEXT *const text =
            &m_Render.assets->comments[selection][line];
        Draw_Text(
            text->glyphs, text->count, line == 0 ? 160 : 304,
            line == 0 ? 64 : (line + 4) * 16, line == 0 ? 0x1f : 0x1d);
    }
    M_Blue(true);
}

void MusicRoomRender_Init(void)
{
    memset(&m_Render, 0, sizeof(m_Render));
    m_Render.seed = 1;
}

void MusicRoomRender_Begin(
    const MUSIC_ROOM_ASSETS *const assets, const TITLE_ASSETS *const menu,
    const uint8_t selection)
{
    m_Render.assets = assets;
    m_Render.menu = menu;
    if (assets == nullptr) {
        return;
    }
    ASSERT(
        assets->background.view.width == VIDEO_WIDTH
        && assets->background.view.height == VIDEO_HEIGHT);
    ASSERT(
        menu->menu_background.view.width == VIDEO_WIDTH
        && menu->menu_background.view.height == VIDEO_HEIGHT);
    ASSERT(selection < MUSIC_ROOM_TRACK_COUNT);
    m_Render.logical_page = 1;
    Palette_SetTone(0);
    Video_ShowPage(0);
    Video_AccessPage(0);
    M_Clear();
    Video_AccessPage(1);
    M_Image(&m_Render.assets->background);
    for (uint8_t i = 0; i <= MUSIC_ROOM_QUIT; i++) {
        MusicRoomRender_Track(i, i == selection ? 15 : 3);
    }
    Video_CopyPage(0);
    Video_AccessPage(1);
    Video_ShowPage(0);
    M_Blue(true);
    M_CommentBackground(true);
    Video_AccessPage(1);
    MusicRoomRender_Comment(selection);
    Video_AccessPage(0);
    MusicRoomRender_Comment(selection);
    Palette_SetTone(100);
}

void MusicRoomRender_Frame(void)
{
    if (m_Render.assets == nullptr) {
        return;
    }
    M_Blue(false);
    Grcg_SetColor(15);
    Grcg_SetPlanes(1);
    MusicPolygons_UpdateAndRender(
        &m_Render.polygons, &m_Render.seed, M_Polygon, nullptr);
    Grcg_Off();
}

void MusicRoomRender_Flip(
    const uint8_t drawing_page, const uint8_t visible_page)
{
    if (m_Render.assets == nullptr) {
        return;
    }
    Video_ShowPage(visible_page);
    m_Render.logical_page = drawing_page;
    Video_AccessPage(drawing_page);
}

void MusicRoomRender_End(void)
{
    if (m_Render.assets == nullptr) {
        return;
    }
    Video_ShowPage(0);
    Video_AccessPage(0);
    M_Clear();
    Video_AccessPage(1);
    M_Image(&m_Render.menu->menu_background);
    Palette_Import(m_Render.menu->menu_palette);
    Palette_Show();
    Video_CopyPage(0);
    Video_AccessPage(0);
}
