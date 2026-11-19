#include "th07/screens/ending/script.h"

#include "th07/files.h"

#include "shell/game.h"
#include "core/vfs.h"
#include "core/base/arena.h"
#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/json.h"

#include "stg/end_diff.h"

#include <string.h>

#define M_DIFF_ARENA_CAPACITY (16 * 1024 * 1024)

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

static bool M_StartsLine(const ENDING_SCRIPT *const script, const size_t at)
{
    return at == 0 || script->data[at - 1] == '\0'
        || M_IsBreak(script->data[at - 1]);
}

static void M_ConvertLines(ENDING_SCRIPT *const script)
{
    script->lines = Mem_AllocArrZ(script->size + 1, sizeof(char *));
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
        script->lines[start] = M_Decode(&script->data[start], end - start);
    }
}

RESULT EndingScript_Load(const char *const path, ENDING_SCRIPT *const out)
{
    *out = (ENDING_SCRIPT) {};
    READER reader;
    MUST(
        Vfs_Open(Shell_GetFiles(), path, VFS_IN_MEMORY, &reader),
        "the ending cannot be read");
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
                EndDiff_Apply(data, size, diff, &patched_size);
            EndingScript_Init(patched, patched_size, out);
            Mem_Free(patched);
        } else {
            EndingScript_Init(data, size, out);
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
    const void *const data, const size_t size, ENDING_SCRIPT *const out)
{
    *out = (ENDING_SCRIPT) {
        .data = Mem_Dup(data, size),
        .size = size,
    };
    M_ConvertLines(out);
}

void EndingScript_Free(ENDING_SCRIPT *const script)
{
    if (script->lines != nullptr) {
        for (size_t i = 0; i < script->size; i++) {
            Mem_Free(script->lines[i]);
        }
    }
    Mem_Free(script->lines);
    Mem_Free(script->data);
    *script = (ENDING_SCRIPT) {};
}

uint8_t EndingScript_Byte(
    const ENDING_SCRIPT *const script, const size_t offset)
{
    return offset < script->size ? script->data[offset] : 0;
}

const char *EndingScript_GetLine(
    const ENDING_SCRIPT *const script, const size_t offset)
{
    return offset < script->size ? script->lines[offset] : nullptr;
}
