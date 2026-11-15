#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds th07 ANM files byte for byte, for tests to use instead of the
// game's data.

#define ANM_BUILDER_MAX_ANMS 4
#define ANM_BUILDER_MAX_SPRITES 64
#define ANM_BUILDER_MAX_SCRIPTS 40
#define ANM_BUILDER_MAX_SCRIPT_BYTES 512

typedef struct {
    int32_t id;
    float x, y, w, h;
} ANM_BUILDER_SPRITE;

typedef struct {
    int32_t id;
    uint8_t bytes[ANM_BUILDER_MAX_SCRIPT_BYTES];
    size_t length;
} ANM_BUILDER_SCRIPT;

typedef struct {
    int32_t width, height, format, version;
    uint32_t color_key;
    const char *name; // "@" for a blank texture; nullptr for none
    const char *alpha_name; // nullptr for none
    // The stored image, while image_pixels is not nullptr.
    int16_t image_format, image_width, image_height;
    const uint8_t *image_pixels;
    size_t image_size;
    ANM_BUILDER_SPRITE sprites[ANM_BUILDER_MAX_SPRITES];
    int32_t sprite_count;
    ANM_BUILDER_SCRIPT scripts[ANM_BUILDER_MAX_SCRIPTS];
    int32_t script_count;
} ANM_BUILDER_ANM;

typedef struct {
    ANM_BUILDER_ANM anms[ANM_BUILDER_MAX_ANMS];
    int32_t count;
} ANM_BUILDER;

void AnmBuilder_Init(ANM_BUILDER *b);

// Appends an ANM of version 2 and format 0 whose image file is name.
ANM_BUILDER_ANM *AnmBuilder_Add(
    ANM_BUILDER *b, int32_t width, int32_t height, const char *name);

// Makes anm store an image of size bytes of pixels.
void AnmBuilder_SetImage(
    ANM_BUILDER_ANM *anm, int16_t format, int16_t width, int16_t height,
    const uint8_t *pixels, size_t size);
void AnmBuilder_AddSprite(
    ANM_BUILDER_ANM *anm, int32_t id, float x, float y, float w, float h);

// Starts an empty script numbered id and returns its index.
int32_t AnmBuilder_BeginScript(ANM_BUILDER_ANM *anm, int32_t id);

// Appends an instruction of count argument words and returns its byte
// offset in the script, as jump arguments give it.
int32_t AnmBuilder_Instr(
    ANM_BUILDER_ANM *anm, int32_t script_index, int16_t opcode, int16_t time,
    uint16_t param_mask, const uint32_t *args, int32_t count);
void AnmBuilder_End(ANM_BUILDER_ANM *anm, int32_t script_index);

// Returns the file, allocated from g_HeapAllocator.
uint8_t *AnmBuilder_Build(const ANM_BUILDER *b, size_t *size);

void AnmBuilder_WriteFile(
    const ANM_BUILDER *b, const char *directory, const char *name);
