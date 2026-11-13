#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds an ECL file byte for byte, in th06's layout: a header, the
// subroutine offset table, the subroutines' instructions, then the
// timeline. Tests use it instead of the game's data.

#define ECL_BUILDER_MAX_SUBS 16
#define ECL_BUILDER_MAX_SUB_BYTES 4096
#define ECL_BUILDER_MAX_TIMELINE_BYTES 1024
#define ECL_BUILDER_MAX_SIZE 81920

typedef struct {
    uint8_t bytes[ECL_BUILDER_MAX_SUB_BYTES];
    size_t length;
} ECL_BUILDER_SUB;

typedef struct {
    ECL_BUILDER_SUB subs[ECL_BUILDER_MAX_SUBS];
    int32_t sub_count;
    uint8_t timeline[ECL_BUILDER_MAX_TIMELINE_BYTES];
    size_t timeline_length;
} ECL_BUILDER;

// Clears b to a file with no subroutines and an empty timeline.
void EclBuilder_Init(ECL_BUILDER *b);
// Starts a new, empty subroutine and returns its number.
int32_t EclBuilder_BeginSub(ECL_BUILDER *b);
// Appends one instruction to subroutine sub, with size bytes of
// arguments. Returns its byte offset from the start of the subroutine,
// for the jumps that target it.
int32_t EclBuilder_InstrBytes(
    ECL_BUILDER *b, int32_t sub, int32_t time, int16_t opcode, uint8_t mask,
    const void *args, size_t size);
// Is like EclBuilder_InstrBytes, with words 32-bit argument words; a
// float argument is passed through its bits.
int32_t EclBuilder_Instr(
    ECL_BUILDER *b, int32_t sub, int32_t time, int16_t opcode, uint8_t mask,
    const uint32_t *args, int32_t words);
// Overwrites argument word index of the instruction at offset in sub.
void EclBuilder_SetArg(
    ECL_BUILDER *b, int32_t sub, int32_t offset, int32_t index, uint32_t value);
// Appends a timeline entry of size bytes, at most 28: the 8-byte header
// and the first argument bytes. args holds the five argument words: the
// position, the life and item, and the score. A size below 8 still
// writes the header. The file ends the timeline with the 4-byte
// terminator of the retail files, time -1 and arg 4.
void EclBuilder_Entry(
    ECL_BUILDER *b, int16_t time, int16_t arg, int16_t opcode, int16_t size,
    const uint32_t *args);
// Assembles the file into out (at least ECL_BUILDER_MAX_SIZE bytes), ends
// each subroutine and the timeline, and returns the size. Sets
// *timeline_offset, when it is not nullptr, to where the timeline starts.
size_t EclBuilder_Build(
    const ECL_BUILDER *b, uint8_t *out, size_t *timeline_offset);
// Returns the bits of value.
uint32_t EclBuilder_Float(float value);
