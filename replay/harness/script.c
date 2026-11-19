#include "script.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M_MAX_FIELDS 3
#define M_MAX_KEYS 32

// Splits text in place at runs of the characters in seps into up to
// capacity fields. Returns their number, or capacity + 1 when there are
// more.
static size_t M_Split(
    char *const text, const char *const seps, char **const fields,
    const size_t capacity)
{
    size_t count = 0;
    for (char *c = text + strspn(text, seps); *c != '\0';
         c += strspn(c, seps)) {
        if (count == capacity) {
            return capacity + 1;
        }
        fields[count++] = c;
        c += strcspn(c, seps);
        if (*c != '\0') {
            *c++ = '\0';
        }
    }
    return count;
}

static RESULT M_ReadKeys(
    const HARNESS_GAME *const game, char *const text, uint32_t *const out)
{
    *out = 0;
    if (strcmp(text, "none") == 0) {
        return OK;
    }
    char *names[M_MAX_KEYS];
    const size_t count = M_Split(text, "+", names, M_MAX_KEYS);
    FAIL_IF(count > M_MAX_KEYS, "too many keys");
    for (size_t i = 0; i < count; i++) {
        const BINDING *const binding = Game_FindBinding(game, names[i]);
        FAIL_IF(binding == nullptr, "unknown key %s", names[i]);
        *out |= binding->mask;
    }
    return OK;
}

static RESULT M_ReadFps(const char *const text, FPS *const out)
{
    if (strcmp(text, "none") == 0) {
        *out = (FPS) {};
        return OK;
    }
    char *end;
    const float rate = strtof(text, &end);
    FAIL_IF(*end != '\0' || !isfinite(rate) || rate < 0, "bad rate %s", text);
    *out = (FPS) { .available = true, .rate = rate };
    return OK;
}

static RESULT M_ReadLine(
    const HARNESS_GAME *const game, char *const line, SCRIPT *const script)
{
    char *fields[M_MAX_FIELDS];
    const size_t count = M_Split(line, " \t\r", fields, M_MAX_FIELDS);
    if (count == 0) {
        return OK;
    }
    uint32_t frame;
    if (strcmp(fields[0], "frames") == 0) {
        FAIL_IF(count != 2 || !Str_ParseU32(fields[1], &frame), "bad frames");
        script->frames = frame;
        return OK;
    }
    FAIL_IF(count != 3, "expected KIND FRAME VALUE");
    FAIL_IF(!Str_ParseU32(fields[1], &frame), "bad frame %s", fields[1]);
    if (strcmp(fields[0], "input") == 0) {
        FAIL_IF(
            script->inputs.count > 0
                && script->inputs.items[script->inputs.count - 1].frame
                    >= frame,
            "input frames must increase");
        SCRIPT_INPUT input = { .frame = frame };
        MUST(M_ReadKeys(game, fields[2], &input.buttons));
        Array_Append(&script->inputs, input);
        return OK;
    }
    if (strcmp(fields[0], "fps") == 0) {
        FAIL_IF(
            script->fps.count > 0
                && script->fps.items[script->fps.count - 1].frame >= frame,
            "fps frames must increase");
        SCRIPT_FPS fps = { .frame = frame };
        MUST(M_ReadFps(fields[2], &fps.fps));
        Array_Append(&script->fps, fps);
        return OK;
    }
    return FAIL("unknown line kind %s", fields[0]);
}

char *Script_NextLine(char **const text)
{
    char *const line = *text;
    if (line == nullptr) {
        return nullptr;
    }
    char *const end = strchr(line, '\n');
    if (end != nullptr) {
        *end = '\0';
    }
    *text = end != nullptr ? end + 1 : nullptr;
    line[strcspn(line, "#")] = '\0';
    return line;
}

RESULT Script_Read(
    READER *const in, const HARNESS_GAME *const game, SCRIPT *const out)
{
    size_t size;
    char *const text = Reader_ReadCString(in, &g_HeapAllocator, &size);
    if (text == nullptr) {
        return Reader_Result(in);
    }
    SCRIPT script = {};
    Array_Init(&script.inputs, &g_HeapAllocator);
    Array_Init(&script.fps, &g_HeapAllocator);
    RESULT result = OK;
    char *rest = text;
    char *line;
    for (int32_t number = 1;
         IS_OK(result) && (line = Script_NextLine(&rest)) != nullptr;
         number++) {
        result = Result_Prefix(
            M_ReadLine(game, line, &script), "%s:%d", Reader_GetName(in),
            number);
    }
    Allocator_Free(&g_HeapAllocator, text, size + 1);
    if (!IS_OK(result)) {
        Script_Free(&script);
        return result;
    }
    *out = script;
    return OK;
}

