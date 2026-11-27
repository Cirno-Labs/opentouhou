#pragma once

#include "th02/input.h"

typedef struct {
    uint16_t keys;
    uint8_t box;
} DIALOGUE_RUNTIME;

// Draws a dialogue box; text gets retail byte counts, and any callback may be
// null.
typedef struct {
    void (*wipe)(void *context);
    void (*face)(void *context, int16_t image);
    void (*text)(void *context, uint8_t box, uint8_t line, uint16_t bytes);
    void *context;
} DIALOGUE_VIEW;

typedef enum {
    DIALOGUE_INPUT,
    DIALOGUE_FRAME,
    DIALOGUE_DONE,
} DIALOGUE_RESULT;

typedef enum {
    DIALOGUE_FIRST_INPUT,
    DIALOGUE_REVEAL,
    DIALOGUE_REVEAL_FRAME,
    DIALOGUE_CONFIRM_INPUT,
    DIALOGUE_CONFIRM_FRAMES,
    DIALOGUE_FINISHED,
} DIALOGUE_PHASE;

typedef struct {
    DIALOGUE_RUNTIME *runtime;
    const DIALOGUE_VIEW *view;
    KEY_WAIT wait;
    DIALOGUE_PHASE phase;
    int16_t face;
    uint16_t cursor;
    uint16_t iterations;
    uint8_t box;
} DIALOGUE;

// Starts a box at byte six, with face 255 as black; keep the runtime and
// view storage alive until done.
void DialogueBox_Begin(
    DIALOGUE *dialogue, DIALOGUE_RUNTIME *runtime, const DIALOGUE_VIEW *view,
    int16_t face);

// Runs one dialogue step, returning INPUT before it samples input. As in
// retail, text reveals two bytes at a time, four face-only steps end each
// box, and confirming waits for its own release then press.
DIALOGUE_RESULT DialogueBox_Update(
    DIALOGUE *dialogue, const TH02_INPUT *sample);
