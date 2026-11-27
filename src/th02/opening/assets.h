#pragma once

#include "th02/high_score_view/render.h"
#include "th02/music_room/render.h"
#include "th02/options_menu/render.h"
#include "th02/shot_select/render.h"
#include "th02/text_font.h"

#include "core/vfs.h"
#include "pc98/zun/bfnt.h"

typedef struct {
    BFNT logo;
    GAIJI gaiji;
    TITLE_ASSETS title;
    OPTIONS_MENU_TEXT reduction[3];
    HIGH_SCORE_VIEW_ASSETS scores;
    MUSIC_ROOM_ASSETS music;
    SHOT_SELECT_ASSETS shots;
} OPENING_ASSETS;

// Loads the opening into an allocator group that outlives every scene;
// failure leaves cleanup to the group.
RESULT OpeningAssets_Load(
    VFS *files, TEXT_FONT *font, const ALLOCATOR *allocator,
    OPENING_ASSETS *assets);
