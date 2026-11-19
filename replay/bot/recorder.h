#pragma once

#include "core/base/result.h"
#include "core/input/schema.h"
#include "core/math/vec.h"

#include <stdint.h>

// Plays one practice stage headless with the bot (bot.h) and saves it as
// the game's own replay file, a new .rpy for replay cases. A game gives a
// RECORDER_GAME; Recorder_Main parses the command line with core/args.

typedef enum {
    RECORDER_STAGE_CLEARED,
    RECORDER_GAME_OVER,
} RECORDER_END;

// Names the values of one choice of a run, such as its difficulty, in the
// order of their numbers.
typedef struct {
    const char *const *names;
    int32_t count;
} RECORDER_CHOICE;

typedef struct {
    // Names the program, such as "th06record".
    const char *program;
    const INPUT_SCHEMA *input_schema;
    RECORDER_CHOICE difficulties;
    RECORDER_CHOICE stages;
    RECORDER_CHOICE characters;
    RECORDER_CHOICE shot_types;
    // Loads the game from Shell_GetFiles and starts practice of the stage
    // as the title's practice menu does.
    RESULT (*start)(
        int32_t difficulty, int32_t stage, int32_t character,
        int32_t shot_type);
    bool (*tick)(const void *input);
    // Writes the offsets from the player to up to max threats and the four
    // play area edges, nearest first, and returns their number. Changes no
    // game state.
    int32_t (*list_threats)(VEC2 *offsets, int32_t max);
    // Returns whether the run has ended, and how in *out.
    bool (*has_ended)(RECORDER_END *out);
    RESULT (*save)(const char *path);
    // Stops the game, also after start failed.
    void (*stop)(void);
} RECORDER_GAME;

// Runs the program of game and returns its exit status: 0 on success,
// 1 on an error.
int Recorder_Main(const RECORDER_GAME *game, int argc, char **argv);
