#pragma once

#include "th02/ending/assets.h"
#include "th02/ending/ending.h"
#include "th02/input.h"
#include "th02/palette.h"
#include "th02/session.h"

typedef enum {
    ENDING_TEXT_INPUT,
    ENDING_TEXT_DELAY,
    ENDING_TEXT_DONE,
} ENDING_TEXT_WAIT;

typedef struct {
    ENDING_TEXT_WAIT wait;
    uint16_t frames;
} ENDING_TEXT_EVENT;

typedef enum {
    ENDING_TEXT_BEGIN,
    ENDING_TEXT_REVEAL,
    ENDING_TEXT_REVEAL_WAIT,
    ENDING_TEXT_HOLD,
    ENDING_TEXT_HOLD_WAIT,
    ENDING_TEXT_FINISHED,
} ENDING_TEXT_PHASE;

typedef struct {
    const DRAW_TEXT *line;
    int16_t left;
    int16_t top;
    uint16_t length;
    uint16_t cursor;
    uint16_t frames_per_pair;
    uint16_t style;
    uint16_t keys;
    uint8_t holds;
    bool fast_forward;
    ENDING_TEXT_PHASE phase;
} ENDING_TEXT;

// Loads 1..100 46-byte text records into a group that outlives the script.
RESULT EndingText_Load(
    READER *reader, const DRAW_TEXT_FONT *font, const ALLOCATOR *allocator,
    ENDING_TEXT_SCRIPT *out);

// Overlays twelve verdict titles on the story's records, keeping CR/LF and
// the following bytes up to each terminator, as retail does.
RESULT EndingText_LoadVerdict(
    READER *story, READER *titles, const DRAW_TEXT_FONT *font,
    const ALLOCATOR *allocator, ENDING_TEXT_SCRIPT *out);

// Copies the built-in labels and rank strings into the script's group.
RESULT EndingText_LoadLabels(
    const DRAW_TEXT_FONT *font, const ALLOCATOR *allocator, ENDING_LABELS *out);

// Clears the text line at X=144..495, Y=332..347, ignoring clipping.
void EndingText_Clear(void);

// Draws the first bytes of line; bytes must end on a character or past
// the terminator.
void EndingText_Put(
    const DRAW_TEXT *line, uint16_t bytes, int16_t left, int16_t top,
    uint16_t style);

// Starts typing line out; length is even, <=78 and splits into whole
// characters at every two bytes. Keep line alive until done.
void EndingText_Begin(
    ENDING_TEXT *text, const DRAW_TEXT *line, int16_t left, int16_t top,
    uint16_t length, uint16_t frames_per_pair, uint16_t style,
    bool fast_forward);

// Runs one typing step, returning INPUT before it samples input. Keeps
// retail's counter resets on zero frames and fast-forward delays; only
// fast_forward clears the line at the end.
ENDING_TEXT_EVENT EndingText_Step(ENDING_TEXT *text, const TH02_INPUT *sample);
