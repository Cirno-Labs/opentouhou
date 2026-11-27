#include "tests/th07/th07_anm_builder.h"

#include "tests/th07/th07_data.h"

#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/io/writer.h"

#include <string.h>

#define M_HEADER_SIZE 0x40
#define M_SPRITE_SIZE 20
#define M_IMAGE_HEADER_SIZE 16

static void M_PutU16(uint8_t *const out, const uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void M_PutU32(uint8_t *const out, const uint32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        out[i] = (uint8_t)(value >> (i * 8));
    }
}

static size_t M_Size(const ANM_BUILDER_ANM *const anm)
{
    size_t size = M_HEADER_SIZE
        + (size_t)anm->sprite_count * (4 + M_SPRITE_SIZE)
        + (size_t)anm->script_count * 8;
    for (int32_t i = 0; i < anm->script_count; i++) {
        size += anm->scripts[i].length;
    }
    if (anm->name != nullptr) {
        size += strlen(anm->name) + 1;
    }
    if (anm->alpha_name != nullptr) {
        size += strlen(anm->alpha_name) + 1;
    }
    if (anm->image_pixels != nullptr) {
        size += M_IMAGE_HEADER_SIZE + anm->image_size;
    }
    return size;
}

static void M_WriteAnm(
    WRITER *const out, const ANM_BUILDER_ANM *const anm, const bool last)
{
    const size_t sprites = M_HEADER_SIZE + (size_t)anm->sprite_count * 4
        + (size_t)anm->script_count * 8;
    size_t scripts = sprites + (size_t)anm->sprite_count * M_SPRITE_SIZE;
    size_t names = scripts;
    for (int32_t i = 0; i < anm->script_count; i++) {
        names += anm->scripts[i].length;
    }
    const size_t name = anm->name != nullptr ? names : 0;
    const size_t alpha_name = anm->alpha_name != nullptr
        ? names + (anm->name != nullptr ? strlen(anm->name) + 1 : 0)
        : 0;
    const size_t image = names
        + (anm->name != nullptr ? strlen(anm->name) + 1 : 0)
        + (anm->alpha_name != nullptr ? strlen(anm->alpha_name) + 1 : 0);

    Writer_WriteI32(out, anm->sprite_count);
    Writer_WriteI32(out, anm->script_count);
    Writer_WriteI32(out, 0);
    Writer_WriteI32(out, anm->width);
    Writer_WriteI32(out, anm->height);
    Writer_WriteI32(out, anm->format);
    Writer_WriteU32(out, anm->color_key);
    Writer_WriteU32(out, (uint32_t)name);
    Writer_WriteI32(out, 0);
    Writer_WriteU32(out, (uint32_t)alpha_name);
    Writer_WriteI32(out, anm->version);
    Writer_WriteU32(out, 0);
    Writer_WriteU32(out, anm->image_pixels != nullptr ? (uint32_t)image : 0);
    Writer_WriteU8(out, anm->image_pixels != nullptr);
    Writer_WriteU8(out, 0);
    Writer_WriteU16(out, 0);
    Writer_WriteU32(out, last ? 0 : (uint32_t)M_Size(anm));
    Writer_WriteU32(out, 0);

    for (int32_t i = 0; i < anm->sprite_count; i++) {
        Writer_WriteU32(out, (uint32_t)(sprites + (size_t)i * M_SPRITE_SIZE));
    }
    for (int32_t i = 0; i < anm->script_count; i++) {
        Writer_WriteI32(out, anm->scripts[i].id);
        Writer_WriteU32(out, (uint32_t)scripts);
        scripts += anm->scripts[i].length;
    }
    for (int32_t i = 0; i < anm->sprite_count; i++) {
        const ANM_BUILDER_SPRITE *const s = &anm->sprites[i];
        Writer_WriteI32(out, s->id);
        Writer_WriteF32(out, s->x);
        Writer_WriteF32(out, s->y);
        Writer_WriteF32(out, s->w);
        Writer_WriteF32(out, s->h);
    }
    for (int32_t i = 0; i < anm->script_count; i++) {
        Writer_WriteBytes(out, anm->scripts[i].bytes, anm->scripts[i].length);
    }
    if (anm->name != nullptr) {
        Writer_WriteBytes(out, anm->name, strlen(anm->name) + 1);
    }
    if (anm->alpha_name != nullptr) {
        Writer_WriteBytes(out, anm->alpha_name, strlen(anm->alpha_name) + 1);
    }
    if (anm->image_pixels != nullptr) {
        Writer_WriteBytes(out, "THTX", 4);
        Writer_WriteU16(out, 0);
        Writer_WriteI16(out, anm->image_format);
        Writer_WriteI16(out, anm->image_width);
        Writer_WriteI16(out, anm->image_height);
        Writer_WriteU32(out, (uint32_t)anm->image_size);
        Writer_WriteBytes(out, anm->image_pixels, anm->image_size);
    }
}

