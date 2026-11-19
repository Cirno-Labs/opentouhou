#pragma once

#include "th07/anm.h"

#include <stdint.h>

#define ASCII_STRING_COUNT 0x100
#define ASCII_POPUP_COUNT 0x2d3
#define ASCII_NORMAL_POPUP_COUNT 0x2d0
#define ASCII_BOSS_MARKER_COUNT 4
#define ASCII_PAUSE_SPRITE_COUNT 10
#define ASCII_CONTINUE_SPRITE_COUNT 6

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
    ANM_VM sprites[ASCII_PAUSE_SPRITE_COUNT];
    ANM_VM background;
} PAUSE_MENU;

typedef struct {
    int32_t state;
    int32_t state_frames;
    ANM_VM sprites[ASCII_CONTINUE_SPRITE_COUNT];
    ANM_VM background;
} CONTINUE_MENU;

typedef struct {
    uint8_t digits[8]; // least significant first
    VEC3 pos; // screen pixels
    uint32_t color;
    TIMER timer;
    bool active;
    uint8_t digit_count;
    uint32_t presentation_generation;
} ASCII_POPUP;

typedef struct {
    ANM_VM glyph;
    ANM_VM digit;
    // Shows the cherry, its maximum and the cherry toward the border.
    ANM_VM summary;
    ANM_VM summary_digit;
    ANM_VM summary_bomb_icon;
    // Mark each boss's x below the play area, in screen pixels.
    ANM_VM boss_markers[ASCII_BOSS_MARKER_COUNT];
    bool boss_marker_blue[ASCII_BOSS_MARKER_COUNT];
    ASCII_STRING strings[ASCII_STRING_COUNT];
    int32_t string_count;
    char frame_rate[32];
    bool hide_frame_rate;
    char replay_rate[8];
    uint32_t color;
    float scale_x, scale_y;
    bool in_arena;
    bool highlight;
    // The interrupt AsciiText_SetSummaryInterrupt sent last.
    int16_t summary_interrupt;
    int32_t glyph_width; // pixels at scale 1
    int32_t next_popup;
    int32_t next_popup2;
    PAUSE_MENU pause_menu;
    CONTINUE_MENU continue_menu;
    ANM_VM demo_icon;
    ASCII_POPUP popups[ASCII_POPUP_COUNT];
    // Enables the menu background scripts when framebuffer capture is
    // available.
    bool capture_background;
    // Enables the alternate font for highlighted strings.
    bool highlight_font;
} ASCII_TEXT;
