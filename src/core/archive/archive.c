#include "core/archive.h"

#include "core/archive/format.h"
#include "core/base/array.h"
#include "core/base/hash_map.h"
#include "core/base/string.h"

#include <string.h>

struct ARCHIVE_ENTRY {
    char *name;
    size_t size;
    uint32_t mode;
    void *data;
    size_t data_size;
};

typedef ARRAY(ARCHIVE_ENTRY) ARCHIVE_ENTRY_ARRAY;
typedef HASH_MAP(const char *, int32_t) ARCHIVE_INDEX;

struct ARCHIVE_TABLE {
    ARCHIVE_ENTRY_ARRAY entries;
    const ALLOCATOR *allocator;
};

struct ARCHIVE {
    VFS_SOURCE base;
    const ARCHIVE_FORMAT *format;
    READER reader;
    ARCHIVE_TABLE table;
    // Maps the lookup key of each entry to its index, as the format looks
    // names up.
    ARCHIVE_INDEX by_key;
    // Maps each entry that is not a directory to its index, by the path the
    // VFS source finds it at.
    ARCHIVE_INDEX by_source_path;
    const ALLOCATOR *allocator;
};

static char *M_Dup(const ALLOCATOR *const allocator, const char *const text)
{
    const size_t size = strlen(text) + 1;
    char *const copy = Allocator_Alloc(allocator, size);
    memcpy(copy, text, size);
    return copy;
}

// Folds the ASCII letters of text to lower case in place, matching how the
// VFS normalises the paths that a source receives.
static void M_FoldAscii(char *const text)
{
    for (char *c = text; *c != '\0'; c++) {
        if (*c >= 'A' && *c <= 'Z') {
            *c = (char)(*c - 'A' + 'a');
        }
    }
}

static const char *M_LastComponent(const char *const path)
{
    const char *const slash = strrchr(path, '/');
    return slash == nullptr ? path : slash + 1;
}

// Returns the key that the format looks path up by, as a new string that
// Mem_Free releases.
static char *M_Key(const ARCHIVE_LOOKUP lookup, const char *const path)
{
    if (lookup == ARCHIVE_LOOKUP_PATH) {
        return Str_Dup(path);
    }
    char *const key = Str_Dup(M_LastComponent(path));
    M_FoldAscii(key);
    return key;
}

static bool M_IsDirectory(const ARCHIVE_ENTRY *const entry)
{
    const size_t length = strlen(entry->name);
    return length > 0 && entry->name[length - 1] == '/';
}

static const char *M_Name(const ARCHIVE *const archive)
{
    return Reader_GetName(&archive->reader);
}

static const ARCHIVE_ENTRY *M_FindSourcePath(
    const ARCHIVE *const archive, const char *const path)
{
    if (archive->format->lookup == ARCHIVE_LOOKUP_LAST_NOCASE) {
        const ARCHIVE_ENTRY *const entry = Archive_Find(archive, path);
        return entry == nullptr || M_IsDirectory(entry) ? nullptr : entry;
    }
    const int32_t *const index = HashMap_Get(&archive->by_source_path, path);
    return index == nullptr ? nullptr : &archive->table.entries.items[*index];
}

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    return M_FindSourcePath((const ARCHIVE *)source, path) != nullptr;
}

static RESULT M_Open(
    VFS_SOURCE *const source, const char *const path,
    [[maybe_unused]] const VFS_MODE mode, READER *const out)
{
    ARCHIVE *const archive = (ARCHIVE *)source;
    const ARCHIVE_ENTRY *const entry = M_FindSourcePath(archive, path);
    FAIL_IF(entry == nullptr, "%s: %s: not found", source->name, path);
    uint8_t *data;
    MUST(Archive_Read(archive, entry, &data));
    const char *const archive_name = M_Name(archive);
    char *const name = archive_name == nullptr
        ? Str_Dup(entry->name)
        : Str_Format("%s/%s", archive_name, entry->name);
    Reader_OpenOwnedBuffer(out, data, entry->size, archive->allocator, name);
    Mem_Free(name);
    return OK;
}

static void M_List(
    const VFS_SOURCE *const source,
    void (*const visit)(void *user, const char *path), void *const user)
{
    const ARCHIVE *const archive = (const ARCHIVE *)source;
    for (int32_t i = 0; i < archive->table.entries.count; i++) {
        const ARCHIVE_ENTRY *const entry = &archive->table.entries.items[i];
        if (M_IsDirectory(entry)) {
            continue;
        }
        char *const path = Str_Dup(entry->name);
        M_FoldAscii(path);
        visit(user, path);
        Mem_Free(path);
    }
}

static void M_Close(VFS_SOURCE *const source)
{
    Archive_Close((ARCHIVE *)source);
}

static const VFS_SOURCE_FUNCS M_SourceFuncs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

