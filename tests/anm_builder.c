// SPDX-License-Identifier: GPL-3.0-only

#include "anm_builder.h"

#include "core/base/assert.h"

#include <string.h>

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

static void M_PutF32(uint8_t *const out, const float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    M_PutU32(out, bits);
}

void AnmBuilder_Init(
    ANM_BUILDER *const b, const int32_t width, const int32_t height)
{
    *b = (ANM_BUILDER) { .width = width, .height = height };
}

void AnmBuilder_AddSprite(
    ANM_BUILDER *const b, const int32_t id, const float x, const float y,
    const float w, const float h)
{
    ASSERT(b->sprite_count < ANM_BUILDER_MAX_SPRITES);
    b->sprites[b->sprite_count++] = (ANM_BUILDER_SPRITE) { id, x, y, w, h };
}

int32_t AnmBuilder_BeginScript(ANM_BUILDER *const b, const int32_t script_id)
{
    ASSERT(b->script_count < ANM_BUILDER_MAX_SCRIPTS);
    const int32_t index = b->script_count++;
    b->scripts[index] = (ANM_BUILDER_SCRIPT) { .script_id = script_id };
    return index;
}

int32_t AnmBuilder_Instr(
    ANM_BUILDER *const b, const int32_t script_index, const int16_t time,
    const uint8_t opcode, const uint32_t *const args, const int32_t args_words)
{
    ANM_BUILDER_SCRIPT *const script = &b->scripts[script_index];
    const int32_t offset = (int32_t)script->length;
    const uint8_t args_size = (uint8_t)(args_words * 4);
    ASSERT(script->length + 4 + args_size <= ANM_BUILDER_MAX_SCRIPT_BYTES);
    uint8_t *const header = &script->bytes[script->length];
    M_PutI16(header, time);
    header[2] = opcode;
    header[3] = args_size;
    for (int32_t i = 0; i < args_words; i++) {
        M_PutU32(&script->bytes[script->length + 4 + i * 4], args[i]);
    }
    script->length += 4 + args_size;
    return offset;
}

size_t AnmBuilder_Build(const ANM_BUILDER *const b, uint8_t *const out)
{
    const size_t header_size = 64;
    const size_t sprite_table_size = (size_t)b->sprite_count * 4;
    const size_t script_table_size = (size_t)b->script_count * 8;
    const size_t sprite_bodies_start =
        header_size + sprite_table_size + script_table_size;
    const size_t sprite_body_size = 20;
    size_t script_bodies_start =
        sprite_bodies_start + (size_t)b->sprite_count * sprite_body_size;

    memset(out, 0, header_size);
    M_PutU32(&out[0], (uint32_t)b->sprite_count);
    M_PutU32(&out[4], (uint32_t)b->script_count);
    M_PutU32(&out[12], (uint32_t)b->width);
    M_PutU32(&out[16], (uint32_t)b->height);

    size_t pos = header_size;
    for (int32_t i = 0; i < b->sprite_count; i++) {
        M_PutU32(
            &out[pos],
            (uint32_t)(sprite_bodies_start + (size_t)i * sprite_body_size));
        pos += 4;
    }
    size_t script_offset = script_bodies_start;
    for (int32_t i = 0; i < b->script_count; i++) {
        M_PutU32(&out[pos], (uint32_t)b->scripts[i].script_id);
        pos += 4;
        M_PutU32(&out[pos], (uint32_t)script_offset);
        pos += 4;
        script_offset += b->scripts[i].length;
    }

    pos = sprite_bodies_start;
    for (int32_t i = 0; i < b->sprite_count; i++) {
        const ANM_BUILDER_SPRITE *const s = &b->sprites[i];
        M_PutU32(&out[pos], (uint32_t)s->id);
        M_PutF32(&out[pos + 4], s->x);
        M_PutF32(&out[pos + 8], s->y);
        M_PutF32(&out[pos + 12], s->w);
        M_PutF32(&out[pos + 16], s->h);
        pos += sprite_body_size;
    }

    pos = script_bodies_start;
    for (int32_t i = 0; i < b->script_count; i++) {
        memcpy(&out[pos], b->scripts[i].bytes, b->scripts[i].length);
        pos += b->scripts[i].length;
    }
    if (b->blank_texture) {
        M_PutU32(&out[28], (uint32_t)pos);
        out[pos++] = '@';
        out[pos++] = 0;
    }

    return pos;
}
