#pragma once

#include <stdint.h>

#define ECL_ARG_COUNT 16
#define ECL_SPELL_CARD_COUNT 141

// Holds one argument word, which an opcode reads as an int or a float.
typedef union {
    int32_t i;
    float f;
} ECL_ARG;

// Holds one parsed instruction. Normally bit n of param_mask marks word n
// as a variable. For opcodes 64 to 72, bits 0 and 1 are the two halves of
// word 0 (sprite, colour), then bits 2 to 7 are words 1 to 6. For 82 and
// 83, bit 1 is the high half of word 0 (colour), then bit n + 1 is word n.
// Instructions stay writable: an operand that names no variable is
// written into the instruction itself, which every enemy on that
// subroutine shares.
typedef struct {
    int32_t time;
    int16_t opcode;
    uint8_t difficulty_mask;
    uint16_t param_mask;
    int32_t target; // the instruction a jump goes to
    ECL_ARG args[ECL_ARG_COUNT];
    char *text; // the spell card name of opcode 90, in UTF-8
} ECL_INSTR;

// Holds the instructions of a subroutine. The last one has time -1 and
// never runs.
typedef struct {
    ECL_INSTR *instrs;
    int32_t count;
} ECL_SUB;

// Returns instruction pc of subroutine sub of the loaded file.
ECL_INSTR *Ecl_GetInstr(int16_t sub, int32_t pc);

// Returns whether sub names a subroutine of the loaded file.
bool Ecl_IsSub(int32_t sub);
