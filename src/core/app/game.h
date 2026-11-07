#pragma once

#include <stdint.h>

#define APP_TICK_RATE 60
#define APP_MAX_PLAYERS 2

typedef enum {
    APP_RESULT_OK,
    APP_RESULT_FAILURE,
} APP_RESULT;

// Holds what one tick records for display.
typedef struct FRAME FRAME;

// Holds the input for one tick: each player's action bitmask, in the order
// the game declared its actions.
typedef struct {
    uint32_t actions[APP_MAX_PLAYERS];
} TICK_INPUT;

typedef struct {
    // Identifies the game in paths and settings, as in "th06".
    const char *id;
    // Sets the window title, in UTF-8.
    const char *title;
    // Sets the size of the game's screen in pixels.
    int logical_width;
    int logical_height;
    // Prepares the game. Called once, after the window exists.
    APP_RESULT (*init)(void);
    // Advances the game by one 60 Hz step and records what to show into
    // the frame. Returns false when the game wants to quit.
    bool (*tick)(const TICK_INPUT *input, FRAME *frame);
    // Releases what init acquired. Called once, also when init failed.
    void (*shutdown)(void);
} GAME_DESC;

// Describes the game that core runs. Each game executable defines it.
extern const GAME_DESC g_GameDesc;
