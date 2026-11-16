#include "stg/end_diff.h"

#include "core/base/array.h"
#include "core/base/memory.h"
#include "core/json.h"

#include <stdio.h>
#include <string.h>

typedef ARRAY(uint8_t) M_BYTES;

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

uint8_t *EndDiff_Apply(
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
