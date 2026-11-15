#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds th07 ANM files byte for byte, for tests to use instead of the
// game's data.

#define TH07_ANM_BUILDER_MAX_ANMS 4
#define TH07_ANM_BUILDER_MAX_SPRITES 8
#define TH07_ANM_BUILDER_MAX_SCRIPTS 24
#define TH07_ANM_BUILDER_MAX_SCRIPT_BYTES 512

typedef struct {
    int32_t id;
    float x, y, w, h;
} TH07_ANM_BUILDER_SPRITE;

typedef struct {
    int32_t id;
    uint8_t bytes[TH07_ANM_BUILDER_MAX_SCRIPT_BYTES];
    size_t length;
} TH07_ANM_BUILDER_SCRIPT;

typedef struct {
    int32_t width, height, format, version;
    uint32_t color_key;
    const char *name; // "@" for a blank texture; nullptr for none
    const char *alpha_name; // nullptr for none
    // The stored image, while image_pixels is not nullptr.
    int16_t image_format, image_width, image_height;
    const uint8_t *image_pixels;
    size_t image_size;
    TH07_ANM_BUILDER_SPRITE sprites[TH07_ANM_BUILDER_MAX_SPRITES];
    int32_t sprite_count;
    TH07_ANM_BUILDER_SCRIPT scripts[TH07_ANM_BUILDER_MAX_SCRIPTS];
    int32_t script_count;
} TH07_ANM_BUILDER_ANM;

typedef struct {
    TH07_ANM_BUILDER_ANM anms[TH07_ANM_BUILDER_MAX_ANMS];
    int32_t count;
} TH07_ANM_BUILDER;

void Th07AnmBuilder_Init(TH07_ANM_BUILDER *b);

// Appends an ANM of version 2 and format 0 whose image file is name.
TH07_ANM_BUILDER_ANM *Th07AnmBuilder_Add(
    TH07_ANM_BUILDER *b, int32_t width, int32_t height, const char *name);

// Makes anm store an image of size bytes of pixels.
void Th07AnmBuilder_SetImage(
    TH07_ANM_BUILDER_ANM *anm, int16_t format, int16_t width, int16_t height,
    const uint8_t *pixels, size_t size);
void Th07AnmBuilder_AddSprite(
    TH07_ANM_BUILDER_ANM *anm, int32_t id, float x, float y, float w, float h);

// Starts an empty script numbered id and returns its index.
int32_t Th07AnmBuilder_BeginScript(TH07_ANM_BUILDER_ANM *anm, int32_t id);

// Appends an instruction of count argument words and returns its byte
// offset in the script, as jump arguments give it.
int32_t Th07AnmBuilder_Instr(
    TH07_ANM_BUILDER_ANM *anm, int32_t script_index, int16_t opcode,
    int16_t time, uint16_t param_mask, const uint32_t *args, int32_t count);
void Th07AnmBuilder_End(TH07_ANM_BUILDER_ANM *anm, int32_t script_index);

// Returns the file, allocated from g_HeapAllocator.
uint8_t *Th07AnmBuilder_Build(const TH07_ANM_BUILDER *b, size_t *size);

void Th07AnmBuilder_WriteFile(
    const TH07_ANM_BUILDER *b, const char *directory, const char *name);
