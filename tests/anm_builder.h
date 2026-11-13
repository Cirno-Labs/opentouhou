#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds an ANM file byte for byte, in th06's layout: a header, the
// sprite and script offset tables, then the sprites and the scripts'
// instructions. Tests use it instead of the game's data.

#define ANM_BUILDER_MAX_SPRITES 64
#define ANM_BUILDER_MAX_SCRIPTS 32
#define ANM_BUILDER_MAX_SCRIPT_BYTES 512
#define ANM_BUILDER_MAX_SIZE 32768

typedef struct {
    int32_t id;
    float x, y, w, h;
} ANM_BUILDER_SPRITE;

typedef struct {
    int32_t script_id;
    uint8_t bytes[ANM_BUILDER_MAX_SCRIPT_BYTES];
    size_t length;
} ANM_BUILDER_SCRIPT;

typedef struct {
    int32_t width, height;
    // Names the image "@", so that loading creates a blank texture of
    // width x height.
    bool blank_texture;
    ANM_BUILDER_SPRITE sprites[ANM_BUILDER_MAX_SPRITES];
    int32_t sprite_count;
    ANM_BUILDER_SCRIPT scripts[ANM_BUILDER_MAX_SCRIPTS];
    int32_t script_count;
} ANM_BUILDER;

// Clears b to an empty file of width by height pixels.
void AnmBuilder_Init(ANM_BUILDER *b, int32_t width, int32_t height);
// Adds a sprite entry, in pixels.
void AnmBuilder_AddSprite(
    ANM_BUILDER *b, int32_t id, float x, float y, float w, float h);
// Starts a new, empty script numbered script_id, and returns its index for
// AnmBuilder_Instr.
int32_t AnmBuilder_BeginScript(ANM_BUILDER *b, int32_t script_id);
// Appends one instruction to the script at script_index. args holds
// args_words 32-bit words (args_words * 4 bytes, at most 4 words), packed
// as the file's raw argument bytes; a float argument is passed
// through its bits. Returns the instruction's byte offset from the
// script's start, to use as a later opcode 5's jump target.
int32_t AnmBuilder_Instr(
    ANM_BUILDER *b, int32_t script_index, int16_t time, uint8_t opcode,
    const uint32_t *args, int32_t args_words);
// Assembles the whole file into out (at least ANM_BUILDER_MAX_SIZE bytes)
// and returns its size.
size_t AnmBuilder_Build(const ANM_BUILDER *b, uint8_t *out);
