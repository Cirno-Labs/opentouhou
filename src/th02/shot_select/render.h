#pragma once

#include "th02/draw.h"
#include "th02/session.h"
#include "th02/title.h"

typedef struct {
    TITLE_IMAGE background;
    TITLE_FLASH_IMAGE portraits[3];
    DRAW_TEXT descriptions[3][3];
    DRAW_TEXT choose;
    DRAW_TEXT extra_note[2];
    DRAW_TEXT cleared;
} SHOT_SELECT_ASSETS;

// Draws the initial screen and sets session.shot_type to one.
void ShotSelectRender_Begin(
    TH02_SESSION *session, const SHOT_SELECT_ASSETS *assets,
    const bool cleared[3]);
void ShotSelectRender_Description(
    const SHOT_SELECT_ASSETS *assets, uint8_t selection, uint8_t color);
void ShotSelectRender_Background(uint8_t selection, bool highlight);
void ShotSelectRender_Picture(
    const SHOT_SELECT_ASSETS *assets, uint8_t selection, int16_t left,
    int16_t top);
void ShotSelectRender_Darken(int16_t left, int16_t top);
