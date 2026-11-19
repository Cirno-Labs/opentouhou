#include "core/vfs/dir_source.h"

#include "core/base/array.h"
#include "core/base/hash_map.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/reader.h"
#include "core/io/writer.h"
#include "core/vfs/normalize.h"

#include <string.h>

// Caps how deep a read-only directory may nest, so that a symbolic link
// cycle cannot walk forever.
#define M_MAX_DEPTH 32

// Holds a file of a read-only directory: its path below the directory as
// the disk spells it, and its disk path.
typedef struct {
    char *spelt;
    char *disk;
} M_ENTRY;

typedef HASH_MAP(const char *, M_ENTRY) M_ENTRIES;

// Holds the last path that a writable directory resolved, so that the
// contains and open of one Vfs_Open walk the disk once.
typedef struct {
    char *path;
    char *disk;
} M_LAST;

typedef struct {
    VFS_SOURCE base;
    char *name;
    const ALLOCATOR *allocator;
    // Map each file of a read-only directory, in normal form, to its
    // entry, and list the normal forms in the order found.
    M_ENTRIES entries;
    ARRAY(char *) order;
    bool writable;
    M_LAST last;
} DIR_SOURCE;

static char *M_Copy(
    const ALLOCATOR *const allocator, const char *const text,
    const size_t length)
{
    char *const copy = Allocator_Alloc(allocator, length + 1);
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static void M_FreeText(const ALLOCATOR *const allocator, char *const text)
{
    if (text != nullptr) {
        Allocator_Free(allocator, text, strlen(text) + 1);
    }
}

// Returns whether name, as the disk spells it, has normal as its normal
// form.
static bool M_Matches(
    const ALLOCATOR *const allocator, const char *const name,
    const char *const normal)
{
    char *normalized;
    if (!Result_Ignore(VfsNormalize_Path(name, allocator, &normalized))) {
        return false;
    }
    const bool same = strcmp(normalized, normal) == 0;
    M_FreeText(allocator, normalized);
    return same;
}

// Returns the path of the entry of disk_dir whose name is normal in normal
// form, as the disk spells it, or nullptr. Of several, takes the last in
// byte order, which normal itself is when the disk has it, so on a
// case-sensitive file system only a directory without that spelling is
// listed. Elsewhere the exact name would hide the disk's spelling.
static char *M_FindEntry(
    const ALLOCATOR *const allocator, const char *const disk_dir,
    const char *const normal)
{
#if !defined(_WIN32) && !defined(__APPLE__)
    char *const exact = Path_Join(allocator, disk_dir, normal);
    if (File_Exists(exact)) {
        return exact;
    }
    M_FreeText(allocator, exact);
#endif
    FILE_LIST list;
    if (!Result_Ignore(File_ListDir(disk_dir, allocator, &list))) {
        return nullptr;
    }
    const char *found = nullptr;
    for (int32_t i = 0; i < list.count; i++) {
        if (M_Matches(allocator, list.items[i], normal)) {
            found = list.items[i];
        }
    }
    char *const path =
        found != nullptr ? Path_Join(allocator, disk_dir, found) : nullptr;
    File_FreeList(&list);
    return path;
}

// Walks path, below the directory, part by part, and sets *out to the disk
// path it names, or to nullptr when the disk lacks a part. Each part takes
// the disk's spelling in any case. With create, a missing part takes the
// spelling that path gives, and each directory on the way is made.
static RESULT M_Walk(
    const DIR_SOURCE *const dir, const char *const path, const bool create,
    char **const out)
{
    const ALLOCATOR *const allocator = dir->allocator;
    *out = nullptr;
    char *disk = Str_DupWith(allocator, dir->name);
    const char *part = path;
    while (disk != nullptr && *part != '\0') {
        const size_t length = strcspn(part, "/");
        char *const given = M_Copy(allocator, part, length);
        char *normal;
        RESULT result = VfsNormalize_Path(given, allocator, &normal);
        char *next = nullptr;
        if (IS_OK(result)) {
            next = M_FindEntry(allocator, disk, normal);
            M_FreeText(allocator, normal);
        }
        if (IS_OK(result) && next == nullptr && create) {
            next = Path_Join(allocator, disk, given);
        }
        M_FreeText(allocator, given);
        M_FreeText(allocator, disk);
        disk = next;
        part += length;
        if (*part == '/') {
            part++;
            if (IS_OK(result) && create) {
                result = File_MakeDir(disk);
            }
        }
        if (!IS_OK(result)) {
            M_FreeText(allocator, disk);
            return result;
        }
    }
    *out = disk;
    return OK;
}

// Returns the disk path of the directory, or else the file, at path, or
// nullptr when the disk has none.
static char *M_Resolve(
    const DIR_SOURCE *const dir, const char *const path, const bool directory)
{
    char *disk;
    FILE_TYPE type;
    if (!Result_Ignore(M_Walk(dir, path, false, &disk)) || disk == nullptr) {
        return nullptr;
    }
    if (!Result_Ignore(File_GetType(disk, &type))
        || (type == FILE_TYPE_DIR) != directory) {
        M_FreeText(dir->allocator, disk);
        return nullptr;
    }
    return disk;
}

static void M_Forget(DIR_SOURCE *const dir)
{
    M_FreeText(dir->allocator, dir->last.path);
    M_FreeText(dir->allocator, dir->last.disk);
    dir->last = (M_LAST) {};
}

// Returns the disk path of the file at path, owned by dir, or nullptr.
static const char *M_FindFile(
    const DIR_SOURCE *const dir, const char *const path)
{
    if (!dir->writable) {
        const M_ENTRY *const entry = HashMap_Get(&dir->entries, path);
        return entry != nullptr ? entry->disk : nullptr;
    }
    // The cache of the last path changes under the const of a lookup.
    DIR_SOURCE *const cache = (DIR_SOURCE *)dir;
    if (cache->last.path == nullptr || strcmp(cache->last.path, path) != 0) {
        M_Forget(cache);
        cache->last.path = Str_DupWith(dir->allocator, path);
        cache->last.disk = M_Resolve(dir, path, false);
    }
    return cache->last.disk;
}

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    return M_FindFile((const DIR_SOURCE *)source, path) != nullptr;
}

