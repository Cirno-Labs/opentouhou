#pragma once

// Drawing for the options menu, on both the graphics page and text RAM.

#include "th02/draw.h"
#include "th02/gaiji.h"
#include "th02/settings.h"
#include "th02/tram.h"

typedef DRAW_TEXT OPTIONS_MENU_TEXT;

void OptionsMenuRender_Init(
    const GAIJI *glyphs, const OPTIONS_MENU_TEXT text[3], TRAM_CONSOLE *console,
    uint8_t clear_attribute);
void OptionsMenuRender_Put(
    const TH02_SETTINGS *settings, const GAIJI *glyphs,
    const OPTIONS_MENU_TEXT text[3], int8_t selection, uint16_t attribute);
