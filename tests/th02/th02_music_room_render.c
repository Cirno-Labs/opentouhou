#include "tests/th02/th02_music_room_render.h"

#include "th02/music_room/render.c"

MUSIC_ROOM_RENDER_STATE *TestMusicRoomRender_State(void)
{
    return &m_Render;
}

void TestMusicRoomRender_UpdateAndRenderPolygons(
    MUSIC_POLYGONS *const state, uint32_t *const seed)
{
    M_UpdateAndRenderPolygons(state, seed);
}
