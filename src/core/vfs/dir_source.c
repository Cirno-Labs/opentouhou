#include "core/vfs/dir_source.h"

#include "core/base/array.h"
#include "core/base/hash_map.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/reader.h"
#include "core/vfs/normalize.h"

#include <SDL3/SDL.h>
#include <string.h>

// Caps how deep the directory tree may nest, so that a symbolic link cycle
// cannot walk forever.
#define M_MAX_DEPTH 32

typedef HASH_MAP(const char *, char *) DIR_ENTRIES;
typedef ARRAY(char *) DIR_PATHS;

typedef struct {
    VFS_SOURCE base;
    DIR_ENTRIES entries;
    // The normalised paths, in the order they were recorded, so list can
    // visit them without depending on the map's iteration order.
    DIR_PATHS order;
    char *name;
    const ALLOCATOR *allocator;
} DIR_SOURCE;

static void M_FreeEntries(DIR_SOURCE *const dir)
{
    for (int32_t i = 0; HashMap_Next(&dir->entries, &i); i++) {
        char *const real_path = dir->entries.values[i];
        Allocator_Free(dir->allocator, real_path, strlen(real_path) + 1);
    }
    HashMap_Free(&dir->entries);
    for (int32_t i = 0; i < dir->order.count; i++) {
        char *const path = dir->order.items[i];
        Allocator_Free(dir->allocator, path, strlen(path) + 1);
    }
    Array_Free(&dir->order);
}

// Records real_path, which Path_Join returned, under vfs_path in the map,
// taking ownership of real_path. A vfs_path already in the map is replaced,
// in favour of the entry found later while walking.
static RESULT M_Record(
    DIR_SOURCE *const dir, const char *const vfs_path, char *const real_path)
{
    char *normalized;
    const RESULT normalize_result =
        VfsNormalize_Path(vfs_path, dir->allocator, &normalized);
    if (!IS_OK(normalize_result)) {
        Allocator_Free(dir->allocator, real_path, strlen(real_path) + 1);
        return normalize_result;
    }
    bool found;
    char **const slot = HashMap_GetOrPut(&dir->entries, normalized, &found);
    if (found) {
        Allocator_Free(dir->allocator, *slot, strlen(*slot) + 1);
        Allocator_Free(dir->allocator, normalized, strlen(normalized) + 1);
    } else {
        Array_Append(&dir->order, normalized);
    }
    *slot = real_path;
    return OK;
}

static RESULT M_Walk(
    DIR_SOURCE *const dir, const char *const disk_dir,
    const char *const vfs_dir, const int depth)
{
    FAIL_IF(depth > M_MAX_DEPTH, "%s: nested too deeply", disk_dir);

    FILE_LIST list;
    MUST(File_ListDir(disk_dir, dir->allocator, &list));

    RESULT result = OK;
    for (int32_t i = 0; IS_OK(result) && i < list.count; i++) {
        const char *const name = list.items[i];
        char *const disk_path = Path_Join(dir->allocator, disk_dir, name);
        char *const vfs_path = Path_Join(dir->allocator, vfs_dir, name);
        SDL_PathInfo info;
        bool recorded = false;
        if (!SDL_GetPathInfo(disk_path, &info)) {
            result = FAIL("%s: %s", disk_path, SDL_GetError());
        } else if (info.type == SDL_PATHTYPE_DIRECTORY) {
            result = M_Walk(dir, disk_path, vfs_path, depth + 1);
        } else {
            // M_Record takes ownership of disk_path, whether it succeeds or
            // fails.
            result = M_Record(dir, vfs_path, disk_path);
            recorded = true;
        }
        Allocator_Free(dir->allocator, vfs_path, strlen(vfs_path) + 1);
        if (!recorded) {
            Allocator_Free(dir->allocator, disk_path, strlen(disk_path) + 1);
        }
    }
    File_FreeList(&list);
    return result;
}

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    const DIR_SOURCE *const dir = (const DIR_SOURCE *)source;
    return HashMap_Contains(&dir->entries, path);
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
    Reader_OpenOwnedBuffer(out, data, size, allocator);
    return OK;
}

static RESULT M_Open(
    VFS_SOURCE *const source, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    DIR_SOURCE *const dir = (DIR_SOURCE *)source;
    char **const real_path = HashMap_Get(&dir->entries, path);
    FAIL_IF(real_path == nullptr, "%s: not found in %s", path, dir->name);
    if (mode == VFS_STREAM) {
        return Reader_OpenFile(out, *real_path);
    }
    return M_ReadWhole(*real_path, dir->allocator, out);
}

static void M_List(
    const VFS_SOURCE *const source,
    void (*const visit)(void *user, const char *path), void *const user)
{
    const DIR_SOURCE *const dir = (const DIR_SOURCE *)source;
    for (int32_t i = 0; i < dir->order.count; i++) {
        visit(user, dir->order.items[i]);
    }
}

static void M_Close(VFS_SOURCE *const source)
{
    DIR_SOURCE *const dir = (DIR_SOURCE *)source;
    M_FreeEntries(dir);
    Mem_Free(dir->name);
    Allocator_Free(dir->allocator, dir, sizeof(*dir));
}

static const VFS_SOURCE_FUNCS M_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

RESULT DirSource_Create(
    const char *const path, const ALLOCATOR *const allocator,
    VFS_SOURCE **const out)
{
    *out = nullptr;
    DIR_SOURCE *const dir = Allocator_AllocZ(allocator, sizeof(*dir));
    dir->base.funcs = &M_Funcs;
    dir->name = Str_Dup(path);
    dir->base.name = dir->name;
    dir->allocator = allocator;
    HashMap_Init(&dir->entries, allocator);
    Array_Init(&dir->order, allocator);

    const RESULT result = M_Walk(dir, path, "", 0);
    if (!IS_OK(result)) {
        M_Close(&dir->base);
        return result;
    }
    *out = &dir->base;
    return OK;
}
