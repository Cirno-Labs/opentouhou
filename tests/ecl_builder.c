// SPDX-License-Identifier: GPL-3.0-only

#include "ecl_builder.h"

#include "core/base/assert.h"

#include <string.h>

#define M_HEADER_SIZE 16
#define M_INSTR_HEADER_SIZE 12
#define M_ENTRY_HEADER_SIZE 8

static void M_PutU32(uint8_t *const out, const uint32_t value)
{
    out[0] = (uint8_t)(value & 0xff);
    out[1] = (uint8_t)((value >> 8) & 0xff);
    out[2] = (uint8_t)((value >> 16) & 0xff);
    out[3] = (uint8_t)((value >> 24) & 0xff);
}

static void M_PutI16(uint8_t *const out, const int16_t value)
{
    out[0] = (uint8_t)((uint16_t)value & 0xff);
    out[1] = (uint8_t)(((uint16_t)value >> 8) & 0xff);
}

void EclBuilder_Init(ECL_BUILDER *const b)
{
    memset(b, 0, sizeof(*b));
}

int32_t EclBuilder_BeginSub(ECL_BUILDER *const b)
{
    ASSERT(b->sub_count < ECL_BUILDER_MAX_SUBS);
    b->subs[b->sub_count].length = 0;
    return b->sub_count++;
}

int32_t EclBuilder_InstrBytes(
    ECL_BUILDER *const b, const int32_t sub, const int32_t time,
    const int16_t opcode, const uint8_t mask, const void *const args,
    const size_t size)
{
    ECL_BUILDER_SUB *const s = &b->subs[sub];
    const int32_t offset = (int32_t)s->length;
    ASSERT(s->length + M_INSTR_HEADER_SIZE + size + 4 <= sizeof(s->bytes));
    uint8_t *const out = &s->bytes[s->length];
    M_PutU32(out, (uint32_t)time);
    M_PutI16(out + 4, opcode);
    M_PutI16(out + 6, (int16_t)(M_INSTR_HEADER_SIZE + size));
    out[8] = 0;
    out[9] = mask;
    out[10] = 0;
    out[11] = 0;
    if (size > 0) {
        memcpy(out + M_INSTR_HEADER_SIZE, args, size);
    }
    s->length += M_INSTR_HEADER_SIZE + size;
    return offset;
}

int32_t EclBuilder_Instr(
    ECL_BUILDER *const b, const int32_t sub, const int32_t time,
    const int16_t opcode, const uint8_t mask, const uint32_t *const args,
    const int32_t words)
{
    uint8_t bytes[64];
    ASSERT(words >= 0 && (size_t)words * 4 <= sizeof(bytes));
    for (int32_t i = 0; i < words; i++) {
        M_PutU32(&bytes[i * 4], args[i]);
    }
    return EclBuilder_InstrBytes(
        b, sub, time, opcode, mask, bytes, (size_t)words * 4);
}

void EclBuilder_SetArg(
    ECL_BUILDER *const b, const int32_t sub, const int32_t offset,
    const int32_t index, const uint32_t value)
{
    M_PutU32(
        &b->subs[sub].bytes[offset + M_INSTR_HEADER_SIZE + index * 4], value);
}

void EclBuilder_Entry(
    ECL_BUILDER *const b, const int16_t time, const int16_t arg,
    const int16_t opcode, const int16_t size, const uint32_t *const args)
{
    const size_t length =
        size < M_ENTRY_HEADER_SIZE ? M_ENTRY_HEADER_SIZE : (size_t)size;
    ASSERT(length <= 28);
    ASSERT(b->timeline_length + length + 4 <= sizeof(b->timeline));
    uint8_t bytes[28];
    M_PutI16(bytes, time);
    M_PutI16(bytes + 2, arg);
    M_PutI16(bytes + 4, opcode);
    M_PutI16(bytes + 6, size);
    for (int32_t i = 0; i < 5; i++) {
        M_PutU32(bytes + M_ENTRY_HEADER_SIZE + i * 4, args[i]);
    }
    memcpy(&b->timeline[b->timeline_length], bytes, length);
    b->timeline_length += length;
}

size_t EclBuilder_Build(
    const ECL_BUILDER *const b, uint8_t *const out,
    size_t *const timeline_offset)
{
    size_t pos = M_HEADER_SIZE + (size_t)b->sub_count * 4;
    memset(out, 0, pos);
    M_PutI16(out, (int16_t)b->sub_count);
    for (int32_t i = 0; i < b->sub_count; i++) {
        M_PutU32(&out[M_HEADER_SIZE + i * 4], (uint32_t)pos);
        memcpy(&out[pos], b->subs[i].bytes, b->subs[i].length);
        pos += b->subs[i].length;
        M_PutU32(&out[pos], 0xffffffffu);
        pos += 4;
    }
    M_PutU32(&out[4], (uint32_t)pos);
    if (timeline_offset != nullptr) {
        *timeline_offset = pos;
    }
    memcpy(&out[pos], b->timeline, b->timeline_length);
    pos += b->timeline_length;
    M_PutI16(&out[pos], -1);
    M_PutI16(&out[pos + 2], 4);
    pos += 4;
    ASSERT(pos <= ECL_BUILDER_MAX_SIZE);
    return pos;
}

uint32_t EclBuilder_Float(const float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
