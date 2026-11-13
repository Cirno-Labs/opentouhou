#include "th06/screens/ending/script.h"

#include "th06/files.h"

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/json.h"

#include <stdio.h>

#include <string.h>

#define M_DIFF_ARENA_CAPACITY (16 * 1024 * 1024)

typedef ARRAY(uint8_t) M_BYTES;

static bool M_IsBreak(const uint8_t c)
{
    return c == '\n' || c == '\r';
}

// Returns the number of bytes before the first NUL, at most size.
static size_t M_Length(const uint8_t *const bytes, const size_t size)
{
    size_t length = 0;
    while (length < size && bytes[length] != 0) {
        length++;
    }
    return length;
}

static char *M_Decode(const uint8_t *const bytes, const size_t size)
{
    char *utf8;
    if (Result_Ignore(Cp932_DecodeText(
            (const char *)bytes, M_Length(bytes, size), &utf8))) {
        return utf8;
    }
    LOG_WARNING("Dropped an ending text that does not convert");
    return Mem_AllocZ(1);
}

static void M_ConvertHalves(ENDING_SCRIPT *const script)
{
    script->halves = Mem_AllocArrZ(script->size + 1, sizeof(char *));
    for (size_t start = 0; start < script->size; start++) {
        if (start != 0 && script->data[start - 1] != '\0'
            && !M_IsBreak(script->data[start - 1])) {
            continue;
        }
        size_t half = start;
        while (half < script->size && script->halves[half] == nullptr) {
            const size_t end = EndingScript_HalfEnd(script, half);
            if (end == half) {
                break;
            }
            uint8_t bytes[ENDING_HALF_BYTES + 2] = {};
            for (size_t i = half; i < end; i++) {
                bytes[i - half] = EndingScript_Byte(script, i);
            }
            script->halves[half] = M_Decode(bytes, end - half);
            if (end - half < ENDING_HALF_BYTES) {
                break;
            }
            half = end;
        }
    }
}

static bool M_StartsLine(const ENDING_SCRIPT *const script, const size_t at)
{
    return at == 0 || script->data[at - 1] == '\0'
        || M_IsBreak(script->data[at - 1]);
}

static void M_ConvertLines(ENDING_SCRIPT *const script)
{
    script->halves = Mem_AllocArrZ(script->size + 1, sizeof(char *));
    for (size_t start = 0; start < script->size; start++) {
        const uint8_t c = script->data[start];
        if (!M_StartsLine(script, start) || c == '@' || c == '\0'
            || M_IsBreak(c)) {
            continue;
        }
        size_t end = start;
        while (end < script->size && script->data[end] != '\0'
               && !M_IsBreak(script->data[end])) {
            end++;
        }
        script->halves[start] = M_Decode(&script->data[start], end - start);
    }
}

// Returns the end of the line at start: the byte after its '\n', or the
// end of data.
static size_t M_LineEnd(
    const uint8_t *const data, const size_t size, const size_t start)
{
    const uint8_t *const line_break = memchr(data + start, '\n', size - start);
    return line_break != nullptr ? (size_t)(line_break - data) + 1 : size;
}

static void M_AppendLines(M_BYTES *const out, const JSON_VALUE *const lines)
{
    for (int32_t i = 0; i < lines->array.count; i++) {
        const char *const line = Json_GetString(Json_At(lines, i), nullptr);
        if (line == nullptr) {
            continue;
        }
        Array_AppendSlice(out, (const uint8_t *)line, (int32_t)strlen(line));
        Array_Append(out, '\r');
        Array_Append(out, '\n');
    }
}

RESULT EndingScript_Load(const char *const path, ENDING_SCRIPT *const out)
{
    *out = (ENDING_SCRIPT) {};
    READER reader;
    MUST(Files_Load(path, &reader), "the ending cannot be read");
    const size_t size = Reader_Remaining(&reader);
    uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    if (IS_OK(result)) {
        ARENA arena;
        Arena_Init(&arena, "ending jdiff", M_DIFF_ARENA_CAPACITY);
        const JSON_VALUE *const diff =
            Files_LoadDiff(path, Arena_Allocator(&arena));
        if (diff != nullptr) {
            size_t patched_size;
            uint8_t *const patched =
                EndingScript_ApplyDiff(data, size, diff, &patched_size);
            EndingScript_Init(patched, patched_size, true, out);
            Mem_Free(patched);
        } else {
            EndingScript_Init(data, size, Files_IsPatched(path), out);
        }
        Arena_Free(&arena);
    }
    if (data != nullptr) {
        Allocator_Free(&g_HeapAllocator, data, size);
    }
    MUST(result, "%s", path);
    return OK;
}

void EndingScript_Init(
    const void *const data, const size_t size, const bool whole_lines,
    ENDING_SCRIPT *const out)
{
    *out = (ENDING_SCRIPT) {
        .data = Mem_Dup(data, size),
        .size = size,
        .whole_lines = whole_lines,
    };
    if (whole_lines) {
        M_ConvertLines(out);
    } else {
        M_ConvertHalves(out);
    }
}

uint8_t *EndingScript_ApplyDiff(
    const uint8_t *const data, const size_t size, const JSON_VALUE *const diff,
    size_t *const out_size)
{
    M_BYTES out;
    Array_Init(&out, &g_HeapAllocator);
    int32_t line = 0;
    size_t at = 0;
    while (at < size) {
        if (data[at] == '@') {
            const size_t end = M_LineEnd(data, size, at);
            Array_AppendSlice(&out, &data[at], (int32_t)(end - at));
            at = end;
            line++;
            continue;
        }
        char key[16];
        snprintf(key, sizeof(key), "%d", line);
        const JSON_VALUE *const lines = Json_Get(Json_Get(diff, key), "lines");
        if (lines != nullptr && lines->type == JSON_ARRAY) {
            M_AppendLines(&out, lines);
        }
        while (at < size && data[at] != '@') {
            const size_t end = M_LineEnd(data, size, at);
            if (lines == nullptr || lines->type != JSON_ARRAY) {
                Array_AppendSlice(&out, &data[at], (int32_t)(end - at));
            }
            at = end;
            line++;
        }
    }
    *out_size = (size_t)out.count;
    uint8_t *const result =
        Mem_Dup(out.count > 0 ? out.items : data, *out_size);
    Array_Free(&out);
    return result;
}

void EndingScript_Free(ENDING_SCRIPT *const script)
{
    if (script->halves != nullptr) {
        for (size_t i = 0; i < script->size; i++) {
            Mem_Free(script->halves[i]);
        }
    }
    Mem_Free(script->halves);
    Mem_Free(script->data);
    *script = (ENDING_SCRIPT) {};
}

uint8_t EndingScript_Byte(
    const ENDING_SCRIPT *const script, const size_t offset)
{
    return offset < script->size ? script->data[offset] : 0;
}

size_t EndingScript_HalfEnd(
    const ENDING_SCRIPT *const script, const size_t start)
{
    size_t end = start;
    while (end < script->size && end - start < ENDING_HALF_BYTES) {
        const uint8_t c = script->data[end];
        if (c == '@' || c == '\0' || M_IsBreak(c)) {
            break;
        }
        end += 2;
    }
    return end;
}

const char *EndingScript_GetHalf(
    const ENDING_SCRIPT *const script, const size_t offset)
{
    return offset < script->size ? script->halves[offset] : nullptr;
}
