#pragma once

#include "core/base/result.h"
#include "core/input/schema.h"

#include <stdint.h>

// Plays a run headless with the bot (bot.h) and saves it in the game's own
// format, such as a th06 .rpy or a th01 case script. A game gives a
// RECORDER_GAME; Recorder_Main parses the command line with core/args.
//
// The recorder keeps the input struct of every frame and grows the run 150
// frames at a time. Each candidate segment, a random sequence of holds of
// role combinations (bot.h), runs after the kept frames from a fresh start
// and is followed by 90 frames without input; a candidate whose setbacks
// rise in that time is dropped. Batches of 16 candidates, at most 3, run
// until one leaves a survivor; the 4 best survivors by score form the
// step's beam, and the best is kept. At a dead end, the run takes the next
// survivor of the step before; when that beam is spent, it backs up by
// more steps each time that depth fails again, until a budget of
// candidates runs out. With --lose, random holds play the run straight to
// its end instead. The same arguments always give the same run.

#define RECORDER_SCORE_MAX 4

// How far a run got, compared in order: the first value decides, then the
// next breaks ties.
typedef struct {
    int64_t values[RECORDER_SCORE_MAX];
    int32_t count;
} RECORDER_SCORE;

typedef struct {
    // Names the program, such as "th06record".
    const char *program;
    const INPUT_SCHEMA *input_schema;
    // Gives the bot.ini, relative to the checkout.
    const char *bot_ini;
    // Gives the frames after which a run stops.
    int32_t max_frames;
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
    // Counts the setbacks so far, which the solver rejects.
    int32_t (*setbacks)(void);
    RECORDER_SCORE (*score)(void);
    bool (*has_ended)(void);
    // Saves the run that has just played: frames input structs, one per
    // frame from the first.
    RESULT (*save)(const char *path, const void *inputs, int32_t frames);
    // Stops the game, also after start failed.
    void (*stop)(void);
} RECORDER_GAME;

// Returns a negative number, 0 or a positive number when a is lower than,
// equal to or higher than b.
int Recorder_CompareScores(RECORDER_SCORE a, RECORDER_SCORE b);

// Restarts the tick rate that the recorder reports, as a fresh run starts.
void Recorder_ResetClock(void);

// Runs the program of game and returns its exit status: 0 on success,
// 1 on an error.
int Recorder_Main(const RECORDER_GAME *game, int argc, char **argv);
