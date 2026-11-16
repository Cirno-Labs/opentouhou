#include "stg/msg_diff.h"

#include "core/base/array.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/json.h"

#include <stdio.h>
#include <string.h>

typedef ARRAY(MSG_INSTR) M_INSTR_ARRAY;

// Holds the state of applying the jdiff of one script, as thcrap's MSG
// patcher keeps it. A box is the lines from a line 0 to the next line 0.
typedef struct {
    const JSON_VALUE *entry;
    M_INSTR_ARRAY out;
    int32_t time;
    int32_t box; // counts the line instructions of a time without a diff
    const JSON_VALUE *lines; // the lines of the box, or nullptr
    int32_t line;
    int32_t last_opcode; // the opcode of the last replaced line, or -1
    int32_t last_line; // the position in out of the last replaced line
    // Holds the lines of the box whose replaced line had a translator note.
    // Holds entry until the first replaced line, and nullptr after a line
    // that hides the note.
    const JSON_VALUE *noted;
} M_PATCH_STATE;

static void M_SetText(MSG_INSTR *const instr, const char *const text)
{
    Mem_Free(instr->text);
    instr->text = Str_Dup(text);
}

// Gives instr a replaced line of the box. As thcrap's MSG patcher marks
// the lines, the line hides the shown translator note when it has no note
// and it is the first replaced line of the script, or the first one of
// another box after a line with a note.
static void M_TakeLine(
    M_PATCH_STATE *const state, MSG_INSTR *const instr, const char *const text)
{
    M_SetText(instr, text);
    instr->removes_note = false;
    if (strchr(text, '\x14') != nullptr) {
        state->noted = state->lines;
    } else if (state->noted != nullptr && state->noted != state->lines) {
        instr->removes_note = true;
        state->noted = nullptr;
    }
}

// Adds the lines of the box that its instructions did not take, after the
// last replaced line, as copies of it.
static void M_EndBox(M_PATCH_STATE *const state, const int32_t line_count)
{
    if (state->last_opcode >= 0 && state->lines != nullptr) {
        const int32_t count = state->lines->array.count;
        for (; state->line < count; state->line++) {
            const char *const text =
                Json_GetString(Json_At(state->lines, state->line), nullptr);
            if (text == nullptr) {
                continue;
            }
            if (state->line >= line_count) {
                LOG_WARNING(
                    "Dropped the dialogue line %d at time %d: a box has %d "
                    "lines",
                    state->line, state->time, line_count);
                continue;
            }
            MSG_INSTR instr = state->out.items[state->last_line];
            instr.index = (int16_t)state->line;
            instr.text = nullptr;
            M_TakeLine(state, &instr, text);
            state->last_line++;
            Array_Insert(&state->out, state->last_line, instr);
        }
    }
    state->line = 0;
    state->lines = nullptr;
}

// Applies the diff to the line instruction instr, which is the last of
// out.
static void M_PatchLine(
    M_PATCH_STATE *const state, MSG_INSTR *const instr,
    const MSG_DIFF_FORMAT format)
{
    if (state->lines == nullptr) {
        state->box++;
        char key[48];
        if (instr->opcode == format.nameplate) {
            snprintf(key, sizeof(key), "%d_h1_%d", state->time, state->box);
        } else {
            snprintf(key, sizeof(key), "%d_%d", state->time, state->box);
        }
        const JSON_VALUE *lines = Json_Get(state->entry, key);
        if (lines != nullptr && lines->type == JSON_OBJECT) {
            lines = Json_Get(lines, "lines");
        }
        if (lines != nullptr && lines->type == JSON_ARRAY) {
            state->lines = lines;
        }
    }
    if (state->lines == nullptr) {
        return;
    }
    const char *const text =
        Json_GetString(Json_At(state->lines, state->line), nullptr);
    state->line++;
    if (text == nullptr) {
        Mem_Free(instr->text);
        state->out.count--;
        return;
    }
    M_TakeLine(state, instr, text);
    state->last_opcode = instr->opcode;
    state->last_line = state->out.count - 1;
}

// Returns whether the box counter starts again for a line 0 of opcode
// after a line of last_opcode. thcrap keeps counting only between two
// lines of the same kind.
static bool M_RestartsBoxes(
    const int32_t last_opcode, const uint8_t opcode,
    const MSG_DIFF_FORMAT format)
{
    return last_opcode == format.nameplate || opcode == format.nameplate;
}

// Applies the jdiff entry of a script: each box of a time code takes its
// lines from "<time>_<box>", or from "<time>_h1_<box>" for nameplates.
static void M_PatchScript(
    MSG_SCRIPT *const script, const JSON_VALUE *const entry,
    const MSG_DIFF_FORMAT format)
{
    M_PATCH_STATE state = {
        .entry = entry,
        .time = -1,
        .box = -1,
        .last_opcode = -1,
        .noted = entry,
    };
    Array_Init(&state.out, &g_HeapAllocator);
    for (int32_t i = 0; i < script->count; i++) {
        MSG_INSTR instr = script->instrs[i];
        if (format.is_final(instr.opcode)) {
            M_EndBox(&state, format.line_count);
        }
        if (instr.time != state.time) {
            state.time = instr.time;
            state.box = -1;
        }
        Array_Append(&state.out, instr);
        if (instr.opcode != format.line && instr.opcode != format.nameplate) {
            continue;
        }
        if (instr.index == 0) {
            M_EndBox(&state, format.line_count);
            if (state.last_opcode >= 0
                && M_RestartsBoxes(state.last_opcode, instr.opcode, format)) {
                state.box = -1;
            }
        }
        state.line = instr.index;
        MSG_INSTR *const last = &state.out.items[state.out.count - 1];
        M_PatchLine(&state, last, format);
    }
    Mem_Free(script->instrs);
    script->instrs = Mem_AllocArr((size_t)state.out.count, sizeof(MSG_INSTR));
    memcpy(
        script->instrs, state.out.items,
        (size_t)state.out.count * sizeof(MSG_INSTR));
    script->count = state.out.count;
    Array_Free(&state.out);
}

void MsgDiff_Apply(
    MSG_SCRIPT *const scripts, const int32_t count,
    const JSON_VALUE *const diff, const MSG_DIFF_FORMAT format)
{
    for (int32_t i = 0; diff != nullptr && i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        const JSON_VALUE *const entry = Json_Get(diff, key);
        if (entry != nullptr && entry->type == JSON_OBJECT) {
            M_PatchScript(&scripts[i], entry, format);
        }
    }
}
