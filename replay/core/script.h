#pragma once

#include "core/base/array.h"
#include "core/io/reader.h"

// Drives a game independently of its .rpy format. OTINPUT 1 scripts contain
// "frames N", "input FRAME BUTTONS" and "capture FRAME" lines. Frames start
// at zero and count rendered ticks from the first title update. Inputs stay
// held until the next input line; "none" releases every button. BUTTONS is
// a '+'-separated list of names from the game's binding table. Each input
// and capture list must increase strictly and stay below N. Blank lines
// and '#' comments are allowed. A capture follows that frame's presentation.

typedef struct {
    const char *name;
    uint16_t word;
} SCRIPT_BINDING;

typedef struct {
    uint32_t frame;
    uint16_t buttons;
} SCRIPT_INPUT;

typedef struct {
    uint32_t frames;
    ARRAY(SCRIPT_INPUT) inputs;
    ARRAY(uint32_t) captures;
} INPUT_SCRIPT;

// Reads at most 1 MiB. On failure, leaves out unchanged. Does not close in.
RESULT Script_Read(
    READER *in, const SCRIPT_BINDING *bindings, size_t binding_count,
    INPUT_SCRIPT *out);

// Releases the script's arrays and leaves it empty.
void Script_Free(INPUT_SCRIPT *script);

// Returns the held buttons at frame, or zero outside the script.
uint16_t Script_Buttons(const INPUT_SCRIPT *script, uint32_t frame);

// Returns whether frame requests a screenshot.
bool Script_Capture(const INPUT_SCRIPT *script, uint32_t frame);
