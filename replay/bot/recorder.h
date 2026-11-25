#pragma once

#include "core/base/result.h"
#include "core/input/schema.h"

#include <stdint.h>

// Plays a run headless with the bot (bot.h) and saves it in the game's own
// format, such as a th06 .rpy or a th01 case script. A game gives a
// RECORDER_GAME; Recorder_Main parses the command line with core/args.
//
// The recorder searches holds of role combinations (bot.h) directly
// against the score. A node is a run prefix that survived; the root ends
// after the script's frames. Expanding a node plays candidate segments
// after its run from a fresh start, each followed by 90 frames without
// input, and drops those whose setbacks rise in that time. A candidate is
// either 150 frames of random holds or the holds of one of the 4 best
// nodes so far with one to three holds changed, lengthened, shortened,
// swapped or replaced. Batches of 16 candidates, half of each kind and at
// most 3, run until one leaves a survivor, and each survivor joins the
// frontier as a child node.
//
// The frontier gives the next node to expand: the best score, then the
// longer run, then the earlier node, so that a stretch of equal scores
// still moves on. When no better node in that order has come for 8
// expansions, the nodes under the ancestor k steps above the next node go
// behind all others, where k counts these demotions since the best node
// last changed; a new best node clears them. The search ends at a
// node whose game ended, that reached the frame cap or the --until value,
// or fails after a budget of candidates. With --lose, random holds play
// the run straight to its end instead. All randomness comes from --seed,
// so the same arguments always give the same run.

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
