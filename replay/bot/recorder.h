#pragma once

#include "core/base/result.h"
#include "core/input/schema.h"

#include <stdint.h>

// Plays a run headless with the bot (bot.h) and saves it in the game's own
// format, such as a th06 .rpy or a th01 case script. A game gives a
// RECORDER_GAME; Recorder_Main parses the command line with core/args.
//
// The recorder searches holds of role combinations (bot.h) directly
// against the score, with Monte Carlo tree search. A node is a run prefix
// that survived; the root ends after the script's frames. Each iteration
// descends from the root by UCB1, value + 0.02 * sqrt(ln(parent visits) /
// visits), to the first node with fewer than 16 children, and plays one
// new child segment after its run, followed by 90 frames without input. The
// segment is either 150 frames of random holds or, for every second child, the
// holds of the node's best child with one to three holds changed, lengthened,
// shortened, swapped or replaced. A child whose setbacks rise in that time dies
// and is never selected. Each visit, dead or alive, counts at every node above
// it.
//
// A node's value is the share of its siblings whose best descendant does
// not beat its own, in score and then run length, times the share of its
// visits that came before that descendant was found. The first factor
// keeps the order of scores without knowing their scale; the second sends
// the search away from a branch that stopped improving, back to a sibling
// or an ancestor's sibling. A node whose children all died or ran out is
// never selected again.
//
// The search ends at a node whose game ended, that reached the frame cap or
// the --until value, when every node ran out, or after a budget of
// candidates; it saves the run of that node, or of the best node in score
// and then run length. With --minimize, the search instead grows one run by
// random segments that add setbacks or end the game: they play up to 3000
// frames, stop at the setback and need no idle frames. All randomness comes
// from --seed, so the same arguments always give the same run.
//
// On Linux, a node's run continues from a snapshot (snapshot.h) at its end
// instead of a fresh start. At most 256 snapshots live; a node whose
// snapshot went, least recently used first, gets one back by playing its
// run from the nearest node above it that has one. With --no-fork, or
// without fork, each candidate plays its run from a fresh start, for the
// same result.

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
    // Gives the candidates a search may play, or 0 for the default.
    int32_t budget;
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
    // Gives how many frames without input the search plays after each
    // segment, in a throwaway copy, to drop a segment that ends where the
    // player dies a moment later; they are not added to the run. nullptr
    // gives 90.
    int32_t (*idle_frames)(void);
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
