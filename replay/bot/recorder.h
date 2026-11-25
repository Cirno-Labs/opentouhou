#pragma once

#include "core/base/result.h"
#include "core/input/schema.h"
#include "core/math/vec.h"

#include <stdint.h>

// Plays a run headless with the bot (bot.h) and saves it in the game's own
// format, such as a th06 .rpy or a th01 case script. A game gives a
// RECORDER_GAME; Recorder_Main parses the command line with core/args.
//
// The recorder keeps the input struct of every frame. With --segment N, it
// grows the run N frames at a time: each candidate segment, played by a bot
// with a seed of its own, runs after the kept frames from a fresh start and
// is followed by 90 frames without input. With --no-miss, a candidate that
// loses a life in that time is dropped. The candidate with the highest
// progress is kept. When 3 batches of 16 candidates give none, the run backs
// up by more kept segments each time that depth fails again, until a budget
// of candidates runs out. The same arguments always give the same run.

typedef struct {
    int32_t lives;
    // Grows as the run gets further, such as with a boss's phases.
    int32_t progress;
} RECORDER_STATE;

typedef struct {
    // Names the program, such as "th06record".
    const char *program;
    const INPUT_SCHEMA *input_schema;
    // Gives the bot.ini that --bot falls back to, relative to the
    // checkout.
    const char *bot_ini;
    // Sets the start value name, given with --start name=value, before the
    // first start.
    RESULT (*set_start)(const char *name, const char *value);
    // Loads the game from Shell_GetFiles and brings it to the run's first
    // frame. Runs again after stop for a fresh run.
    RESULT (*start)(void);
    // Writes the input of the run's frame-th frame and returns true while
    // the game plays that frame by script, such as the presses that lead
    // from the title into gameplay; returns false once the bot plays.
    bool (*script_input)(int32_t frame, void *input);
    // Runs one frame and returns false when the game quit.
    bool (*tick)(const void *input);
    // Writes the offsets from the player to up to max threats and the play
    // area edges, nearest first, and returns their number. Changes no game
    // state.
    int32_t (*list_threats)(VEC2 *offsets, int32_t max);
    RECORDER_STATE (*state)(void);
    bool (*has_ended)(void);
    // Saves the run that has just played: frames input structs, one per
    // frame from the first.
    RESULT (*save)(const char *path, const void *inputs, int32_t frames);
    // Stops the game, also after start failed.
    void (*stop)(void);
} RECORDER_GAME;

// Restarts the tick rate that the recorder reports, as a fresh run starts.
void Recorder_ResetClock(void);

// Runs the program of game and returns its exit status: 0 on success,
// 1 on an error.
int Recorder_Main(const RECORDER_GAME *game, int argc, char **argv);
