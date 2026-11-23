#pragma once

#include "th06/gui.h"

#include "stg/chain.h"

#define GUI_HUD_BOSS_BAR 0x13
#define GUI_LINE_SCRIPT 0x702

CHAIN_STATUS GuiDraw_Frame(void);

// Loads base_tsa's text images from the patches, when nmlgc/base_tsa is in
// the stack: pre-drawn rows of the stage title or stage logo, the song
// titles, the boss titles and the boss names. A missing image, or one
// whose size is not a whole number of rows, replaces nothing.
void GuiTextImage_Load(void);

// Gives the text sprites their own rows and scripts back, and releases
// the images.
void GuiTextImage_Release(void);

// Shows the rows of stage on the text sprites: the stage title, the
// stage's song, the boss title and the boss name. The boss title and the
// boss name are replaced together or not at all. A sprite whose image
// has no such row keeps its own row and script.
void GuiTextImage_ShowStage(int32_t stage);

// Shows the row of the boss song of stage on the song title sprite.
void GuiTextImage_ShowBossSong(int32_t stage);

// Returns whether a text image replaces sprite, so that no text is drawn
// into its row.
bool GuiTextImage_Replaces(int32_t sprite);

// Returns whether the stage logo image replaces the stage title. The logo
// includes the stage number, so the ASCII stage line is not drawn.
bool GuiTextImage_ShowsStageLogo(void);
