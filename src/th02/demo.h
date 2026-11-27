#pragma once

// The three recorded demos that the title screen plays: 7000 input words
// each, played from frame 18 of their stage until live input or the
// closing fade.

#include "th02/session.h"

#include "core/vfs.h"

#define DEMO_INPUT_COUNT 7000
#define DEMO_PLAY_COUNT (DEMO_INPUT_COUNT - 50)

typedef struct {
    uint16_t keys[DEMO_INPUT_COUNT];
} DEMO_RECORD;

typedef struct {
    const DEMO_RECORD *record;
    uint16_t frame;
    bool ending;
} DEMO;

// Reads a demo of exactly 7000 input words.
RESULT Demo_Read(READER *reader, DEMO_RECORD *out);

// Loads the three demos.
RESULT Demo_Load(VFS *files, DEMO_RECORD out[3]);

// Sets the demo's stage, shot and frame 18 in session.
void Demo_ConfigureSession(TH02_SESSION *session);

// Starts playing record, which must outlive the playback.
void Demo_Begin(DEMO *demo, const DEMO_RECORD *record);

// Reads the next keys; returns false on live input or after word 6949,
// when the stage must fade out, quit and reset sound.
bool Demo_Input(DEMO *demo, uint16_t *keys);
