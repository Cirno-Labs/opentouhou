// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/gui.h"
#include "th06/chain.h"

#define M_CALC_PRIORITY 12
#define M_DRAW_PRIORITY 11
#define M_EFFECT_SLOT 0xb
#define M_EFFECT_FIRST_SPRITE 0x2b3
#define M_FRONT_SLOT 0xd
#define M_LOADING_SLOT 0xe
#define M_FACE_SLOT 0xf
#define M_FACE_COUNT 3
#define M_STAGE_FACE_SLOT 0x12
#define M_STAGE_FACE_COUNT 3
#define M_FRONT_SCRIPT 0x600
#define M_BOSS_BAR 0x13
#define M_BOSS_BAR_APPEAR_SCRIPT 0x613
#define M_BOSS_BAR_LEAVE_SCRIPT 0x614
#define M_BOSS_HEALTH_BAR 0x15
#define M_BOMB_BAR_SCRIPT 0x617
#define M_SPELL_BAR_SCRIPT 0x618
#define M_LOADING_SCRIPT 0x619
#define M_PLAYER_PORTRAIT 0x4a0
#define M_BOSS_PORTRAIT_SCRIPT 0x4a2
#define M_BOSS_PORTRAIT_SPRITE 0x4a8
#define M_BOMB_PORTRAIT_SCRIPT 0x4a1
#define M_SPELL_PORTRAIT_SCRIPT 0x4a3
#define M_SPELL_PORTRAIT_SPRITE 0x4a8
#define M_STAGE_NAME_SCRIPT 0x700
#define M_SONG_NAME_SCRIPT 0x701
#define M_LINE_SCRIPT 0x702
#define M_NAMEPLATE_SCRIPT 0x704
#define M_BOMB_NAME_SCRIPT 0x706
#define M_SPELL_NAME_SCRIPT 0x707
#define M_NAME_FONT_SIZE 15
#define M_BANNER_FONT_SIZE 15
#define M_SONG_FONT_SIZE 16
#define M_LINE_FONT_SIZE 15
#define M_SHOOT_WAIT_FRAMES 8
#define M_BOX_OPEN_FRAMES 60
#define M_BOSS_BAR_ALPHA_STEP 4
#define M_BOSS_BAR_ALPHA_RISE_END 0xfc
#define M_BOSS_BAR_ALPHA_MAX 0xff
#define M_BOSS_HEALTH_RISE 0.01f
#define M_BOSS_HEALTH_FALL 0.02f
#define M_BONUS_POPUP_FRAMES 250
#define M_FULL_POWER_POPUP_FRAMES 180
#define M_SPELL_BONUS_POPUP_FRAMES 280
#define M_MAX_POWER 128
#define M_FRAME_Z 0.49f
#define M_FRAME_SIDE 6
#define M_FRAME_TOP 7
#define M_FRAME_BOTTOM 8
#define M_FULL_POWER_SPRITE 0x12
#define M_VALUE_BACKGROUND 0x16

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
