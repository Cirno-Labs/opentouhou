#include "th07_msg_builder.h"

#include "core/base/assert.h"
#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"

#include <string.h>

#define M_INSTR_HEADER_SIZE 4

static void M_PutU32(uint8_t *const out, const uint32_t value)
{
    out[0] = (uint8_t)(value & 0xff);
    out[1] = (uint8_t)((value >> 8) & 0xff);
    out[2] = (uint8_t)((value >> 16) & 0xff);
    out[3] = (uint8_t)((value >> 24) & 0xff);
}

static void M_PutU16(uint8_t *const out, const uint16_t value)
{
    out[0] = (uint8_t)(value & 0xff);
    out[1] = (uint8_t)((value >> 8) & 0xff);
}

void MsgBuilder_Init(MSG_BUILDER *const b)
{
    memset(b, 0, sizeof(*b));
}

int32_t MsgBuilder_BeginScript(MSG_BUILDER *const b)
{
    ASSERT(b->count < MSG_BUILDER_MAX_SCRIPTS);
    memset(&b->scripts[b->count], 0, sizeof(b->scripts[b->count]));
    return b->count++;
}

void MsgBuilder_Instr(
    MSG_BUILDER *const b, const int32_t script, const uint16_t time,
    const uint8_t opcode, const void *const args, const uint8_t size)
{
    ASSERT(script >= 0 && script < b->count);
    MSG_BUILDER_SCRIPT *const s = &b->scripts[script];
    ASSERT(s->size + M_INSTR_HEADER_SIZE + size <= sizeof(s->bytes));
    uint8_t *const out = &s->bytes[s->size];
    M_PutU16(out, time);
    out[2] = opcode;
    out[3] = size;
    if (size != 0) {
        memcpy(out + M_INSTR_HEADER_SIZE, args, size);
    }
    s->size += M_INSTR_HEADER_SIZE + (size_t)size;
}

void MsgBuilder_Op(
    MSG_BUILDER *const b, const int32_t script, const uint16_t time,
    const uint8_t opcode)
{
    MsgBuilder_Instr(b, script, time, opcode, nullptr, 0);
}

void MsgBuilder_OpInt(
    MSG_BUILDER *const b, const int32_t script, const uint16_t time,
    const uint8_t opcode, const int32_t value)
{
    uint8_t args[4];
    M_PutU32(args, (uint32_t)value);
    MsgBuilder_Instr(b, script, time, opcode, args, sizeof(args));
}

void MsgBuilder_OpWho(
    MSG_BUILDER *const b, const int32_t script, const uint16_t time,
    const uint8_t opcode, const int16_t who, const int16_t index)
{
    uint8_t args[4];
    M_PutU16(args, (uint16_t)who);
    M_PutU16(args + 2, (uint16_t)index);
    MsgBuilder_Instr(b, script, time, opcode, args, sizeof(args));
}

void MsgBuilder_OpText(
    MSG_BUILDER *const b, const int32_t script, const uint16_t time,
    const uint8_t opcode, const int16_t who, const int16_t index,
    const char *const text)
{
    char *encoded;
    ASSERT(Result_Log(
        Cp932_Encode(text, strlen(text), &encoded), LOG_LEVEL_ERROR));
    const size_t length = strlen(encoded);
    ASSERT(4 + length + 1 <= UINT8_MAX);
    uint8_t args[UINT8_MAX];
    M_PutU16(args, (uint16_t)who);
    M_PutU16(args + 2, (uint16_t)index);
    memcpy(args + 4, encoded, length + 1);
    Mem_Free(encoded);
    MsgBuilder_Instr(b, script, time, opcode, args, (uint8_t)(4 + length + 1));
}

void MsgBuilder_OpByte(
    MSG_BUILDER *const b, const int32_t script, const uint16_t time,
    const uint8_t opcode, const uint8_t value)
{
    const uint8_t args[4] = { value };
    MsgBuilder_Instr(b, script, time, opcode, args, sizeof(args));
}

void MsgBuilder_SetOffset(
    MSG_BUILDER *const b, const int32_t script, const int32_t offset)
{
    ASSERT(script >= 0 && script < b->count);
    b->scripts[script].offset = offset;
    b->scripts[script].overridden = true;
}

size_t MsgBuilder_Build(const MSG_BUILDER *const b, uint8_t *const out)
{
    M_PutU32(out, (uint32_t)b->count);
    size_t at = 4 + 4 * (size_t)b->count;
    for (int32_t i = 0; i < b->count; i++) {
        const MSG_BUILDER_SCRIPT *const s = &b->scripts[i];
        M_PutU32(
            out + 4 + 4 * (size_t)i,
            (uint32_t)(s->overridden ? s->offset : (int32_t)at));
        memcpy(out + at, s->bytes, s->size);
        at += s->size;
    }
    return at;
}
