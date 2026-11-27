#pragma once

#include "core/math/vec.h"
#include "stg/timer.h"

#include <stdint.h>

#define ANM_SPRITE_COUNT 0xa00
#define ANM_SCRIPT_COUNT 0xa00
#define ANM_SLOT_COUNT 0x32
#define ANM_INSTR_ARG_COUNT 8

// A rectangle of a loaded ANM file's texture.
typedef struct {
    int32_t slot; // the ANM slot it came from; negative when unused
    float x0;
    float y0;
    float x1;
    float y1; // pixels, in the texture
    float texture_width;
    float texture_height;
    float u0;
    float v0;
    float u1;
    float v1;
    float width;
    float height; // in the header's pixels
    float scale_x;
    float scale_y; // texture size over the header's size
    int32_t id; // the order sprites were loaded in
} ANM_SPRITE;

typedef union {
    int32_t i;
    float f;
} ANM_ARG;

// One parsed ANM script instruction. Jump targets are instruction indices
// instead of byte offsets: args[0] of opcode 4, args[1] of opcode 5 and
// args[2] of opcodes 67 to 78.
typedef struct {
    int16_t opcode;
    int16_t time;
    uint16_t param_mask; // bit n: args[n] may name a variable
    ANM_ARG args[ANM_INSTR_ARG_COUNT];
} ANM_INSTR;

// Holds a script's instructions up to and including its end marker. As in
// the original, VMs share them, so opcode 5's loop counter is shared too.
typedef struct {
    ANM_INSTR *instrs;
    int32_t count;
    int32_t sprite_offset; // added to opcode 3's sprite argument
} ANM_SCRIPT;

typedef enum {
    ANM_EASE_LINEAR,
    ANM_EASE_IN2,
    ANM_EASE_IN3,
    ANM_EASE_IN4,
    ANM_EASE_OUT2,
    ANM_EASE_OUT3,
    ANM_EASE_OUT4,
} ANM_EASE;

typedef enum {
    ANM_INTERP_POS,
    ANM_INTERP_COLOR,
    ANM_INTERP_ALPHA,
    ANM_INTERP_ROTATION,
    ANM_INTERP_SCALE,
    ANM_INTERP_COUNT,
} ANM_INTERP;

#define ANM_INT_VAR_COUNT 6
#define ANM_FLOAT_VAR_COUNT 4

// Holds the state of one sprite that a script animates.
typedef struct {
    const ANM_SCRIPT *script;
    int32_t pc; // -1 once the script ended
    TIMER script_time;
    TIMER wait_timer; // counts down opcode 79's wait
    int16_t script_index;
    int16_t pending_interrupt; // 0 for none
    bool stopped; // waiting for an interrupt

    int16_t sprite; // -1 for none
    int16_t base_sprite; // the sprite the variants count from
    const ANM_SPRITE *sprite_data;
    int32_t sprite_time; // script_time when the sprite was last set

    VEC3 pos;
    VEC3 pos_offset; // set instead of pos while use_pos_offset is set
    VEC3 rotation;
    VEC3 rotation_speed;
    float scale_x;
    float scale_y;
    float scale_speed_x;
    float scale_speed_y;
    float uv_scroll_x;
    float uv_scroll_y;
    float uv_scroll_speed_x;
    float uv_scroll_speed_y;

    // interp_time counts up to interp_length, which is 0 for none.
    TIMER interp_time[ANM_INTERP_COUNT];
    int32_t interp_length[ANM_INTERP_COUNT];
    uint8_t interp_ease[ANM_INTERP_COUNT];
    VEC3 move_from;
    VEC3 move_to;
    VEC3 rotate_from;
    VEC3 rotate_to;
    VEC2 scale_from;
    VEC2 scale_to;
    uint32_t color_from;
    uint32_t color_to;

    // Variables 10000-10003, 10008 and 10009, and 10004-10007.
    int32_t int_vars[ANM_INT_VAR_COUNT];
    float float_vars[ANM_FLOAT_VAR_COUNT];

    // matrix scales the 256-unit quad to the sprite; draw_matrix adds the
    // VM's scale and rotation; size_matrix maps the quad to the sprite's
    // texture coordinates.
    MAT4 matrix;
    MAT4 draw_matrix;
    MAT4 size_matrix;
    uint32_t color; // 0xAARRGGBB
    uint32_t color2; // drawn instead of color while use_color2 is set

    bool shown;
    bool visible;
    bool blend_add;
    bool use_pos_offset;
    bool z_write_off;
    // Make 3D drawing rebuild draw_matrix, unless keep_draw_matrix is set.
    bool rotation_changed;
    bool scale_changed;
    bool keep_draw_matrix;
    bool stage_camera; // 3D drawing goes through the stage camera
    bool use_color2;
    uint8_t flip; // bit 0 mirrors horizontally, bit 1 vertically
    uint8_t anchor; // bit 0 anchors the left edge, bit 1 the top edge
    int16_t auto_rotate;

    uint8_t font_width;
    uint8_t font_height;

    // Identifies one presentation lifetime. Exclude it from simulation hashes
    // and saves.
    uint32_t presentation_generation;
} ANM_VM;
