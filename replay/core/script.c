#include "script.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define M_MAX_SIZE (1024 * 1024)
#define M_LINE_SIZE 512

static char *M_Token(char **const cursor)
{
    char *start = *cursor;
    while (*start == ' ' || *start == '\t' || *start == '\r') {
        start++;
    }
    if (*start == '\0' || *start == '#') {
        *cursor = start;
        return nullptr;
    }
    char *end = start;
    while (*end != '\0' && *end != ' ' && *end != '\t' && *end != '\r'
           && *end != '#') {
        end++;
    }
    if (*end == '#') {
        *end = '\0';
        *cursor = end;
    } else if (*end != '\0') {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = end;
    }
    return start;
}

static RESULT M_Number(const char *const token, uint32_t *const out)
{
    FAIL_IF(
        token == nullptr || *token < '0' || *token > '9',
        "expected a frame number");
    errno = 0;
    char *end;
    const unsigned long number = strtoul(token, &end, 10);
    FAIL_IF(
        errno != 0 || *end != '\0' || number > INT32_MAX,
        "invalid frame number %s", token);
    *out = (uint32_t)number;
    return OK;
}

static RESULT M_Buttons(
    char *token, const SCRIPT_BINDING *const bindings,
    const size_t binding_count, uint16_t *const out)
{
    FAIL_IF(token == nullptr, "expected button names or none");
    *out = 0;
    if (strcmp(token, "none") == 0) {
        return OK;
    }
    while (token != nullptr) {
        char *const next = strchr(token, '+');
        if (next != nullptr) {
            *next = '\0';
        }
        bool found = false;
        for (size_t i = 0; i < binding_count; i++) {
            if (strcmp(token, bindings[i].name) == 0) {
                FAIL_IF(
                    (*out & bindings[i].word) != 0, "repeated button %s",
                    token);
                *out |= bindings[i].word;
                found = true;
                break;
            }
        }
        FAIL_IF(!found, "unknown button %s", token);
        token = next != nullptr ? next + 1 : nullptr;
    }
    return OK;
}

