#include "th07_ecl_builder.h"

#include "core/base/assert.h"

#include <string.h>

#define M_INSTR_HEADER_SIZE 12
#define M_ENTRY_HEADER_SIZE 8
#define M_ENTRY_ARGS_SIZE 24
#define M_NAME_KEY 0xaa

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
    const int16_t opcode, const uint8_t difficulty_mask,
    const uint16_t param_mask, const void *const args, const size_t size)
{
    ECL_BUILDER_SUB *const s = &b->subs[sub];
    const int32_t offset = (int32_t)s->length;
    ASSERT(s->length + M_INSTR_HEADER_SIZE + size <= sizeof(s->bytes));
    uint8_t *const out = &s->bytes[s->length];
    M_PutU32(out, (uint32_t)time);
    M_PutU16(out + 4, (uint16_t)opcode);
    M_PutU16(out + 6, (uint16_t)(M_INSTR_HEADER_SIZE + size));
    out[8] = 0;
    out[9] = difficulty_mask;
    M_PutU16(out + 10, param_mask);
    if (size > 0) {
        memcpy(out + M_INSTR_HEADER_SIZE, args, size);
    }
    s->length += M_INSTR_HEADER_SIZE + size;
    return offset;
}

int32_t EclBuilder_Instr(
    ECL_BUILDER *const b, const int32_t sub, const int32_t time,
    const int16_t opcode, const uint8_t difficulty_mask,
    const uint16_t param_mask, const uint32_t *const words, const int32_t count)
{
    uint8_t bytes[64];
    ASSERT(count >= 0 && (size_t)count * 4 <= sizeof(bytes));
    for (int32_t i = 0; i < count; i++) {
        M_PutU32(&bytes[i * 4], words[i]);
    }
    return EclBuilder_InstrBytes(
        b, sub, time, opcode, difficulty_mask, param_mask, bytes,
        (size_t)count * 4);
}

void EclBuilder_SetArg(
    ECL_BUILDER *const b, const int32_t sub, const int32_t offset,
    const int32_t index, const uint32_t value)
{
    M_PutU32(
        &b->subs[sub].bytes[offset + M_INSTR_HEADER_SIZE + index * 4], value);
}

int32_t EclBuilder_SpellCard(
    ECL_BUILDER *const b, const int32_t sub, const int32_t time,
    const int16_t sprite, const uint16_t index, const char *const name)
{
    uint8_t bytes[4 + ECL_BUILDER_NAME_SIZE] = {};
    M_PutU16(bytes, (uint16_t)sprite);
    M_PutU16(bytes + 2, index);
    const size_t length = strlen(name);
    memcpy(
        bytes + 4, name,
        length < ECL_BUILDER_NAME_SIZE ? length : ECL_BUILDER_NAME_SIZE);
    for (size_t i = 4; i < sizeof(bytes); i++) {
        bytes[i] ^= M_NAME_KEY;
    }
    return EclBuilder_InstrBytes(
        b, sub, time, 90, 0xff, 0, bytes, sizeof(bytes));
}

int32_t EclBuilder_BeginTimeline(ECL_BUILDER *const b)
{
    ASSERT(b->timeline_count < ECL_BUILDER_MAX_TIMELINES);
    b->timelines[b->timeline_count].length = 0;
    return b->timeline_count++;
}

void EclBuilder_Entry(
    ECL_BUILDER *const b, const int32_t timeline, const int16_t time,
    const int16_t sub, const int16_t opcode, const int16_t size,
    const uint32_t *const args)
{
    ECL_BUILDER_TIMELINE *const t = &b->timelines[timeline];
    ASSERT(size >= M_ENTRY_HEADER_SIZE);
    ASSERT(size <= M_ENTRY_HEADER_SIZE + M_ENTRY_ARGS_SIZE);
    ASSERT(t->length + (size_t)size <= sizeof(t->bytes));
    uint8_t bytes[M_ENTRY_HEADER_SIZE + M_ENTRY_ARGS_SIZE];
    M_PutU16(bytes, (uint16_t)time);
    M_PutU16(bytes + 2, (uint16_t)sub);
    M_PutU16(bytes + 4, (uint16_t)opcode);
    M_PutU16(bytes + 6, (uint16_t)size);
    for (int32_t i = 0; i < 6; i++) {
        M_PutU32(bytes + M_ENTRY_HEADER_SIZE + i * 4, args[i]);
    }
    memcpy(&t->bytes[t->length], bytes, (size_t)size);
    t->length += (size_t)size;
}

size_t EclBuilder_Build(const ECL_BUILDER *const b, uint8_t *const out)
{
    size_t pos = ECL_BUILDER_SUB_TABLE + (size_t)b->sub_count * 4;
    ASSERT(pos <= ECL_BUILDER_MAX_SIZE);
    memset(out, 0, pos);
    M_PutU16(out, (uint16_t)b->sub_count);
    M_PutU16(out + 2, (uint16_t)b->timeline_count);
    for (int32_t i = 0; i < b->sub_count; i++) {
        const ECL_BUILDER_SUB *const s = &b->subs[i];
        ASSERT(pos + s->length + M_INSTR_HEADER_SIZE <= ECL_BUILDER_MAX_SIZE);
        M_PutU32(&out[ECL_BUILDER_SUB_TABLE + i * 4], (uint32_t)pos);
        memcpy(&out[pos], s->bytes, s->length);
        pos += s->length;
        M_PutU32(&out[pos], 0xffffffffu);
        M_PutU16(&out[pos + 4], 0xffff);
        M_PutU16(&out[pos + 6], M_INSTR_HEADER_SIZE);
        memset(&out[pos + 8], 0, 4);
        pos += M_INSTR_HEADER_SIZE;
    }
    for (int32_t i = 0; i < b->timeline_count; i++) {
        const ECL_BUILDER_TIMELINE *const t = &b->timelines[i];
        ASSERT(pos + t->length + 4 <= ECL_BUILDER_MAX_SIZE);
        M_PutU32(&out[4 + i * 4], (uint32_t)pos);
        memcpy(&out[pos], t->bytes, t->length);
        pos += t->length;
        M_PutU16(&out[pos], 0xffff);
        M_PutU16(&out[pos + 2], 0);
        pos += 4;
    }
    return pos;
}

uint32_t EclBuilder_Float(const float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
