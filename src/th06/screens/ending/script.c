#include "th06/screens/ending/script.h"

#include "th06/files.h"

#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"

#include <string.h>

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
    if (Result_Ignore(
            Cp932_Decode((const char *)bytes, M_Length(bytes, size), &utf8))) {
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
        EndingScript_Init(data, size, out);
    }
    if (data != nullptr) {
        Allocator_Free(&g_HeapAllocator, data, size);
    }
    MUST(result, "%s", path);
    return OK;
}

void EndingScript_Init(
    const void *const data, const size_t size, ENDING_SCRIPT *const out)
{
    *out = (ENDING_SCRIPT) { .data = Mem_Dup(data, size), .size = size };
    M_ConvertHalves(out);
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
