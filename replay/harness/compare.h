#pragma once

#include "state.h"

#include "core/base/arena.h"
#include "core/image/pixels.h"

// Compares a run's state rows with a case's, and its screenshots with
// those of another run.

// Holds one side's row: frame first, then the game's columns, whose HASH
// ones a case gives only on a check row, then the low 4 bits of the
// running hash.
typedef struct {
    const uint32_t *values;
    uint8_t hash;
} STATE_SIDE;

typedef struct {
    uint64_t rows;
    // Tells whether a row differs in a column, in the low 4 bits of the
    // running hash or in a check, or only one side has it, and then gives
    // the index of the first.
    bool differs;
    uint64_t first_row;
    // Hold that row of each side, with values nullptr on the side that does
    // not have it.
    STATE_SIDE expected;
    STATE_SIDE actual;
    // Gives the first check at or after first_row when check_expected is
    // not nullptr, and each side's row there, whose HASH columns tell which
    // lists differ.
    uint64_t check_row;
    const uint32_t *check_expected;
    const uint32_t *check_actual;
} STATE_DIFF;

typedef struct {
    uint32_t frame;
    int64_t different_pixels;
    // Gives the largest difference of one channel.
    uint8_t max_delta;
} SHOT_DIFF;

typedef struct STATE_COMPARE STATE_COMPARE;

// Starts comparing rows with expected, which stays owned by the caller,
// keeping a running hash of the rows as state.h does. The rows of the
// diff come from arena.
STATE_COMPARE *Compare_OpenState(
    STATE_READER *expected, const HARNESS_GAME *game, ARENA *arena);

// Compares one row of the run, the game's columns, with the next expected
// row.
void Compare_StateRow(
    STATE_COMPARE *compare, uint32_t frame, const uint32_t *row);

// Counts the expected rows left over, destroys compare and writes the
// result to out.
void Compare_CloseState(STATE_COMPARE *compare, STATE_DIFF *out);

// Writes the XOR of a and b, and |a - b| * 8 clamped to 255, per
// channel, with alpha 255. Fills *out with frame, the count of pixels
// that differ and the largest difference of one channel. Reports failure
// when the sizes differ.
RESULT Compare_Images(
    const IMAGE *a, const IMAGE *b, uint32_t frame, IMAGE *xor_out,
    IMAGE *delta_out, SHOT_DIFF *out);

// Returns dir/kind/FRAME.png, the file of the screenshot of frame, from
// allocator.
char *Compare_ShotPath(
    const ALLOCATOR *allocator, const char *dir, const char *kind,
    uint32_t frame);

// Compares every shots/FRAME.png in run_dir with the same file in
// against_dir. Writes xor/FRAME.png and delta/FRAME.png for each pair
// that differs, and shots.tsv with one SHOT_DIFF per pair, into out_dir.
// A file that only one side has is a line with different_pixels -1.
// Writes the count of pairs that differ or miss a side to different.
RESULT Compare_Shots(
    const char *run_dir, const char *against_dir, const char *out_dir,
    uint32_t *different);
