// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/anm.h"

#include <stdint.h>

#define ASCII_STRING_COUNT 0x100
#define ASCII_POPUP_COUNT 0x203
#define ASCII_NORMAL_POPUP_COUNT 0x200
#define ASCII_MENU_SPRITE_COUNT 6

typedef struct {
    char *text;
    VEC3 pos;
    uint32_t color;
    float scale_x, scale_y;
    bool highlight;
    bool in_arena;
} ASCII_STRING;

typedef struct {
    int32_t state;
    int32_t state_frames;
    ANM_VM sprites[ASCII_MENU_SPRITE_COUNT];
    ANM_VM background;
} PAUSE_MENU;

typedef struct {
    uint8_t digits[10];
    VEC3 pos;
    uint32_t color;
    TIMER timer;
    bool active;
    uint8_t digit_count;
    uint32_t presentation_generation;
} ASCII_POPUP;

typedef struct {
    ANM_VM glyph;
    ANM_VM digit;
    ASCII_STRING strings[ASCII_STRING_COUNT];
    int32_t string_count;
    uint32_t color;
    float scale_x, scale_y;
    bool in_arena;
    bool highlight;
    int32_t next_popup;
    int32_t next_popup2;
    PAUSE_MENU pause_menu;
    PAUSE_MENU continue_menu;
    ASCII_POPUP popups[ASCII_POPUP_COUNT];
    // Enables the menu background scripts when framebuffer capture is
    // available.
    bool capture_background;
    // Enables the alternate font for highlighted strings.
    bool highlight_font;
} ASCII_TEXT;
