#pragma once

#include "game.h"

#include "core/base/array.h"
#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

// Holds a frame-indexed input script, the inputs.txt of a case. Lines:
//
//     frames N            the run ends after frame N - 1; optional
//     input FRAME KEYS    holds KEYS from FRAME on; "none" releases all
//     fps FRAME RATE      shows RATE from FRAME on; "none" shows nothing
//
// KEYS is a '+'-separated list of binding names. Frame 0 is the first
// presented frame. Frames of each line kind increase strictly. Blank
// lines and '#' comments are allowed.

typedef struct {
    uint32_t frame;
    uint32_t buttons;
} SCRIPT_INPUT;

// Holds a displayed frame rate; available is false before the game's
// first measurement.
typedef struct {
    bool available;
    float rate;
} FPS;

typedef struct {
    uint32_t frame;
    FPS fps;
} SCRIPT_FPS;

typedef struct {
    // Gives the number of frames, or 0 when the script sets none.
    uint32_t frames;
    ARRAY(SCRIPT_INPUT) inputs;
    ARRAY(SCRIPT_FPS) fps;
} SCRIPT;

// Reads a script with game's bindings. On failure, leaves out unchanged.
RESULT Script_Read(READER *in, const HARNESS_GAME *game, SCRIPT *out);

// Cuts the next line off *text in place and returns it without its '#'
// comment, or returns nullptr after the last line. Scripts and screenshot
// lists share the format.
char *Script_NextLine(char **text);

// Writes script with game's bindings, in the line order of the format.
void Script_Write(WRITER *out, const HARNESS_GAME *game, const SCRIPT *script);

// Releases the script's arrays and leaves it empty.
void Script_Free(SCRIPT *script);

// Returns the buttons held at frame.
uint32_t Script_Buttons(const SCRIPT *script, uint32_t frame);

// Returns the rate shown at frame.
FPS Script_Fps(const SCRIPT *script, uint32_t frame);

// Appends an input line when buttons differ from those held at frame.
// Frames must not decrease from one call to the next.
void Script_RecordButtons(SCRIPT *script, uint32_t frame, uint32_t buttons);

// Appends an fps line when fps differs from the rate shown at frame.
// Frames must not decrease from one call to the next.
void Script_RecordFps(SCRIPT *script, uint32_t frame, FPS fps);
