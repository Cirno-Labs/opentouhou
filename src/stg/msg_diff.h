#pragma once

#include "core/json/types.h"

#include <stdbool.h>
#include <stdint.h>

// Holds one parsed dialogue instruction of a MSG script (data/msg*.dat).
// A game's own opcodes say which fields an instruction uses; removes_note
// marks a line that MsgDiff_Apply replaced, for the drawing code to hide
// a shown translator note.
typedef struct {
    int32_t time;
    uint8_t opcode;
    int16_t who;
    int16_t index;
    int32_t value;
    char *text; // the text of a line or nameplate instruction, in UTF-8
    bool removes_note;
} MSG_INSTR;

typedef struct {
    MSG_INSTR *instrs;
    int32_t count;
} MSG_SCRIPT;

// Tells MsgDiff_Apply a game's own dialogue format: the opcodes of a line
// and a nameplate instruction, the number of lines a box can hold (past
// which a jdiff line is dropped), and a predicate for the opcodes that
// end a script.
typedef struct {
    uint8_t line;
    uint8_t nameplate;
    int32_t line_count;
    bool (*is_final)(uint8_t opcode);
} MSG_DIFF_FORMAT;

// Applies the jdiff diff to count scripts, as thcrap's MSG patcher
// applies it: script i takes its entry from key "<i>", and each box of a
// time code takes its lines from "<time>_<box>" of that entry, or
// "<time>_h1_<box>" for a nameplate. Fewer lines delete the extra line
// instructions of the box; more lines add copies of its last one, up to
// format.line_count. The box counter restarts at a new time code, and
// whenever a nameplate instruction meets a line instruction or the
// reverse. A jdiff line keeps its markup and translator notes;
// removes_note marks the line that hides a shown note, as thcrap's
// patcher marks it.
void MsgDiff_Apply(
    MSG_SCRIPT *scripts, int32_t count, const JSON_VALUE *diff,
    MSG_DIFF_FORMAT format);
