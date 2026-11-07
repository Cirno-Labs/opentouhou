#pragma once

#include <stdint.h>

#define APP_TICK_RATE 60
#define INPUT_MAX_PLAYERS 2

typedef enum {
    APP_RESULT_OK,
    APP_RESULT_FAILURE,
} APP_RESULT;

// Holds what one tick records for display.
typedef struct FRAME FRAME;

// Holds one player's input for one tick: a bitmask of the game's actions, in
// the order the game declared them.
typedef struct {
    uint32_t actions;
} PLAYER_INPUT;

// Holds the input of every player for one tick. A one-player game reads p1;
// a versus game loops over players.
typedef struct {
    union {
        PLAYER_INPUT players[INPUT_MAX_PLAYERS];
        struct {
            PLAYER_INPUT p1;
            PLAYER_INPUT p2;
        };
    };
} TICK_INPUT;

static_assert(INPUT_MAX_PLAYERS == 2);

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
