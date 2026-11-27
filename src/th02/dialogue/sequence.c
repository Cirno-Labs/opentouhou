#include "th02/dialogue/sequence.h"

#include "core/base/assert.h"

#include <string.h>

static bool M_Wait(DIALOGUE_SEQUENCE *const sequence)
{
    Wait_Delay(&sequence->wait, 1);
    sequence->waiting = true;
    return false;
}

static void M_Begin(
    DIALOGUE_SEQUENCE *const sequence, const PATTERN_BANK *const patterns,
    const DIALOGUE_ASSETS *const assets, DIALOGUE_RUNTIME *const runtime,
    const bool slide)
{
    memset(sequence, 0, sizeof(*sequence));
    sequence->patterns = patterns;
    sequence->render = (DIALOGUE_RENDER) {
        .assets = assets,
    };
    sequence->view = DialogueRender_View(&sequence->render);
    sequence->runtime = runtime;
    if (slide) {
        DialogueSlide_BeginEnter(&sequence->slide, sequence->patterns);
    } else {
        sequence->phase = DIALOGUE_SEQUENCE_BOXES;
    }
}

static void M_Generic(
    DIALOGUE_SEQUENCE *const sequence, const DIALOGUE_SCRIPTS *const scripts,
    const uint8_t stage, const bool post)
{
    ASSERT(stage < 6);
    const unsigned script = stage * 2 + post;
    sequence->faces = scripts->faces[script];
    sequence->count = scripts->counts[script];
    sequence->leave = !post;
    ASSERT(
        (unsigned)sequence->runtime->box + sequence->count
        <= sequence->render.assets->count);
}

void DialogueSequence_Begin(
    DIALOGUE_SEQUENCE *const sequence, const PATTERN_BANK *const patterns,
    const DIALOGUE_ASSETS *const assets, const DIALOGUE_SCRIPTS *const scripts,
    DIALOGUE_RUNTIME *const runtime, const uint8_t stage, const bool post)
{
    M_Begin(sequence, patterns, assets, runtime, true);
    M_Generic(sequence, scripts, stage, post);
}

void DialogueSequence_BeginPart(
    DIALOGUE_SEQUENCE *const sequence, const PATTERN_BANK *const patterns,
    const DIALOGUE_ASSETS *const assets, DIALOGUE_RUNTIME *const runtime,
    const uint8_t *const faces, const uint8_t count)
{
    M_Begin(sequence, patterns, assets, runtime, true);
    ASSERT((unsigned)runtime->box + count <= assets->count);
    sequence->faces = faces;
    sequence->count = count;
}

void DialogueSequence_BeginBare(
    DIALOGUE_SEQUENCE *const sequence, const PATTERN_BANK *const patterns,
    const DIALOGUE_ASSETS *const assets, DIALOGUE_RUNTIME *const runtime,
    const uint8_t *const faces, const uint8_t count)
{
    M_Begin(sequence, patterns, assets, runtime, false);
    ASSERT((unsigned)runtime->box + count <= assets->count);
    sequence->faces = faces;
    sequence->count = count;
}

void DialogueSequence_SlideOut(DIALOGUE_SEQUENCE *const sequence)
{
    ASSERT(sequence->phase == DIALOGUE_SEQUENCE_DONE);
    DialogueSlide_BeginLeave(&sequence->slide, sequence->patterns);
    sequence->phase = DIALOGUE_SEQUENCE_LEAVE;
}

void DialogueSequence_NextPart(
    DIALOGUE_SEQUENCE *const sequence, const uint8_t *const faces,
    const uint8_t count)
{
    ASSERT(sequence->phase == DIALOGUE_SEQUENCE_DONE);
    ASSERT(
        (unsigned)sequence->runtime->box + count
        <= sequence->render.assets->count);
    sequence->faces = faces;
    sequence->count = count;
    sequence->index = 0;
    sequence->box_active = false;
    sequence->leave = false;
    sequence->phase = DIALOGUE_SEQUENCE_BOXES;
}

void DialogueSequence_BeginIntro(
    DIALOGUE_SEQUENCE *const sequence, const PATTERN_BANK *const patterns,
    const DIALOGUE_ASSETS *const assets, DIALOGUE_RUNTIME *const runtime,
    const uint8_t stage)
{
    ASSERT(stage == 1 || stage == 5);
    M_Begin(sequence, patterns, assets, runtime, true);
    sequence->intro_faces[0] = stage == 1 ? 0 : 255;
    sequence->intro_faces[1] = stage == 1 ? 96 : 0;
    sequence->intro_faces[2] = 255;
    sequence->faces = sequence->intro_faces;
    sequence->count = stage == 1 ? 2 : 3;
    ASSERT((unsigned)runtime->box + sequence->count <= assets->count);
}

void DialogueSequence_Continue(
    DIALOGUE_SEQUENCE *const sequence, const DIALOGUE_SCRIPTS *const scripts,
    const uint8_t stage)
{
    ASSERT(sequence->phase == DIALOGUE_SEQUENCE_DONE);
    M_Generic(sequence, scripts, stage, false);
    sequence->index = 0;
    sequence->box_active = false;
    sequence->phase = DIALOGUE_SEQUENCE_BOXES;
}

bool DialogueSequence_Update(
    DIALOGUE_SEQUENCE *const sequence, const TH02_INPUT *const input)
{
    if (sequence->phase == DIALOGUE_SEQUENCE_DONE) {
        return true;
    }
    if (sequence->waiting) {
        if (!Wait_Ready(&sequence->wait, nullptr, 0)) {
            return false;
        }
        sequence->waiting = false;
    }
    while (true) {
        if (sequence->phase == DIALOGUE_SEQUENCE_ENTER
            || sequence->phase == DIALOGUE_SEQUENCE_LEAVE) {
            if (DialogueSlide_Update(&sequence->slide)
                == DIALOGUE_SLIDE_FRAME) {
                return M_Wait(sequence);
            }
            if (sequence->phase == DIALOGUE_SEQUENCE_LEAVE) {
                sequence->phase = DIALOGUE_SEQUENCE_DONE;
                return true;
            }
            sequence->phase = DIALOGUE_SEQUENCE_BOXES;
        }
        if (sequence->index == sequence->count) {
            if (!sequence->leave) {
                sequence->phase = DIALOGUE_SEQUENCE_DONE;
                return true;
            }
            DialogueSlide_BeginLeave(&sequence->slide, sequence->patterns);
            sequence->phase = DIALOGUE_SEQUENCE_LEAVE;
            continue;
        }
        if (!sequence->box_active) {
            DialogueBox_Begin(
                &sequence->box, sequence->runtime, &sequence->view,
                sequence->faces[sequence->index]);
            sequence->box_active = true;
        }
        const DIALOGUE_RESULT result =
            DialogueBox_Update(&sequence->box, input);
        if (result == DIALOGUE_FRAME) {
            return M_Wait(sequence);
        }
        if (result == DIALOGUE_DONE) {
            sequence->index++;
            sequence->box_active = false;
        }
    }
}
