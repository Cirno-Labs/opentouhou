#pragma once

#include "case.h"

#include "core/base/result.h"
#include "core/input.h"
#include "stg/chain.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Plays a game's original .rpy headless and compares every calc with an
// OTRC case (case.h). A RUNNER_GAME says what is game-specific.

typedef enum { RUNNER_U32, RUNNER_I32, RUNNER_F32, RUNNER_HASH } RUNNER_KIND;

// Names one column and how a difference prints it.
typedef struct {
    const char *name;
    RUNNER_KIND kind;
} RUNNER_FIELD;

// Builds a RUNNER_FIELD; pass it to a game's X(name, kind) column list.
#define RUNNER_FIELD(name, kind) { #name, RUNNER_##kind },

// Says where the game is after a calc, for the coverage checks.
typedef struct {
    int32_t stage; // as the case's stage column counts it
    bool in_play; // the capture writes this calc's row
    bool replay_ending; // playback requested the replay-end scene
    bool at_title;
} RUNNER_POINT;

// Says what start_replay found in the replay.
typedef struct {
    int32_t last_stage; // as RUNNER_POINT counts it
    int32_t last_stage_inputs; // frames that playback reads, one per calc
} RUNNER_REPLAY;

// Says where a case ends, as the game's capture closes it.
typedef enum {
    // The rows go on past play: the replay-end scene is requested on the
    // last stage, and then the title is reached, before the case ends.
    RUNNER_END_AT_TITLE,
    // The rows start and end with play: calcs before the first in play are
    // not compared, and the calc after the last row is out of play.
    // A last stage of N inputs, once reached, takes N or N + 1 calcs (the
    // case says which); a case may end before it, at a game over.
    RUNNER_END_WITH_PLAY,
} RUNNER_END;

// Describes one game to the runner; every pointer but after_tick is set.
typedef struct {
    const char *id; // the case's game_id, "th06"
    const char *program; // "th06replay", for usage and help
    const RUNNER_FIELD *start_fields;
    int32_t start_count;
    const RUNNER_FIELD *calc_fields;
    int32_t calc_count;
    const INPUT_SCHEMA *input_schema; // its size of zeros is every tick's input
    RUNNER_END end;
    CHAIN *chain; // the game's g_Chain, which the runner hooks
    RESULT (*open_data)(const char *data_dir);
    void (*close_data)(void);
    RESULT (*init)(void);
    bool (*tick)(const void *input);
    void (*shutdown)(void);
    // Keeps the replay's bytes for playback and sets the game up to play
    // it from the title, after one tick. The runner frees data afterwards.
    RESULT (*start_replay)(
        const char *path, const uint8_t *data, size_t size, RUNNER_REPLAY *out);
    void (*release_replay)(void); // after shutdown
    void (*fill_start)(uint32_t *row); // start_count values
    void (*fill_calc)(uint32_t *row); // calc_count values
    void (*observe)(RUNNER_POINT *out);
    RESULT (*after_tick)(void); // outside the calcs; nullptr for none
} RUNNER_GAME;

typedef struct {
    FILE *output;
    bool break_on_diff;
    bool has_break_at;
    uint64_t break_at;
} RUNNER_OPTIONS;

typedef struct {
    uint64_t rows;
    uint64_t first_diff;
    uint64_t restarted_calcs; // calcs after the first of their tick
    int64_t last_stage_calcs;
    uint32_t differences;
    int32_t start_columns;
    int32_t calc_columns;
    int32_t skipped_columns;
    bool different;
    bool complete_schema;
    bool complete_replay;
} RUNNER_REPORT;

// Runs the case at case_path against the replay at replay_path; the game's
// data must be open. Fails on invalid input, a difference, or coverage
// short of game->end; START is compared after the first calc only.
RESULT Runner_Run(
    const RUNNER_GAME *game, const char *case_path, const char *replay_path,
    const RUNNER_OPTIONS *options, RUNNER_REPORT *report);

// Parses the runner's options through core/args, runs every case and
// returns the exit status.
int Runner_Main(const RUNNER_GAME *game, int argc, char **argv);

// The game this program runs, which the shared entry points pass on; each
// game's runner defines it.
extern const RUNNER_GAME g_RunnerGame;
