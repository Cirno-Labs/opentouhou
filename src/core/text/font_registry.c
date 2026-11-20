#include "core/text/font_registry.h"

#include "core/base/array.h"
#include "core/base/cp932.h"
#include "core/base/hash_map.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/reader.h"
#include "core/text/private.h"

#include <string.h>

#define M_NAME_FAMILY 1
#define M_NAME_SUBFAMILY 2
#define M_NAME_FULL 4
#define M_NAME_TYPOGRAPHIC_FAMILY 16
#define M_FS_SELECTION_REGULAR 0x40

typedef struct {
    char *source;
    uint8_t *data;
    size_t size;
} M_FILE;

typedef struct {
    int32_t file;
    int32_t face;
} M_FACE;

typedef struct {
    char *key;
    int32_t face;
} M_NAME;

typedef ARRAY(M_FILE) M_FILE_ARRAY;
typedef ARRAY(M_NAME) M_NAME_ARRAY;
typedef HASH_MAP(char *, M_FACE) M_FACE_MAP;
typedef HASH_MAP(char *, bool) M_FAMILY_SET;
typedef HASH_MAP(char *, M_FILE) M_FILE_MAP;

static struct {
    bool initialized;
    bool use_system;
    M_FILE_ARRAY files;
    M_FACE_MAP faces;
    // The families that the system was asked for.
    M_FAMILY_SET asked;
    // The font files that FONT cannot open, by lowercase name.
    M_FILE_MAP named;
} m_Registry;

static void M_Init(void)
{
    if (!m_Registry.initialized) {
        Array_Init(&m_Registry.files, &g_HeapAllocator);
        HashMap_Init(&m_Registry.faces, &g_HeapAllocator);
        HashMap_Init(&m_Registry.asked, &g_HeapAllocator);
        HashMap_Init(&m_Registry.named, &g_HeapAllocator);
        m_Registry.initialized = true;
    }
}

static char *M_Key(const char *const name)
{
    char *const key = Str_Dup(name);
    Str_ToLower(key);
    return key;
}

static void M_FreeFile(M_FILE *const file)
{
    Mem_Free(file->source);
    Mem_Free(file->data);
}

static void M_PutUtf8(char **const out, const uint32_t code)
{
    char *p = *out;
    if (code < 0x80) {
        *p++ = (char)code;
    } else if (code < 0x800) {
        *p++ = (char)(0xc0 | code >> 6);
        *p++ = (char)(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
        *p++ = (char)(0xe0 | code >> 12);
        *p++ = (char)(0x80 | (code >> 6 & 0x3f));
        *p++ = (char)(0x80 | (code & 0x3f));
    } else {
        *p++ = (char)(0xf0 | code >> 18);
        *p++ = (char)(0x80 | (code >> 12 & 0x3f));
        *p++ = (char)(0x80 | (code >> 6 & 0x3f));
        *p++ = (char)(0x80 | (code & 0x3f));
    }
    *out = p;
}

// Converts UTF-16BE to UTF-8, or returns nullptr for a NUL or an unpaired
// surrogate.
static char *M_DecodeUtf16(const uint8_t *const data, const size_t size)
{
    char *const text = Mem_Alloc(size / 2 * 3 + 1);
    char *p = text;
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint32_t code = Text_ReadBe(data + i, 2);
        if (code >= 0xd800 && code < 0xdc00 && i + 3 < size) {
            const uint32_t low = Text_ReadBe(data + i + 2, 2);
            if (low >= 0xdc00 && low < 0xe000) {
                code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                i += 2;
            }
        }
        if (code == 0 || (code >= 0xd800 && code < 0xe000)) {
            Mem_Free(text);
            return nullptr;
        }
        M_PutUtf8(&p, code);
    }
    *p = '\0';
    return text;
}

// Returns the UTF-8 text of a name record, or nullptr for an encoding that
// is not read.
static char *M_DecodeName(
    const uint32_t platform, const uint32_t encoding, const uint8_t *const data,
    const size_t size)
{
    if (platform == 0
        || (platform == 3
            && (encoding == 0 || encoding == 1 || encoding == 10))) {
        return M_DecodeUtf16(data, size);
    }
    if (platform == 1 && encoding == 0) {
        for (size_t i = 0; i < size; i++) {
            if (data[i] == 0 || data[i] >= 0x80) {
                return nullptr;
            }
        }
        char *const text = Mem_Alloc(size + 1);
        memcpy(text, data, size);
        text[size] = '\0';
        return text;
    }
    if (platform == 1 && encoding == 1) {
        char *text;
        return Result_Ignore(Cp932_Decode((const char *)data, size, &text))
            ? text
            : nullptr;
    }
    return nullptr;
}

