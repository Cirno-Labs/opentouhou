#pragma once

// Drawing for the main menu, on both the graphics page and text RAM.

#include "th02/draw.h"
#include "th02/menu.h"
#include "th02/settings.h"
#include "th02/title.h"
#include "th02/tram.h"

// The main menu's draw calls, set by MainMenu_Init: the real functions
// below in production, a fake observer in tests.
typedef struct {
    void (*init)(
        const TITLE_ASSETS *assets, const TH02_SETTINGS *settings,
        TRAM_CONSOLE *console, uint8_t clear_attribute);
    void (*put)(
        const TH02_SETTINGS *settings, bool extra_unlocked, int8_t selection,
        uint16_t attribute);
    void (*restore)(const TITLE_ASSETS *assets);
    void (*clear)(TRAM_CONSOLE *console, uint8_t clear_attribute);
} MAIN_MENU_RENDER;

void MainMenuRender_Init(
    const TITLE_ASSETS *assets, const TH02_SETTINGS *settings,
    TRAM_CONSOLE *console, uint8_t clear_attribute);
void MainMenuRender_Put(
    const TH02_SETTINGS *settings, bool extra_unlocked, int8_t selection,
    uint16_t attribute);
void MainMenuRender_Restore(const TITLE_ASSETS *assets);

extern const MAIN_MENU_RENDER g_MainMenuRender;
