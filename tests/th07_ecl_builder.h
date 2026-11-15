#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds a th07 ECL file byte for byte: the header with its timeline
// offsets, the subroutine table, the subroutines, then the timelines.
// The last timeline ends the file. Tests use it instead of the game's
// data.

#define ECL_BUILDER_MAX_SUBS 16
#define ECL_BUILDER_MAX_TIMELINES 16
#define ECL_BUILDER_MAX_SUB_BYTES 2048
#define ECL_BUILDER_MAX_TIMELINE_BYTES 512
#define ECL_BUILDER_MAX_SIZE 49152
#define ECL_BUILDER_SUB_TABLE 0x44
#define ECL_BUILDER_NAME_SIZE 48

typedef struct {
    uint8_t bytes[ECL_BUILDER_MAX_SUB_BYTES];
    size_t length;
} ECL_BUILDER_SUB;

typedef struct {
    uint8_t bytes[ECL_BUILDER_MAX_TIMELINE_BYTES];
    size_t length;
} ECL_BUILDER_TIMELINE;

typedef struct {
    ECL_BUILDER_SUB subs[ECL_BUILDER_MAX_SUBS];
    int32_t sub_count;
    ECL_BUILDER_TIMELINE timelines[ECL_BUILDER_MAX_TIMELINES];
    int32_t timeline_count;
} ECL_BUILDER;

// Clears b to a file with no subroutines and no timelines.
void EclBuilder_Init(ECL_BUILDER *b);

// Starts a new, empty subroutine and returns its number.
int32_t EclBuilder_BeginSub(ECL_BUILDER *b);

// Appends one instruction to subroutine sub, with size bytes of
// arguments. Returns its byte offset from the start of the subroutine,
// for the jumps that target it.
int32_t EclBuilder_InstrBytes(
    ECL_BUILDER *b, int32_t sub, int32_t time, int16_t opcode,
    uint8_t difficulty_mask, uint16_t param_mask, const void *args,
    size_t size);

// Is like EclBuilder_InstrBytes, with count 32-bit argument words; a
// float argument is passed through its bits.
int32_t EclBuilder_Instr(
    ECL_BUILDER *b, int32_t sub, int32_t time, int16_t opcode,
    uint8_t difficulty_mask, uint16_t param_mask, const uint32_t *words,
    int32_t count);

// Overwrites argument word index of the instruction at offset in sub.
void EclBuilder_SetArg(
    ECL_BUILDER *b, int32_t sub, int32_t offset, int32_t index, uint32_t value);

// Appends opcode 90 to sub: the sprite and the card number in word 0, and
// the 48 bytes of name, CP932 and NUL-padded, XORed with 0xaa as the file
// stores them. A name of 48 bytes or more has no NUL.
int32_t EclBuilder_SpellCard(
    ECL_BUILDER *b, int32_t sub, int32_t time, int16_t sprite, uint16_t index,
    const char *name);

// Starts a new, empty timeline and returns its number.
int32_t EclBuilder_BeginTimeline(ECL_BUILDER *b);

// Appends an entry of size bytes to timeline: the 8-byte header and the
// first size - 8 bytes of the six argument words, the position, the life,
// the item and the score.
void EclBuilder_Entry(
    ECL_BUILDER *b, int32_t timeline, int16_t time, int16_t sub, int16_t opcode,
    int16_t size, const uint32_t *args);

// Assembles the file into out (at least ECL_BUILDER_MAX_SIZE bytes)
// and returns its size. Each subroutine ends with a 12-byte instruction
// of time -1, each timeline with the 4-byte terminator time -1.
size_t EclBuilder_Build(const ECL_BUILDER *b, uint8_t *out);

// Returns the bits of value.
uint32_t EclBuilder_Float(float value);
