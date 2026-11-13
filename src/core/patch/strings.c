// SPDX-License-Identifier: GPL-3.0-only

#include "core/patch/strings.h"

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/hash_map.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/json.h"
#include "core/patch/files.h"

#include <stdlib.h>
#include <string.h>

// Sets how much address space the tables reserve for their JSON trees.
#define M_ARENA_CAPACITY (64 * 1024 * 1024)
#define M_TL_NOTE_SEPARATOR '\x14'

typedef HASH_MAP(uint32_t, const char *) M_LOCATIONS;
typedef ARRAY(char) M_TEXT;

struct PATCH_STRINGS {
    ARENA arena;
    JSON_INDEX definitions;
    M_LOCATIONS locations;
};

// Holds what a conversion takes from the arguments.
typedef struct {
    char length[4];
    char conversion;
    int32_t stars;
} M_CONVERSION;

static bool M_ParseHex(const char *const text, uint32_t *const out)
{
    if (text[0] == '\0') {
        return false;
    }
    char *end;
    const unsigned long value = strtoul(text, &end, 16);
    if (*end != '\0' || value > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

static bool M_ParseAddress(
    const char *const key, const uint32_t image_base, uint32_t *const out)
{
    if ((key[0] == 'R' || key[0] == 'r') && (key[1] == 'x' || key[1] == 'X')) {
        uint32_t offset;
        if (!M_ParseHex(key + 2, &offset)) {
            return false;
        }
        *out = image_base + offset;
        return true;
    }
    if (key[0] == '0' && (key[1] == 'x' || key[1] == 'X')) {
        return M_ParseHex(key + 2, out);
    }
    return false;
}

static void M_ReadLocations(
    PATCH_STRINGS *const strings, const JSON_VALUE *const locations,
    const uint32_t image_base)
{
    if (locations == nullptr || locations->type != JSON_OBJECT) {
        return;
    }
    for (int32_t i = 0; i < locations->object.count; i++) {
        const JSON_MEMBER *const member = &locations->object.members[i];
        const char *const id = Json_GetString(member->value, nullptr);
        uint32_t address;
        if (id == nullptr
            || !M_ParseAddress(member->key, image_base, &address)) {
            LOG_WARNING("stringlocs.js: left out the entry %s", member->key);
            continue;
        }
        HashMap_Put(&strings->locations, address, id);
    }
}

static const char *M_Check(
    const char *const id, const char *const text, const char *const original)
{
    if (text == nullptr) {
        return original;
    }
    if (!Patch_SameConversions(original, text)) {
        LOG_WARNING(
            "Rejected the translation of %s: its conversions differ from "
            "\"%s\"",
            id, original);
        return original;
    }
    return text;
}

// Reads the conversion after the '%' at *p into *out and moves *p past it.
// Returns false for a malformed conversion.
static bool M_ReadConversion(const char **const p, M_CONVERSION *const out)
{
    const char *c = *p;
    *out = (M_CONVERSION) {};
    while (*c != '\0' && strchr("-+ #0'", *c) != nullptr) {
        c++;
    }
    if (*c == '*') {
        out->stars++;
        c++;
    } else {
        while (*c >= '0' && *c <= '9') {
            c++;
        }
    }
    if (*c == '.') {
        c++;
        if (*c == '*') {
            out->stars++;
            c++;
        } else {
            while (*c >= '0' && *c <= '9') {
                c++;
            }
        }
    }
    size_t length = 0;
    while (*c != '\0' && strchr("hlLqjzt", *c) != nullptr
           && length < sizeof(out->length) - 1) {
        out->length[length++] = *c++;
    }
    if (*c == '\0' || strchr("diouxXeEfFgGaAcspn", *c) == nullptr) {
        return false;
    }
    out->conversion = *c++;
    *p = c;
    return true;
}

// Moves *p to the next conversion and reads it into *out. Returns 1 for a
// conversion, 0 at the end of the format and -1 for a malformed conversion.
static int M_NextConversion(const char **const p, M_CONVERSION *const out)
{
    for (;;) {
        const char *const percent = strchr(*p, '%');
        if (percent == nullptr) {
            return 0;
        }
        if (percent[1] == '%') {
            *p = percent + 2;
            continue;
        }
        *p = percent + 1;
        return M_ReadConversion(p, out) ? 1 : -1;
    }
}

static void M_AppendPlain(M_TEXT *const out, const char *text, size_t size);

// Returns the size of the markup at text, which starts with '<', and
// appends its plain text to out. Returns 0, and appends nothing, when text
// does not start a markup with at least two parameters and a closing '>'.
static size_t M_MatchMarkup(
    const char *const text, const size_t size, M_TEXT *const out)
{
    int32_t depth = 0;
    int32_t params = 0;
    const char *start = text + 1;
    const char *second = nullptr;
    size_t second_size = 0;
    bool skip = false;
    for (size_t i = 1; i < size; i++) {
        const char c = text[i];
        depth += c == '<';
        depth -= c == '>';
        if ((depth == 0 && c == '$') || depth == -1) {
            if (params == 0) {
                skip =
                    memchr(start, 's', (size_t)(text + i - start)) != nullptr;
            } else if (params == 1) {
                second = start;
                second_size = (size_t)(text + i - start);
            }
            params++;
            start = text + i + 1;
            if (depth == -1) {
                if (params < 2) {
                    return 0;
                }
                if (!skip) {
                    M_AppendPlain(out, second, second_size);
                }
                return i + 1;
            }
        }
    }
    return 0;
}

static void M_AppendPlain(
    M_TEXT *const out, const char *const text, const size_t size)
{
    size_t i = 0;
    while (i < size && text[i] != M_TL_NOTE_SEPARATOR) {
        if (text[i] == '<') {
            const size_t matched = M_MatchMarkup(text + i, size - i, out);
            if (matched > 0) {
                i += matched;
                continue;
            }
        }
        Array_Append(out, text[i]);
        i++;
    }
}

PATCH_STRINGS *Patch_LoadStrings(
    const PATCH_STACK *const stack, const uint32_t image_base)
{
    PATCH_STRINGS *const strings = Mem_AllocZ(sizeof(*strings));
    Arena_Init(&strings->arena, "patch strings", M_ARENA_CAPACITY);
    HashMap_Init(&strings->locations, &g_HeapAllocator);
    const ALLOCATOR *const allocator = Arena_Allocator(&strings->arena);
    Json_BuildIndex(
        Patch_LoadJson(stack, PATCH_ROOT, "stringdefs.js", allocator),
        &g_HeapAllocator, &strings->definitions);
    M_ReadLocations(
        strings,
        Patch_LoadJson(stack, PATCH_GAME_DIR, "stringlocs.js", allocator),
        image_base);
    return strings;
}

void Patch_FreeStrings(PATCH_STRINGS *const strings)
{
    if (strings == nullptr) {
        return;
    }
    HashMap_Free(&strings->definitions);
    HashMap_Free(&strings->locations);
    Arena_Free(&strings->arena);
    Mem_Free(strings);
}

const char *Patch_TranslateAt(
    const PATCH_STRINGS *const strings, const uint32_t address,
    const char *const original)
{
    if (strings == nullptr) {
        return original;
    }
    const char *const *const id = HashMap_Get(&strings->locations, address);
    return id != nullptr ? Patch_Translate(strings, *id, original) : original;
}

const char *Patch_Translate(
    const PATCH_STRINGS *const strings, const char *const id,
    const char *const original)
{
    if (strings == nullptr) {
        return original;
    }
    return M_Check(
        id, Json_GetString(Json_IndexGet(&strings->definitions, id), nullptr),
        original);
}

bool Patch_SameConversions(const char *const a, const char *const b)
{
    const char *p = a;
    const char *q = b;
    for (;;) {
        M_CONVERSION first;
        M_CONVERSION second;
        const int a_next = M_NextConversion(&p, &first);
        const int b_next = M_NextConversion(&q, &second);
        if (a_next < 0 || b_next < 0) {
            return strcmp(a, b) == 0;
        }
        if (a_next != b_next) {
            return false;
        }
        if (a_next == 0) {
            return true;
        }
        if (first.conversion != second.conversion || first.stars != second.stars
            || strcmp(first.length, second.length) != 0) {
            return false;
        }
    }
}

char *Patch_PlainText(const char *const text)
{
    M_TEXT out;
    Array_Init(&out, &g_HeapAllocator);
    M_AppendPlain(&out, text, strlen(text));
    Array_Append(&out, '\0');
    char *const result = Str_Dup(out.items);
    Array_Free(&out);
    return result;
}
