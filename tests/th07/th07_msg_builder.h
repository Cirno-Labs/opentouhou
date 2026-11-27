#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds a th07 MSG file byte for byte: the script count, the script
// offsets, then the scripts in order. Each instruction is its frame
// (u16), its opcode (u8), its argument size (u8) and its arguments. Tests
// use it instead of the game's data.

#define MSG_BUILDER_MAX_SCRIPTS 32
#define MSG_BUILDER_MAX_SCRIPT_BYTES 2048
#define MSG_BUILDER_MAX_SIZE                                                   \
    (4 + 4 * MSG_BUILDER_MAX_SCRIPTS                                           \
     + MSG_BUILDER_MAX_SCRIPTS * MSG_BUILDER_MAX_SCRIPT_BYTES)

typedef struct {
    uint8_t bytes[MSG_BUILDER_MAX_SCRIPT_BYTES];
    size_t size;
    int32_t offset; // in place of the script's own when overridden is set
    bool overridden;
} MSG_BUILDER_SCRIPT;

typedef struct {
    MSG_BUILDER_SCRIPT scripts[MSG_BUILDER_MAX_SCRIPTS];
    int32_t count;
} MSG_BUILDER;

// Clears b to a file with no scripts.
void MsgBuilder_Init(MSG_BUILDER *b);

// Starts a new, empty script and returns its number.
int32_t MsgBuilder_BeginScript(MSG_BUILDER *b);

// Appends an instruction with size bytes of arguments to script.
void MsgBuilder_Instr(
    MSG_BUILDER *b, int32_t script, uint16_t time, uint8_t opcode,
    const void *args, uint8_t size);

// Appends an instruction without arguments.
void MsgBuilder_Op(
    MSG_BUILDER *b, int32_t script, uint16_t time, uint8_t opcode);

// Appends an instruction whose argument is one 32-bit value (ops 4, 7).
void MsgBuilder_OpInt(
    MSG_BUILDER *b, int32_t script, uint16_t time, uint8_t opcode,
    int32_t value);

// Appends an instruction whose arguments are who and index, 16 bits each
// (ops 1, 2, and 5 with the interrupt in index).
void MsgBuilder_OpWho(
    MSG_BUILDER *b, int32_t script, uint16_t time, uint8_t opcode, int16_t who,
    int16_t index);

// Appends an instruction whose arguments are who, index and the UTF-8 text,
// converted to CP932 and NUL-terminated (ops 3, 8).
void MsgBuilder_OpText(
    MSG_BUILDER *b, int32_t script, uint16_t time, uint8_t opcode, int16_t who,
    int16_t index, const char *text);

// Appends an instruction whose argument is one byte, padded to 4 (op 13).
void MsgBuilder_OpByte(
    MSG_BUILDER *b, int32_t script, uint16_t time, uint8_t opcode,
    uint8_t value);

// Makes the file store offset as the offset of script.
void MsgBuilder_SetOffset(MSG_BUILDER *b, int32_t script, int32_t offset);

// Assembles the file into out (at least MSG_BUILDER_MAX_SIZE bytes) and
// returns its size.
size_t MsgBuilder_Build(const MSG_BUILDER *b, uint8_t *out);
