#pragma once

#include "core/base/result.h"
#include "core/config.h"
#include "core/input.h"
#include "core/patch/stack.h"

#define APP_TICK_RATE 60

typedef struct {
    // Identifies the game in paths and settings, as in "th06".
    const char *id;
    // Sets the window title, in UTF-8.
    const char *title;
    // Names the game's version, as tools/shared/versioning.py gives it.
    const char *version;
    // Sets the size of the game's screen in pixels.
    int logical_width;
    int logical_height;
    // Sets the game's input struct and actions. shell/game creates the
    // INPUT_STATE from it and passes tick a buffer of schema->size *
    // schema->players bytes, one player's input after another.
    const INPUT_SCHEMA *input_schema;
    // Describes the game's own settings and points at the struct that holds
    // them. shell/game loads them from its INI before init and saves them
    // after shutdown. Both are nullptr for a game without settings.
    const CONFIG_SCHEMA *settings_schema;
    void *settings;
    // Describes the game to thcrap patches. shell/game opens the patch stack
    // from it before init. nullptr for a game that takes no patches.
    const PATCH_GAME *patch_game;
    // Names the files that the data folder must hold, and their number.
    // shell/game looks for them before it opens the window, as Vfs_Open
    // finds files. A missing file stops the start with a message that
    // names it and the data folder.
    const char *const *data_files;
    int32_t data_file_count;
    // Prepares the game. Called once, after the window exists.
    RESULT (*init)(void);
    // Advances the game by one 60 Hz step and draws it through core/gfx.
    // shell/game starts and finishes the tick's drawing around the call.
    // input points to input_schema->players input structs. Returns false
    // when the game wants to quit.
    bool (*tick)(const void *input);
    // Releases what init acquired. Called once, also when init failed.
    void (*shutdown)(void);
} GAME_DESC;

// Describes the game that shell/game runs. Each game executable defines it.
extern const GAME_DESC g_GameDesc;
