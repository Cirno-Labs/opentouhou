#pragma once

#include "core/base/result.h"
#include "core/input/schema.h"
#include "core/math/vec.h"

#include <stdint.h>

// Plays one practice run headless with a deterministic bot and saves it
// as a .rpy, the way the game itself records a practice session, without
// going through its title or result menus. A RECORDER_GAME says what is
// game-specific.

// How a practice run ended.
typedef enum { RECORDER_STAGE_CLEARED, RECORDER_GAME_OVER } RECORDER_END;

// Describes one game to the recorder; every pointer is set.
typedef struct {
    const char *program; // "th07record", for usage and help
    const INPUT_SCHEMA *input_schema; // the bot's input, zeroed between ticks
    RESULT (*open_data)(const char *data_dir);
    void (*close_data)(void);
    RESULT (*init)(void);
    // Boots to the title, then sets up and starts a practice run exactly
    // as the title's practice menu would, bypassing the menus themselves.
    RESULT (*start_practice)(
        int32_t difficulty, int32_t stage, int32_t character,
        int32_t shot_type);
    bool (*tick)(const void *input);
    // Writes the offsets from the player to up to max threats (bullets,
    // lasers, enemies and the play area's edges as points beside the
    // player), nearest first, and returns how many it wrote. Reads game
    // state only; never changes it, so playback is unaffected.
    int32_t (*list_threats)(VEC2 *offsets, int32_t max);
    // Reports whether the run has ended and how, once true; called after
    // every tick during the run.
    bool (*has_ended)(RECORDER_END *out);
    RESULT (*save_replay)(const char *path);
    void (*shutdown)(void);
} RECORDER_GAME;

// Parses the recorder's options through core/args, plays one practice run
// with the shared bot and writes it as a .rpy; returns the exit status.
int Recorder_Main(const RECORDER_GAME *game, int argc, char **argv);

// The game this program records, which the shared entry point passes on;
// each game's recorder defines it.
extern const RECORDER_GAME g_RecorderGame;
