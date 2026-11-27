#pragma once

#include "th02/dialogue/private.h"
#include "th02/wait.h"

typedef enum {
    DIALOGUE_SEQUENCE_ENTER,
    DIALOGUE_SEQUENCE_BOXES,
    DIALOGUE_SEQUENCE_LEAVE,
    DIALOGUE_SEQUENCE_DONE,
} DIALOGUE_SEQUENCE_PHASE;

typedef struct {
    const PATTERN_BANK *patterns;
    DIALOGUE_SLIDE slide;
    DIALOGUE_RENDER render;
    DIALOGUE_VIEW view;
    DIALOGUE box;
    DIALOGUE_RUNTIME *runtime;
    WAIT wait;
    const uint8_t *faces;
    uint8_t count;
    uint8_t index;
    uint8_t intro_faces[3];
    DIALOGUE_SEQUENCE_PHASE phase;
    bool leave;
    bool waiting;
    bool box_active;
} DIALOGUE_SEQUENCE;

// Starts a dialogue with a slide in, granting the pending score delta
// without clearing it; keep the caches and runtime alive until done.
void DialogueSequence_Begin(
    DIALOGUE_SEQUENCE *sequence, const PATTERN_BANK *patterns,
    const DIALOGUE_ASSETS *assets, const DIALOGUE_SCRIPTS *scripts,
    DIALOGUE_RUNTIME *runtime, uint8_t stage, bool post);

// Starts a part that slides in but not out.
void DialogueSequence_BeginPart(
    DIALOGUE_SEQUENCE *sequence, const PATTERN_BANK *patterns,
    const DIALOGUE_ASSETS *assets, DIALOGUE_RUNTIME *runtime,
    const uint8_t *faces, uint8_t count);

// Starts a part without sliding, score, palette or page changes.
void DialogueSequence_BeginBare(
    DIALOGUE_SEQUENCE *sequence, const PATTERN_BANK *patterns,
    const DIALOGUE_ASSETS *assets, DIALOGUE_RUNTIME *runtime,
    const uint8_t *faces, uint8_t count);

// Slides a completed part out; disabled tiles stay black until Update is
// done.
void DialogueSequence_SlideOut(DIALOGUE_SEQUENCE *sequence);

// Starts the next part after a completed one, keeping the box cursor, page
// and pending tile restoration.
void DialogueSequence_NextPart(
    DIALOGUE_SEQUENCE *sequence, const uint8_t *faces, uint8_t count);

// Starts the stage-2 or Extra introduction, ready for Continue.
void DialogueSequence_BeginIntro(
    DIALOGUE_SEQUENCE *sequence, const PATTERN_BANK *patterns,
    const DIALOGUE_ASSETS *assets, DIALOGUE_RUNTIME *runtime, uint8_t stage);

// Continues the introduction, keeping its box cursor, front page and
// pending tile restoration.
void DialogueSequence_Continue(
    DIALOGUE_SEQUENCE *sequence, const DIALOGUE_SCRIPTS *scripts,
    uint8_t stage);

// Runs one dialogue frame; returns true when done. Held input can reveal a
// whole box in one call, but confirming takes counted release/press frames.
bool DialogueSequence_Update(
    DIALOGUE_SEQUENCE *sequence, const TH02_INPUT *input);
