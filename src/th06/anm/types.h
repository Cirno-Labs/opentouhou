#pragma once

#include "th06/timer.h"

#include "core/math/vec.h"

#include <stdint.h>

// Loads the sprites and scripts of ANM files and runs a script on a
// sprite once per frame. Drawing is not part of this module.

#define ANM_SPRITE_COUNT 0x800
#define ANM_SCRIPT_COUNT 0x800
#define ANM_SLOT_COUNT 0x80

// A rectangle of a loaded ANM file's texture.
typedef struct {
    int32_t slot; // the ANM slot it came from; negative when unused
    float x0, y0, x1, y1; // pixels, in the texture
    float texture_width, texture_height;
    float u0, v0, u1, v1;
    float width, height;
    int32_t id; // the order sprites were loaded in
} ANM_SPRITE;

// One parsed ANM script instruction. args holds the instruction's raw
// argument words as the file stores them, except that opcode 5's
// args[0] is the resolved instruction index instead of a byte offset.
typedef struct {
    int16_t time;
    uint8_t opcode;
    uint32_t args[4];
} ANM_INSTR;

typedef struct {
    const ANM_INSTR *instrs;
    int32_t count;
    int32_t sprite_offset; // added to opcodes 1 and 16's sprite argument
} ANM_SCRIPT;

// How opcodes 18-20 ease a position interpolation.
typedef enum {
    ANM_MOVE_LINEAR,
    ANM_MOVE_OUT,
    ANM_MOVE_OUT4,
} ANM_MOVE;

// Holds the state of one sprite that a script animates.
typedef struct {
    const ANM_SCRIPT *script;
    int32_t pc; // the next instruction to run; -1 once the script ended
    TIMER script_time;
    int16_t script_index; // the script Anm_StartScript started
    int16_t pending_interrupt; // the label opcode 22 to jump to; 0 for none
    bool stopped; // waiting at a stop instruction for pending_interrupt

    int16_t sprite; // the sprite Anm_SetSprite chose; -1 for none
    int16_t base_sprite; // the sprite the color variants count from
    const ANM_SPRITE *sprite_data;
    int32_t sprite_time; // script_time when the sprite was last set

    VEC3 pos;
    VEC3 pos_offset; // set instead of pos while use_pos_offset is set
    bool use_pos_offset;
    ANM_MOVE move_mode;
    int16_t move_time; // the length of the position interpolation; 0 for none
    TIMER move_timer;
    VEC3 move_from, move_to;

    VEC3 rotation;
    VEC3 rotation_speed;
    int16_t auto_rotate;

    // scale_speed_x/y are added to the scale every frame while scale_time
    // is 0 (opcode 11). scale_target_x/y, scale_time and scale_timer run an
    // interpolation from scale_from_x/y to them (opcode 30); opcode 30
    // also sets scale_speed_x/y to 0, so nothing is added once it ends.
    float scale_x, scale_y;
    float scale_speed_x, scale_speed_y;
    float scale_target_x, scale_target_y;
    float scale_from_x, scale_from_y;
    int16_t scale_time;
    TIMER scale_timer;

    uint32_t color; // 0xAARRGGBB
    uint32_t fade_from, fade_to;
    int16_t fade_time; // the length of the alpha interpolation; 0 for none
    TIMER fade_timer;

    bool shown, visible, blend_add, color_op, z_write_off;
    uint8_t flip; // bit 0 mirrors horizontally, bit 1 vertically
    uint8_t anchor; // bit 0 anchors the left edge, bit 1 the top edge

    float uv_scroll_x, uv_scroll_y;
    float uv_scale_x, uv_scale_y; // the sprite's size in texture coordinates

    uint8_t font_width, font_height;
} ANM_VM;
