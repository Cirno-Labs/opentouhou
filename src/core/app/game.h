#pragma once

#include "core/base/result.h"
#include "core/input.h"

#define APP_TICK_RATE 60

// Holds what one tick records for display.
typedef struct FRAME FRAME;

typedef struct {
    // Identifies the game in paths and settings, as in "th06".
    const char *id;
    // Sets the window title, in UTF-8.
    const char *title;
    // Sets the size of the game's screen in pixels.
    int logical_width;
    int logical_height;
    // Sets the game's input struct and actions. core/app creates the
    // INPUT_STATE from it and passes tick a buffer of schema->size *
    // schema->players bytes, one player's input after another.
    const INPUT_SCHEMA *input_schema;
    // Prepares the game. Called once, after the window exists.
    RESULT (*init)(void);
    // Advances the game by one 60 Hz step and records what to show into
    // the frame. input points to input_schema->players input structs.
    // Returns false when the game wants to quit.
    bool (*tick)(const void *input, FRAME *frame);
    // Releases what init acquired. Called once, also when init failed.
    void (*shutdown)(void);
} GAME_DESC;

// Describes the game that core runs. Each game executable defines it.
extern const GAME_DESC g_GameDesc;
