#include "th02/dialogue.h"

#include "core/base/assert.h"

#include <string.h>

static DIALOGUE_RESULT M_NextInput(DIALOGUE *const dialogue)
{
    dialogue->phase =
        dialogue->cursor <= 80 ? DIALOGUE_REVEAL : DIALOGUE_CONFIRM_INPUT;
    return DIALOGUE_INPUT;
}

void DialogueBox_Begin(
    DIALOGUE *const dialogue, DIALOGUE_RUNTIME *const runtime,
    const DIALOGUE_VIEW *const view, const int16_t face)
{
    memset(dialogue, 0, sizeof(*dialogue));
    dialogue->runtime = runtime;
    dialogue->view = view;
    dialogue->face = face;
    dialogue->cursor = 6;
    dialogue->box = runtime->box;
    if (view != nullptr && view->wipe != nullptr) {
        view->wipe(view->context);
    }
}

DIALOGUE_RESULT DialogueBox_Update(
    DIALOGUE *const dialogue, const TH02_INPUT *const sample)
{
    if (dialogue->phase == DIALOGUE_FINISHED) {
        return DIALOGUE_DONE;
    }
    if (dialogue->phase == DIALOGUE_FIRST_INPUT) {
        dialogue->phase = DIALOGUE_REVEAL;
        return DIALOGUE_INPUT;
    }
    if (dialogue->phase == DIALOGUE_REVEAL_FRAME) {
        if (dialogue->iterations % 3 == 0) {
            dialogue->cursor += 2;
        }
        return M_NextInput(dialogue);
    }
    ASSERT(sample != nullptr);
    if (dialogue->phase == DIALOGUE_CONFIRM_INPUT) {
        GameInput_BeginWait(&dialogue->wait, sample);
        dialogue->phase = DIALOGUE_CONFIRM_FRAMES;
        return DIALOGUE_FRAME;
    }
    if (dialogue->phase == DIALOGUE_CONFIRM_FRAMES) {
        if (GameInput_AdvanceWait(&dialogue->wait, sample)) {
            dialogue->runtime->box++;
            dialogue->phase = DIALOGUE_FINISHED;
            return DIALOGUE_DONE;
        }
        return DIALOGUE_FRAME;
    }
    ASSERT(dialogue->phase == DIALOGUE_REVEAL);
    dialogue->runtime->keys = GameInput_Sense(sample);
    const DIALOGUE_VIEW *const view = dialogue->view;
    if (view != nullptr && view->face != nullptr) {
        view->face(view->context, dialogue->face);
    }
    if (dialogue->cursor <= 72 && view != nullptr && view->text != nullptr) {
        const uint8_t line = dialogue->cursor <= 36 ? 0 : 1;
        view->text(
            view->context, dialogue->box, line, dialogue->cursor - line * 36);
    }
    dialogue->iterations++;
    if (dialogue->runtime->keys == 0) {
        dialogue->phase = DIALOGUE_REVEAL_FRAME;
        return DIALOGUE_FRAME;
    }
    dialogue->cursor += 2;
    return M_NextInput(dialogue);
}
