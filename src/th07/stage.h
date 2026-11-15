#pragma once

#include "th07/anm.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include "stg/timer.h"

#include <stdint.h>

// Runs the stage background: loads the stage's data file and background
// sprites, moves the camera and fog along the stage script, and records
// background objects in layer order and two ANM background layers. Enemy
// scripts start the spell card background and send the stage script to
// its labels.

#define STAGE_SONG_COUNT 4

// Selects what a stage script instruction does at its frame. An
// instruction at frame -1 ends the script; any other opcode does nothing.
typedef enum {
    STAGE_OP_KEYFRAME = 0, // the camera jumps to args
    STAGE_OP_FOG = 1, // args: the color's bits, near, far
    STAGE_OP_FOG_FADE = 2, // args.x: frames
    STAGE_OP_WAIT = 3, // waits for script.label, then runs on after that label
    STAGE_OP_JUMP = 4, // args.x: index, args.y: time
    STAGE_OP_EYE = 5,
    STAGE_OP_EYE_FADE = 6, // args.x: frames, args.y: a STAGE_EASE
    STAGE_OP_AT = 7,
    STAGE_OP_AT_FADE = 8,
    STAGE_OP_UP = 9,
    STAGE_OP_UP_FADE = 10,
    STAGE_OP_FOV = 11,
    STAGE_OP_FOV_FADE = 12,
    STAGE_OP_CLEAR_COLOR = 13,
    STAGE_OP_EYE_START = 14,
    STAGE_OP_EYE_END = 15,
    STAGE_OP_EYE_START_TANGENT = 16,
    STAGE_OP_EYE_END_TANGENT = 17,
    STAGE_OP_EYE_HERMITE = 18, // args.x: frames
    STAGE_OP_AT_START = 19,
    STAGE_OP_AT_END = 20,
    STAGE_OP_AT_START_TANGENT = 21,
    STAGE_OP_AT_END_TANGENT = 22,
    STAGE_OP_AT_HERMITE = 23,
    STAGE_OP_UP_START = 24,
    STAGE_OP_UP_END = 25,
    STAGE_OP_UP_START_TANGENT = 26,
    STAGE_OP_UP_END_TANGENT = 27,
    STAGE_OP_UP_HERMITE = 28,
    STAGE_OP_LAYER0 = 29, // args.x: a script from 0x300, negative hides
    STAGE_OP_LAYER1 = 30, // negative hides layer 0, as the original
    STAGE_OP_LABEL = 31,
} STAGE_OP;

// The reverse of ANM_EASE's order.
typedef enum {
    STAGE_EASE_LINEAR = 0,
    STAGE_EASE_OUT2, // slow down
    STAGE_EASE_OUT3,
    STAGE_EASE_OUT4,
    STAGE_EASE_IN2, // speed up
    STAGE_EASE_IN3,
    STAGE_EASE_IN4,
    STAGE_EASE_HERMITE,
} STAGE_EASE;

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
    VEC3 eye;
    VEC3 at; // relative to eye
    VEC3 up;
    VEC3 dir; // at, normalized
    VEC3 right; // sizes projected sprites, through Anm_SetCameras
    float fov;
} STAGE_CAMERA;

typedef enum {
    STAGE_FADE_EYE,
    STAGE_FADE_AT,
    STAGE_FADE_UP,
    STAGE_FADE_FOV,
    STAGE_FADE_COUNT,
} STAGE_FADE;

typedef struct {
    // From the data file. The names are in UTF-8: text that is valid UTF-8
    // stays, and any other text converts from CP932.
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
        int32_t label; // set by enemy scripts, 0 for none
    } script;

    ANM_VM layers[2];
    uint32_t clear_color;

    struct {
        STAGE_CAMERA current;
        STAGE_CAMERA start, end; // where a fade begins and ends
        STAGE_CAMERA start_tangent, end_tangent;
        VEC3 pos, key_pos, next_key_pos;
        int32_t key_frame, next_key_frame;
        int32_t frames[STAGE_FADE_COUNT];
        TIMER time[STAGE_FADE_COUNT];
        STAGE_EASE ease[STAGE_FADE_COUNT];
        bool jumped; // the next STAGE_OP_EYE moves the petals along
    } camera;

    struct {
        STAGE_FOG current, from, to;
        int32_t frames;
        TIMER time;
    } fog;

    struct {
        int32_t state; // 1 fading in, 2 shown, 0 none; set by enemy scripts
        int32_t frames;
    } spell_card;

    // Modulates the sprites drawn after the stage, while color's alpha is
    // set. Each draw clears it unless keep is set.
    struct {
        uint32_t color;
        bool keep;
    } tint;

    // Run and drawn while spell_card.state is 1 or more.
    struct {
        int32_t count, offset;
        ANM_VM sprites[32];
    } spell_background;

    bool clear_once; // clears the play area to black at the next draw
} STAGE;

extern STAGE g_Stage;

// Adds the update element (priority 7) and the draw elements (priorities
// 3 and 4). Adding loads the files of g_GameManager.run.stage. A data
// file with a quad list or a script that runs past its end, a jump out
// of the script, or a keyframe with no keyframe after it, fails the add.
RESULT Stage_Register(void);

// Removes the three elements and releases the stage's files.
void Stage_Remove(void);

// Sets the tint to color, or to the average of the two while the tint's
// alpha is set.
void Stage_SetTint(uint32_t color);
