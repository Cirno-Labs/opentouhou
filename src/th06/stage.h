#pragma once

#include "th06/anm.h"
#include "th06/timer.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include <stdint.h>

// Runs the stage background: loads the stage's data file and background
// sprites, moves the camera and the fog along the stage script, and
// animates the background objects. Nothing in gameplay reads it; enemy
// scripts only start the spell card background and let the script past
// its waits. Drawing comes with graphics.

#define STAGE_SONG_COUNT 4

// Selects what a stage script instruction does at its frame. Any other
// opcode ends the script.
typedef enum {
    STAGE_OP_KEYFRAME = 0, // the camera passes args; frame -1 holds it there
    STAGE_OP_FOG = 1, // args: the color's bits, near, far
    STAGE_OP_CAMERA_OFFSET = 2, // fades the camera offset to args
    STAGE_OP_OFFSET_FADE = 3, // args.x holds the offset fade's frames
    STAGE_OP_FOG_FADE = 4, // args.x holds the fog fade's frames
    STAGE_OP_WAIT = 5, // waits for script.resume
} STAGE_OP;

typedef struct {
    int32_t frame;
    int16_t opcode;
    VEC3 args;
} STAGE_INSTR;

typedef struct {
    int16_t type; // 0 drawn and animated, 1 animated only
    int16_t script; // from the stage's first background script
    int16_t sprite; // its VM in data.sprites
    VEC3 pos;
    float width, height; // 0 keeps the sprite's size
} STAGE_QUAD;

typedef struct {
    int8_t layer; // 0 to 3
    bool animating; // some sprite still runs a script
    VEC3 pos;
    VEC3 size; // the box tested against the view
    STAGE_QUAD *quads;
    int32_t quad_count;
} STAGE_OBJECT;

typedef struct {
    int16_t object;
    VEC3 pos;
} STAGE_INSTANCE;

typedef struct {
    float near, far;
    uint32_t color;
} STAGE_FOG;

typedef struct {
    // From the data file. The names are in UTF-8.
    struct {
        char *name;
        char *song_names[STAGE_SONG_COUNT];
        char *song_paths[STAGE_SONG_COUNT];
        STAGE_OBJECT *objects;
        int32_t object_count;
        STAGE_INSTANCE *instances;
        int32_t instance_count;
        STAGE_INSTR *script;
        int32_t script_count;
        ANM_VM *sprites; // one per quad
        int32_t sprite_count;
    } data;

    struct {
        TIMER time;
        int32_t index;
        bool resume; // set by an enemy script to pass a wait
    } script;

    struct {
        VEC3 pos;
        VEC3 key_pos, next_key_pos;
        int32_t key_frame, next_key_frame;
    } camera;

    // Fades the offset and zoom of g_GameManager.camera.
    struct {
        VEC3 from, to;
        int32_t frames;
        TIMER time;
    } offset;

    struct {
        STAGE_FOG current, from, to;
        int32_t frames;
        TIMER time;
        bool color_changed;
    } fog;

    struct {
        int32_t state; // 1 fading in, 2 shown, 0 none; set by enemy scripts
        int32_t frames;
        ANM_VM background;
    } spell_card;
} STAGE;

extern STAGE g_Stage;

// Adds the update element (priority 6) and the draw elements (priorities
// 3 and 4). Adding loads the files of g_GameManager.run.stage. A data
// file with an offset, a quad list or a script that runs past its end,
// or a keyframe with no keyframe after it, fails the add.
RESULT Stage_Register(void);
// Removes the three elements and releases the stage's files.
void Stage_Remove(void);