void AnmBuilder_Init(ANM_BUILDER *const b)
{
    *b = (ANM_BUILDER) {};
}

ANM_BUILDER_ANM *AnmBuilder_Add(
    ANM_BUILDER *const b, const int32_t width, const int32_t height,
    const char *const name)
{
    ASSERT(b->count < ANM_BUILDER_MAX_ANMS);
    ANM_BUILDER_ANM *const anm = &b->anms[b->count++];
    *anm = (ANM_BUILDER_ANM) {
        .width = width, .height = height, .version = 2, .name = name
    };
    return anm;
}

void AnmBuilder_SetImage(
    ANM_BUILDER_ANM *const anm, const int16_t format, const int16_t width,
    const int16_t height, const uint8_t *const pixels, const size_t size)
{
    anm->image_format = format;
    anm->image_width = width;
    anm->image_height = height;
    anm->image_pixels = pixels;
    anm->image_size = size;
}

void AnmBuilder_AddSprite(
    ANM_BUILDER_ANM *const anm, const int32_t id, const float x, const float y,
    const float w, const float h)
{
    ASSERT(anm->sprite_count < ANM_BUILDER_MAX_SPRITES);
    anm->sprites[anm->sprite_count++] = (ANM_BUILDER_SPRITE) { id, x, y, w, h };
}

int32_t AnmBuilder_BeginScript(ANM_BUILDER_ANM *const anm, const int32_t id)
{
    ASSERT(anm->script_count < ANM_BUILDER_MAX_SCRIPTS);
    const int32_t index = anm->script_count++;
    anm->scripts[index] = (ANM_BUILDER_SCRIPT) { .id = id };
    return index;
}

int32_t AnmBuilder_Instr(
    ANM_BUILDER_ANM *const anm, const int32_t script_index,
    const int16_t opcode, const int16_t time, const uint16_t param_mask,
    const uint32_t *const args, const int32_t count)
{
    ANM_BUILDER_SCRIPT *const script = &anm->scripts[script_index];
    const size_t size = 8 + (size_t)count * 4;
    ASSERT(script->length + size <= ANM_BUILDER_MAX_SCRIPT_BYTES);
    uint8_t *const out = &script->bytes[script->length];
    M_PutU16(out, (uint16_t)opcode);
    M_PutU16(out + 2, (uint16_t)size);
    M_PutU16(out + 4, (uint16_t)time);
    M_PutU16(out + 6, param_mask);
    for (int32_t i = 0; i < count; i++) {
        M_PutU32(out + 8 + i * 4, args[i]);
    }
    const int32_t offset = (int32_t)script->length;
    script->length += size;
    return offset;
}

void AnmBuilder_End(ANM_BUILDER_ANM *const anm, const int32_t script_index)
{
    ANM_BUILDER_SCRIPT *const script = &anm->scripts[script_index];
    ASSERT(script->length + 8 <= ANM_BUILDER_MAX_SCRIPT_BYTES);
    uint8_t *const out = &script->bytes[script->length];
    memset(out, 0, 8);
    M_PutU16(out, 0xffff);
    script->length += 8;
}

uint8_t *AnmBuilder_Build(const ANM_BUILDER *const b, size_t *const size)
{
    WRITER out;
    Writer_OpenBuffer(&out, &g_HeapAllocator, nullptr);
    for (int32_t i = 0; i < b->count; i++) {
        M_WriteAnm(&out, &b->anms[i], i == b->count - 1);
    }
    uint8_t *const bytes = Writer_TakeBuffer(&out, size);
    Result_Ignore(Writer_Close(&out));
    return bytes;
}

void AnmBuilder_WriteFile(
    const ANM_BUILDER *const b, const char *const directory,
    const char *const name)
{
    size_t size;
    uint8_t *const bytes = AnmBuilder_Build(b, &size);
    Th07Data_WriteFile(directory, name, bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
}