static void M_Write(WRITER *const out, const char *const text)
{
    Writer_WriteBytes(out, text, strlen(text));
}

static void M_WriteKeys(
    WRITER *const out, const HARNESS_GAME *const game, const uint32_t buttons)
{
    if (buttons == 0) {
        M_Write(out, "none");
        return;
    }
    const char *sep = "";
    for (size_t i = 0; i < game->binding_count; i++) {
        if ((buttons & game->bindings[i].mask) != 0) {
            M_Write(out, sep);
            M_Write(out, game->bindings[i].name);
            sep = "+";
        }
    }
}

void Script_Write(
    WRITER *const out, const HARNESS_GAME *const game,
    const SCRIPT *const script)
{
    char line[64];
    if (script->frames != 0) {
        snprintf(line, sizeof(line), "frames %" PRIu32 "\n", script->frames);
        M_Write(out, line);
    }
    for (int32_t i = 0; i < script->inputs.count; i++) {
        const SCRIPT_INPUT *const input = &script->inputs.items[i];
        snprintf(line, sizeof(line), "input %" PRIu32 " ", input->frame);
        M_Write(out, line);
        M_WriteKeys(out, game, input->buttons);
        M_Write(out, "\n");
    }
    for (int32_t i = 0; i < script->fps.count; i++) {
        const SCRIPT_FPS *const fps = &script->fps.items[i];
        if (fps->fps.available) {
            snprintf(
                line, sizeof(line), "fps %" PRIu32 " %.9g\n", fps->frame,
                (double)fps->fps.rate);
        } else {
            snprintf(line, sizeof(line), "fps %" PRIu32 " none\n", fps->frame);
        }
        M_Write(out, line);
    }
}

void Script_Free(SCRIPT *const script)
{
    Array_Free(&script->inputs);
    Array_Free(&script->fps);
    script->frames = 0;
}

// Returns the index of the last of count entries of size bytes at items
// whose leading u32 frame is at most frame, or -1.
static int32_t M_Find(
    const void *const items, const int32_t count, const size_t size,
    const uint32_t frame)
{
    int32_t low = 0;
    int32_t high = count;
    while (low < high) {
        const int32_t middle = low + (high - low) / 2;
        uint32_t at;
        memcpy(&at, (const char *)items + (size_t)middle * size, sizeof(at));
        if (at <= frame) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low - 1;
}

uint32_t Script_Buttons(const SCRIPT *const script, const uint32_t frame)
{
    const int32_t i = M_Find(
        script->inputs.items, script->inputs.count, sizeof(SCRIPT_INPUT),
        frame);
    return i < 0 ? 0 : script->inputs.items[i].buttons;
}

FPS Script_Fps(const SCRIPT *const script, const uint32_t frame)
{
    const int32_t i =
        M_Find(script->fps.items, script->fps.count, sizeof(SCRIPT_FPS), frame);
    return i < 0 ? (FPS) {} : script->fps.items[i].fps;
}

void Script_RecordButtons(
    SCRIPT *const script, const uint32_t frame, const uint32_t buttons)
{
    if (script->inputs.count > 0
        && script->inputs.items[script->inputs.count - 1].frame == frame) {
        script->inputs.count--;
    }
    if (Script_Buttons(script, frame) != buttons) {
        Array_Append(
            &script->inputs,
            ((SCRIPT_INPUT) { .frame = frame, .buttons = buttons }));
    }
}

void Script_RecordFps(SCRIPT *const script, const uint32_t frame, const FPS fps)
{
    if (script->fps.count > 0
        && script->fps.items[script->fps.count - 1].frame == frame) {
        script->fps.count--;
    }
    const FPS shown = Script_Fps(script, frame);
    if (shown.available != fps.available
        || (fps.available && shown.rate != fps.rate)) {
        Array_Append(
            &script->fps, ((SCRIPT_FPS) { .frame = frame, .fps = fps }));
    }
}