// Returns the table of the face at offset with tag, and its size, or
// nullptr.
static const uint8_t *M_FindTable(
    const uint8_t *const data, const size_t size, const size_t offset,
    const char *const tag, size_t *const length)
{
    if (offset > size || size - offset < 12) {
        return nullptr;
    }
    const uint32_t count = Text_ReadBe(data + offset + 4, 2);
    for (uint32_t i = 0; i < count; i++) {
        const size_t record = offset + 12 + (size_t)i * 16;
        if (record + 16 > size) {
            return nullptr;
        }
        if (memcmp(data + record, tag, 4) == 0) {
            const size_t table = Text_ReadBe(data + record + 8, 4);
            *length = Text_ReadBe(data + record + 12, 4);
            return table <= size && *length <= size - table ? data + table
                                                            : nullptr;
        }
    }
    return nullptr;
}

// Adds the names of the face whose table directory starts at offset. Only
// a Regular face, by its OS/2 selection flags or its subfamily name, adds
// its family names, so that a family opens its Regular face; every face
// adds its full names.
static void M_ReadFaceNames(
    const uint8_t *const data, const size_t size, const size_t offset,
    const int32_t face, M_NAME_ARRAY *const names)
{
    size_t length;
    bool regular = false;
    const uint8_t *const os2 = M_FindTable(data, size, offset, "OS/2", &length);
    if (os2 != nullptr && length >= 64) {
        regular = (Text_ReadBe(os2 + 62, 2) & M_FS_SELECTION_REGULAR) != 0;
    }
    const uint8_t *const name =
        M_FindTable(data, size, offset, "name", &length);
    if (name == nullptr || length < 6) {
        return;
    }
    const uint32_t name_count = Text_ReadBe(name + 2, 2);
    const size_t storage = Text_ReadBe(name + 4, 2);
    M_NAME_ARRAY families;
    Array_Init(&families, &g_HeapAllocator);
    for (uint32_t j = 0; j < name_count; j++) {
        const size_t entry = 6 + (size_t)j * 12;
        if (entry + 12 > length) {
            break;
        }
        const uint32_t id = Text_ReadBe(name + entry + 6, 2);
        const size_t text_size = Text_ReadBe(name + entry + 8, 2);
        const size_t text = storage + Text_ReadBe(name + entry + 10, 2);
        if ((id != M_NAME_FAMILY && id != M_NAME_SUBFAMILY && id != M_NAME_FULL
             && id != M_NAME_TYPOGRAPHIC_FAMILY)
            || text > length || text_size > length - text || text_size == 0) {
            continue;
        }
        char *const decoded = M_DecodeName(
            Text_ReadBe(name + entry, 2), Text_ReadBe(name + entry + 2, 2),
            name + text, text_size);
        if (decoded == nullptr) {
            continue;
        }
        Str_ToLower(decoded);
        if (id == M_NAME_SUBFAMILY) {
            regular = regular || strcmp(decoded, "regular") == 0;
            Mem_Free(decoded);
        } else {
            Array_Append(
                id == M_NAME_FULL ? names : &families,
                ((M_NAME) { decoded, face }));
        }
    }
    for (int32_t i = 0; i < families.count; i++) {
        if (regular) {
            Array_Append(names, families.items[i]);
        } else {
            Mem_Free(families.items[i].key);
        }
    }
    Array_Free(&families);
}

static void M_FreeNames(M_NAME_ARRAY *const names)
{
    for (int32_t i = 0; i < names->count; i++) {
        Mem_Free(names->items[i].key);
    }
    Array_Free(names);
}

// Reads the names of every face of the font in data.
static RESULT M_ReadNames(
    const char *const source, const uint8_t *const data, const size_t size,
    M_NAME_ARRAY *const names)
{
    Array_Init(names, &g_HeapAllocator);
    FAIL_IF(data == nullptr || size < 12, "%s is not a font file", source);
    const uint32_t tag = Text_ReadBe(data, 4);
    if (tag == 0x74746366) {
        const uint32_t count = size >= 16 ? Text_ReadBe(data + 8, 4) : 0;
        for (uint32_t i = 0; i < count && 12 + (size_t)i * 4 + 4 <= size; i++) {
            M_ReadFaceNames(
                data, size, Text_ReadBe(data + 12 + (size_t)i * 4, 4),
                (int32_t)i, names);
        }
    } else if (tag == 0x00010000 || tag == 0x4f54544f || tag == 0x74727565) {
        M_ReadFaceNames(data, size, 0, 0, names);
    } else {
        return FAIL("%s is not a font file", source);
    }
    if (names->count == 0) {
        M_FreeNames(names);
        return FAIL("%s has no family name", source);
    }
    return OK;
}