static RESULT M_Line(
    char *line, const SCRIPT_BINDING *const bindings,
    const size_t binding_count, bool *const header, INPUT_SCRIPT *const script)
{
    char *cursor = line;
    const char *const command = M_Token(&cursor);
    if (command == nullptr) {
        return OK;
    }
    char *const value = M_Token(&cursor);
    if (!*header) {
        FAIL_IF(
            strcmp(command, "OTINPUT") != 0 || value == nullptr
                || strcmp(value, "1") != 0,
            "expected OTINPUT 1");
        *header = true;
    } else {
        uint32_t frame = 0;
        MUST(M_Number(value, &frame));
        if (strcmp(command, "frames") == 0) {
            FAIL_IF(
                script->frames != 0 || frame == 0,
                "frames must appear once and be positive");
            script->frames = frame;
        } else if (strcmp(command, "input") == 0) {
            FAIL_IF(
                script->inputs.count != 0
                    && frame
                        <= script->inputs.items[script->inputs.count - 1].frame,
                "input frames must increase strictly");
            SCRIPT_INPUT input = { .frame = frame };
            MUST(M_Buttons(
                M_Token(&cursor), bindings, binding_count, &input.buttons));
            Array_Append(&script->inputs, input);
        } else if (strcmp(command, "fps") == 0) {
            FAIL_IF(
                script->fps.count != 0
                    && frame <= script->fps.items[script->fps.count - 1].frame,
                "FPS frames must increase strictly");
            const char *const token = M_Token(&cursor);
            FAIL_IF(token == nullptr, "expected FPS or none");
            SCRIPT_FPS observation = { .frame = frame };
            if (strcmp(token, "none") != 0) {
                errno = 0;
                char *end;
                observation.rate = strtof(token, &end);
                FAIL_IF(
                    errno != 0 || end == token || *end != '\0'
                        || !isfinite(observation.rate) || observation.rate < 0,
                    "invalid FPS %s", token);
                observation.available = true;
            } else {
                FAIL_IF(
                    script->fps.count != 0,
                    "none must be the first FPS observation");
            }
            Array_Append(&script->fps, observation);
        } else if (strcmp(command, "capture") == 0) {
            FAIL_IF(
                script->captures.count != 0
                    && frame
                        <= script->captures.items[script->captures.count - 1],
                "capture frames must increase strictly");
            Array_Append(&script->captures, frame);
        } else if (strcmp(command, "action") == 0) {
            FAIL_IF(
                script->actions.count != 0
                    && frame < script->actions.items[script->actions.count - 1]
                                   .frame,
                "action frames must not decrease");
            const char *const name = M_Token(&cursor);
            SCRIPT_ACTION action = { .frame = frame };
            FAIL_IF(
                name == nullptr || strlen(name) >= sizeof(action.name),
                "invalid action name");
            for (const char *c = name; *c != '\0'; c++) {
                FAIL_IF(
                    !((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9')
                      || *c == '-' || *c == '_'),
                    "invalid action name %s", name);
            }
            memcpy(action.name, name, strlen(name) + 1);
            MUST(M_Number(M_Token(&cursor), &action.value));
            Array_Append(&script->actions, action);
        } else {
            return FAIL("unknown command %s", command);
        }
    }
    FAIL_IF(M_Token(&cursor) != nullptr, "extra text after command");
    return OK;
}

static RESULT M_Read(
    READER *const in, const SCRIPT_BINDING *const bindings,
    const size_t binding_count, INPUT_SCRIPT *const script)
{
    FAIL_IF(Reader_Remaining(in) > M_MAX_SIZE, "input script exceeds 1 MiB");
    bool header = false;
    uint32_t line_number = 0;
    while (Reader_Remaining(in) != 0 && !Reader_Failed(in)) {
        char line[M_LINE_SIZE];
        size_t length = 0;
        while (Reader_Remaining(in) != 0) {
            const uint8_t c = Reader_ReadU8(in);
            if (c == '\n') {
                break;
            }
            FAIL_IF(
                c == 0 || length == sizeof(line) - 1,
                "invalid or overlong script line %u", line_number + 1);
            line[length++] = (char)c;
        }
        line[length] = '\0';
        line_number++;
        MUST(
            M_Line(line, bindings, binding_count, &header, script), "line %u",
            line_number);
    }
    MUST(Reader_Result(in));
    FAIL_IF(
        !header || script->frames == 0,
        "missing script header or frames command");
    FAIL_IF(
        script->inputs.count != 0
            && script->inputs.items[script->inputs.count - 1].frame
                >= script->frames,
        "input is outside the script");
    FAIL_IF(
        script->captures.count != 0
            && script->captures.items[script->captures.count - 1]
                >= script->frames,
        "capture is outside the script");
    FAIL_IF(
        script->actions.count != 0
            && script->actions.items[script->actions.count - 1].frame
                >= script->frames,
        "action is outside the script");
    FAIL_IF(
        script->fps.count != 0
            && script->fps.items[script->fps.count - 1].frame >= script->frames,
        "FPS observation is outside the script");
    return OK;
}

RESULT Script_Read(
    READER *const in, const SCRIPT_BINDING *const bindings,
    const size_t binding_count, INPUT_SCRIPT *const out)
{
    INPUT_SCRIPT script = {};
    Array_Init(&script.inputs, &g_HeapAllocator);
    Array_Init(&script.captures, &g_HeapAllocator);
    Array_Init(&script.actions, &g_HeapAllocator);
    Array_Init(&script.fps, &g_HeapAllocator);
    const RESULT result = M_Read(in, bindings, binding_count, &script);
    if (!IS_OK(result)) {
        Script_Free(&script);
        return result;
    }
    *out = script;
    return OK;
}

void Script_Free(INPUT_SCRIPT *const script)
{
    Array_Free(&script->inputs);
    Array_Free(&script->captures);
    Array_Free(&script->actions);
    Array_Free(&script->fps);
    memset(script, 0, sizeof(*script));
}

uint16_t Script_Buttons(const INPUT_SCRIPT *const script, const uint32_t frame)
{
    if (frame >= script->frames) {
        return 0;
    }
    int32_t low = 0;
    int32_t high = script->inputs.count;
    while (low < high) {
        const int32_t middle = low + (high - low) / 2;
        if (script->inputs.items[middle].frame <= frame) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low == 0 ? 0 : script->inputs.items[low - 1].buttons;
}

bool Script_Capture(const INPUT_SCRIPT *const script, const uint32_t frame)
{
    int32_t low = 0;
    int32_t high = script->captures.count;
    while (low < high) {
        const int32_t middle = low + (high - low) / 2;
        if (script->captures.items[middle] < frame) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low < script->captures.count && script->captures.items[low] == frame;
}

const SCRIPT_FPS *Script_Fps(
    const INPUT_SCRIPT *const script, const uint32_t frame)
{
    if (frame >= script->frames) {
        return nullptr;
    }
    int32_t low = 0;
    int32_t high = script->fps.count;
    while (low < high) {
        const int32_t middle = low + (high - low) / 2;
        if (script->fps.items[middle].frame <= frame) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low == 0 ? nullptr : &script->fps.items[low - 1];
}
