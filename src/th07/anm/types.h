#pragma once

#include <stdint.h>

#define ANM_SPRITE_COUNT 0xa00
#define ANM_SCRIPT_COUNT 0xa00
#define ANM_SLOT_COUNT 0x32
#define ANM_INSTR_ARG_COUNT 8

// A rectangle of a loaded ANM file's texture.
typedef struct {
    int32_t slot; // the ANM slot it came from; negative when unused
    float x0, y0, x1, y1; // pixels, in the texture
    float texture_width, texture_height;
    float u0, v0, u1, v1;
    float width, height; // in the header's pixels
    float scale_x, scale_y; // texture size over the header's size
    int32_t id; // the order sprites were loaded in
} ANM_SPRITE;

typedef union {
    int32_t i;
    float f;
} ANM_ARG;

// One parsed ANM script instruction. Jump targets are instruction indices
// instead of byte offsets: args[0] of opcode 4, args[1] of opcode 5 and
// args[2] of opcodes 67 to 78.
typedef struct {
    int16_t opcode;
    int16_t time;
    uint16_t param_mask; // bit n: args[n] may name a variable
    ANM_ARG args[ANM_INSTR_ARG_COUNT];
} ANM_INSTR;

// Holds a script's instructions up to and including its end marker. As in
// the original, VMs share them, so opcode 5's loop counter is shared too.
typedef struct {
    ANM_INSTR *instrs;
    int32_t count;
    int32_t sprite_offset; // added to opcode 3's sprite argument
} ANM_SCRIPT;