void ArchiveTable_Add(
    ARCHIVE_TABLE *const table, const char *const name, const size_t size,
    const uint32_t mode, const void *const data, const size_t data_size)
{
    void *const record = Allocator_Alloc(table->allocator, data_size);
    if (data_size > 0) {
        memcpy(record, data, data_size);
    }
    Array_Append(
        &table->entries,
        ((ARCHIVE_ENTRY) {
            .name = M_Dup(table->allocator, name),
            .size = size,
            .mode = mode,
            .data = record,
            .data_size = data_size,
        }));
}

RESULT Archive_Open(
    const ARCHIVE_FORMAT *const format, READER *const reader,
    const ALLOCATOR *const allocator, ARCHIVE **const out)
{
    *out = nullptr;
    ARCHIVE *const archive = Allocator_AllocZ(allocator, sizeof(*archive));
    archive->format = format;
    archive->reader = *reader;
    archive->allocator = allocator;
    archive->table.allocator = allocator;
    Array_Init(&archive->table.entries, allocator);
    HashMap_Init(&archive->by_key, allocator);
    HashMap_Init(&archive->by_source_path, allocator);

    // The format reads through a slice, so that a failed read leaves no error
    // flag on the archive's own reader.
    Reader_Seek(&archive->reader, 0);
    READER whole =
        Reader_Slice(&archive->reader, Reader_Size(&archive->reader));
    RESULT result = format->read_table(&whole, &archive->table);
    if (!IS_OK(result)) {
        const char *const name = M_Name(archive);
        if (name != nullptr) {
            result = Result_Prefix(result, "%s", name);
        }
        Archive_Close(archive);
        return result;
    }
    for (int32_t i = 0; i < archive->table.entries.count; i++) {
        char *const key =
            M_Key(format->lookup, archive->table.entries.items[i].name);
        HashMap_Put(&archive->by_key, key, i);
        Mem_Free(key);
    }
    *out = archive;
    return OK;
}

void Archive_Close(ARCHIVE *const archive)
{
    for (int32_t i = 0; i < archive->table.entries.count; i++) {
        ARCHIVE_ENTRY *const entry = &archive->table.entries.items[i];
        Allocator_Free(
            archive->allocator, entry->name, strlen(entry->name) + 1);
        Allocator_Free(archive->allocator, entry->data, entry->data_size);
    }
    Array_Free(&archive->table.entries);
    HashMap_Free(&archive->by_key);
    HashMap_Free(&archive->by_source_path);
    Reader_Close(&archive->reader);
    Allocator_Free(archive->allocator, archive, sizeof(*archive));
}

int32_t Archive_GetCount(const ARCHIVE *const archive)
{
    return archive->table.entries.count;
}

const ARCHIVE_ENTRY *Archive_GetEntry(
    const ARCHIVE *const archive, const int32_t index)
{
    return Array_Get(&archive->table.entries, index);
}

const ARCHIVE_ENTRY *Archive_Find(
    const ARCHIVE *const archive, const char *const path)
{
    char *const key = M_Key(archive->format->lookup, path);
    const int32_t *const index = HashMap_Get(&archive->by_key, key);
    Mem_Free(key);
    return index == nullptr ? nullptr : &archive->table.entries.items[*index];
}

const char *ArchiveEntry_GetName(const ARCHIVE_ENTRY *const entry)
{
    return entry->name;
}

size_t ArchiveEntry_GetSize(const ARCHIVE_ENTRY *const entry)
{
    return entry->size;
}

uint32_t ArchiveEntry_GetMode(const ARCHIVE_ENTRY *const entry)
{
    return entry->mode;
}

RESULT Archive_Read(
    ARCHIVE *const archive, const ARCHIVE_ENTRY *const entry,
    uint8_t **const out)
{
    *out = nullptr;
    Reader_Seek(&archive->reader, 0);
    READER whole =
        Reader_Slice(&archive->reader, Reader_Size(&archive->reader));
    uint8_t *const data = Allocator_Alloc(archive->allocator, entry->size);
    const RESULT result =
        archive->format->read_entry(&whole, entry->data, data, entry->size);
    if (!IS_OK(result)) {
        Allocator_Free(archive->allocator, data, entry->size);
        const char *const name = M_Name(archive);
        return name == nullptr
            ? Result_Prefix(result, "%s", entry->name)
            : Result_Prefix(result, "%s: %s", name, entry->name);
    }
    *out = data;
    return OK;
}

VFS_SOURCE *Archive_ToSource(ARCHIVE *const archive)
{
    archive->base.funcs = &M_SourceFuncs;
    const char *const name = M_Name(archive);
    archive->base.name = name == nullptr ? "archive" : name;
    if (archive->format->lookup == ARCHIVE_LOOKUP_PATH) {
        for (int32_t i = 0; i < archive->table.entries.count; i++) {
            const ARCHIVE_ENTRY *const entry = &archive->table.entries.items[i];
            if (M_IsDirectory(entry)) {
                continue;
            }
            char *const path = Str_Dup(entry->name);
            M_FoldAscii(path);
            HashMap_Put(&archive->by_source_path, path, i);
            Mem_Free(path);
        }
    }
    return &archive->base;
}