// Adds data, which the registry then owns, under names. A name that an
// added font has changes hands only with override.
static void M_Commit(
    const char *const source, uint8_t *const data, const size_t size,
    M_NAME_ARRAY *const names, const bool override)
{
    M_Init();
    const int32_t file = m_Registry.files.count;
    Array_Append(&m_Registry.files, ((M_FILE) { Str_Dup(source), data, size }));
    for (int32_t i = 0; i < names->count; i++) {
        bool found;
        M_FACE *const face =
            HashMap_GetOrPut(&m_Registry.faces, names->items[i].key, &found);
        // The first face of a file with a name keeps it, as the regular
        // face comes first in a collection.
        if (!found || (override && face->file != file)) {
            face->file = file;
            face->face = names->items[i].face;
        }
    }
    M_FreeNames(names);
}

RESULT FontRegistry_Add(
    const char *const source, const void *const data, const size_t size)
{
    M_NAME_ARRAY names;
    MUST(M_ReadNames(source, data, size, &names));
    M_Commit(source, Mem_Dup(data, size), size, &names, true);
    return OK;
}

void FontRegistry_AddFile(
    const char *const name, const char *const source, const void *const data,
    const size_t size)
{
    M_Init();
    char *const key = M_Key(name);
    bool found;
    M_FILE *const file = HashMap_GetOrPut(&m_Registry.named, key, &found);
    Mem_Free(key);
    if (found) {
        M_FreeFile(file);
    }
    *file = (M_FILE) { Str_Dup(source), Mem_Dup(data, size), size };
}

bool FontRegistry_FindFile(
    const char *const name, const void **const data, size_t *const size)
{
    M_Init();
    char *const key = M_Key(name);
    const M_FILE *const file = HashMap_Get(&m_Registry.named, key);
    Mem_Free(key);
    if (file == nullptr) {
        return false;
    }
    *data = file->data;
    *size = file->size;
    return true;
}

RESULT FontRegistry_AddSystemData(
    const char *const source, uint8_t *const data, const size_t size,
    const char *const family)
{
    M_NAME_ARRAY names;
    const RESULT read = M_ReadNames(source, data, size, &names);
    if (!IS_OK(read)) {
        Mem_Free(data);
        return read;
    }
    char *const key = M_Key(family);
    bool found = false;
    for (int32_t i = 0; i < names.count && !found; i++) {
        found = strcmp(names.items[i].key, key) == 0;
    }
    Mem_Free(key);
    if (!found) {
        M_FreeNames(&names);
        Mem_Free(data);
        return FAIL("%s is not the font %s", source, family);
    }
    M_Commit(source, data, size, &names, false);
    return OK;
}

RESULT FontRegistry_AddSystemFile(
    const char *const path, const char *const family)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const size_t size = Reader_Remaining(&reader);
    uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    if (!IS_OK(result)) {
        Mem_Free(data);
        return result;
    }
    return FontRegistry_AddSystemData(path, data, size, family);
}

void FontRegistry_Reset(const bool use_system)
{
    if (m_Registry.initialized) {
        for (int32_t i = 0; i < m_Registry.files.count; i++) {
            M_FreeFile(&m_Registry.files.items[i]);
        }
        Array_Free(&m_Registry.files);
        for (int32_t i = 0; HashMap_Next(&m_Registry.named, &i); i++) {
            M_FreeFile(&m_Registry.named.values[i]);
        }
        HashMap_Free(&m_Registry.named);
        HashMap_Free(&m_Registry.faces);
        HashMap_Free(&m_Registry.asked);
    }
    memset(&m_Registry, 0, sizeof(m_Registry));
    m_Registry.use_system = use_system;
}

RESULT FontRegistry_Open(
    const char *const family, const float pixel_height, FONT **const out)
{
    *out = nullptr;
    FAIL_IF(family == nullptr, "Missing font family");
    M_Init();
    char *const key = M_Key(family);
    const M_FACE *face = HashMap_Get(&m_Registry.faces, key);
    if (face == nullptr && m_Registry.use_system
        && !HashMap_Contains(&m_Registry.asked, key)) {
        HashMap_Put(&m_Registry.asked, key, true);
        Result_Log(FontRegistry_AddSystemFont(family), LOG_LEVEL_INFO);
        face = HashMap_Get(&m_Registry.faces, key);
    }
    Mem_Free(key);
    FAIL_IF(face == nullptr, "No font has the family %s", family);
    const M_FILE *const file = &m_Registry.files.items[face->file];
    MUST(
        Font_Open(file->data, file->size, face->face, pixel_height, out), "%s",
        file->source);
    return OK;
}