static RESULT M_ReadWhole(
    const char *const real_path, const ALLOCATOR *const allocator,
    READER *const out)
{
    READER file;
    MUST(Reader_OpenFile(&file, real_path));
    const size_t size = Reader_Size(&file);
    void *const data = Reader_ReadAlloc(&file, size, allocator);
    const RESULT result = Reader_Result(&file);
    Reader_Close(&file);
    if (!IS_OK(result)) {
        return result;
    }
    Reader_OpenOwnedBuffer(out, data, size, allocator, real_path);
    return OK;
}

static RESULT M_Open(
    VFS_SOURCE *const source, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    const DIR_SOURCE *const dir = (const DIR_SOURCE *)source;
    const char *const disk = M_FindFile(dir, path);
    FAIL_IF(disk == nullptr, "%s: not found in %s", path, dir->name);
    return mode == VFS_STREAM ? Reader_OpenFile(out, disk)
                              : M_ReadWhole(disk, dir->allocator, out);
}

static void M_List(
    const VFS_SOURCE *const source, const char *const path,
    void (*const visit)(void *user, const char *name), void *const user)
{
    const DIR_SOURCE *const dir = (const DIR_SOURCE *)source;
    const ALLOCATOR *const allocator = dir->allocator;
    if (!dir->writable) {
        for (int32_t i = 0; i < dir->order.count; i++) {
            const char *const normal = dir->order.items[i];
            const char *const name = VfsNormalize_NameBelow(normal, path);
            if (name != nullptr) {
                // Normal form keeps the length and the separators.
                const M_ENTRY *const entry = HashMap_Get(&dir->entries, normal);
                visit(user, entry->spelt + (name - normal));
            }
        }
        return;
    }
    char *const disk = M_Resolve(dir, path, true);
    FILE_LIST list;
    if (disk == nullptr
        || !Result_Ignore(File_ListDir(disk, allocator, &list))) {
        M_FreeText(allocator, disk);
        return;
    }
    for (int32_t i = 0; i < list.count; i++) {
        char *const entry = Path_Join(allocator, disk, list.items[i]);
        FILE_TYPE type;
        if (Result_Ignore(File_GetType(entry, &type))
            && type != FILE_TYPE_DIR) {
            visit(user, list.items[i]);
        }
        M_FreeText(allocator, entry);
    }
    File_FreeList(&list);
    M_FreeText(allocator, disk);
}

