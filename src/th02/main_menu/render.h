#pragma once

// Drawing for the main menu, on both the graphics page and text RAM.

#include "th02/draw.h"
#include "th02/menu.h"
#include "th02/settings.h"
#include "th02/title.h"
#include "th02/tram.h"

void MainMenuRender_Init(
    const TITLE_ASSETS *assets, const TH02_SETTINGS *settings,
    TRAM_CONSOLE *console, uint8_t clear_attribute);
void MainMenuRender_Put(
    const TH02_SETTINGS *settings, bool extra_unlocked, int8_t selection,
    uint16_t attribute);
void MainMenuRender_Restore(const TITLE_ASSETS *assets);
