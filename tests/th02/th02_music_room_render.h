#pragma once

#include "th02/music_room/private.h"

MUSIC_ROOM_RENDER_STATE *TestMusicRoomRender_State(void);

void TestMusicRoomRender_UpdateAndRenderPolygons(
    MUSIC_POLYGONS *state, uint32_t *seed);