static RESULT M_Write(
    VFS_SOURCE *const source, const char *const path, const void *const data,
    const size_t size)
{
    DIR_SOURCE *const dir = (DIR_SOURCE *)source;
    M_Forget(dir);
    char *disk;
    MUST(M_Walk(dir, path, true, &disk));
    WRITER writer;
    RESULT result = Writer_OpenFile(&writer, disk);
    M_FreeText(dir->allocator, disk);
    if (IS_OK(result)) {
        Writer_WriteBytes(&writer, data, size);
        result = Writer_Close(&writer);
    }
    return result;
}

static void M_Close(VFS_SOURCE *const source)
{
    DIR_SOURCE *const dir = (DIR_SOURCE *)source;
    for (int32_t i = 0; HashMap_Next(&dir->entries, &i); i++) {
        M_FreeText(dir->allocator, dir->entries.values[i].spelt);
        M_FreeText(dir->allocator, dir->entries.values[i].disk);
    }
    HashMap_Free(&dir->entries);
    for (int32_t i = 0; i < dir->order.count; i++) {
        M_FreeText(dir->allocator, dir->order.items[i]);
    }
    Array_Free(&dir->order);
    M_Forget(dir);
    M_FreeText(dir->allocator, dir->name);
    Allocator_Free(dir->allocator, dir, sizeof(*dir));
}

static const VFS_SOURCE_FUNCS M_ReadFuncs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

static const VFS_SOURCE_FUNCS M_WriteFuncs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .write = M_Write,
    .close = M_Close,
};

// Records each file below disk_dir, at path below the directory, in the
// entries of dir. Of two files of one normal form, takes the later found.
static RESULT M_Record(
    DIR_SOURCE *const dir, const char *const disk_dir, const char *const path,
    const int32_t depth)
{
    const ALLOCATOR *const allocator = dir->allocator;
    FAIL_IF(depth > M_MAX_DEPTH, "%s: nested too deeply", disk_dir);
    FILE_LIST list;
    MUST(File_ListDir(disk_dir, allocator, &list));
    RESULT result = OK;
    for (int32_t i = 0; IS_OK(result) && i < list.count; i++) {
        char *const disk = Path_Join(allocator, disk_dir, list.items[i]);
        char *const given = Path_Join(allocator, path, list.items[i]);
        char *normal = nullptr;
        FILE_TYPE type;
        result = File_GetType(disk, &type);
        if (IS_OK(result) && type == FILE_TYPE_DIR) {
            result = M_Record(dir, disk, given, depth + 1);
        } else if (
            IS_OK(result)
            && Result_Ignore(VfsNormalize_Path(given, allocator, &normal))) {
            bool found;
            M_ENTRY *const slot =
                HashMap_GetOrPut(&dir->entries, normal, &found);
            if (found) {
                M_FreeText(allocator, slot->spelt);
                M_FreeText(allocator, slot->disk);
                M_FreeText(allocator, normal);
            } else {
                Array_Append(&dir->order, normal);
            }
            *slot = (M_ENTRY) { .spelt = given, .disk = disk };
            continue;
        }
        M_FreeText(allocator, disk);
        M_FreeText(allocator, given);
    }
    File_FreeList(&list);
    return result;
}

RESULT VfsDirSource_Create(
    const char *const path, const bool writable,
    const ALLOCATOR *const allocator, VFS_SOURCE **const out)
{
    FILE_TYPE type;
    FAIL_IF(
        !Result_Ignore(File_GetType(path, &type)) || type != FILE_TYPE_DIR,
        "%s: not a directory", path);
    DIR_SOURCE *const dir = Allocator_AllocZ(allocator, sizeof(*dir));
    dir->base.funcs = writable ? &M_WriteFuncs : &M_ReadFuncs;
    dir->name = Str_DupWith(allocator, path);
    dir->base.name = dir->name;
    dir->allocator = allocator;
    HashMap_Init(&dir->entries, allocator);
    Array_Init(&dir->order, allocator);
    dir->writable = writable;
    if (!writable) {
        const RESULT result = M_Record(dir, path, "", 0);
        if (!IS_OK(result)) {
            M_Close(&dir->base);
            return result;
        }
    }
    *out = &dir->base;
    return OK;
}
