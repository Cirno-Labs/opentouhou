#include "core/vfs.h"

#include "core/base/array.h"
#include "core/base/string.h"
#include "core/vfs/dir_source.h"
#include "core/vfs/normalize.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    VFS_SOURCE *source;
    // Holds the normalized directory that the source covers, "" for all.
    char *mount_at;
    int32_t priority;
} VFS_MOUNT;

typedef struct {
    // Holds the name as the winning source spells it.
    char *name;
    char *normalized;
    int32_t priority;
} M_LISTED;

typedef ARRAY(M_LISTED) M_LISTED_ARRAY;

typedef struct {
    const VFS *vfs;
    int32_t priority;
    M_LISTED_ARRAY *listed;
} M_LIST_VISIT;

typedef ARRAY(VFS_MOUNT) VFS_MOUNT_ARRAY;

struct VFS {
    VFS_MOUNT_ARRAY mounts;
    VFS_HOOK hook;
    void *hook_context;
    const ALLOCATOR *allocator;
};

static void M_FreeText(const VFS *const vfs, char *const text)
{
    Allocator_Free(vfs->allocator, text, strlen(text) + 1);
}

// Returns the highest-priority mount that covers path and, unless writing,
// has it, breaking a tie between equal priorities in favor of the one
// mounted later, or nullptr when none does.
static const VFS_MOUNT *M_Find(
    const VFS *const vfs, const char *const path, const bool writing)
{
    const VFS_MOUNT *best = nullptr;
    for (int32_t i = 0; i < vfs->mounts.count; i++) {
        const VFS_MOUNT *const mount = &vfs->mounts.items[i];
        const char *const below = VfsNormalize_Below(path, mount->mount_at);
        if (below == nullptr
            || (writing
                    ? mount->source->funcs->write == nullptr
                    : !mount->source->funcs->contains(mount->source, below))) {
            continue;
        }
        if (best == nullptr || mount->priority >= best->priority) {
            best = mount;
        }
    }
    return best;
}

static char *M_JoinSourceNames(const VFS *const vfs)
{
    char *joined = Str_Dup("");
    for (int32_t i = 0; i < vfs->mounts.count; i++) {
        const char *const name = vfs->mounts.items[i].source->name;
        char *const next =
            i == 0 ? Str_Dup(name) : Str_Format("%s, %s", joined, name);
        Mem_Free(joined);
        joined = next;
    }
    return joined;
}

static RESULT M_NotFound(
    const VFS *const vfs, const char *const path, const char *const normalized)
{
    char *const names = M_JoinSourceNames(vfs);
    const RESULT result = strcmp(path, normalized) == 0
        ? FAIL("%s: not found, searched: %s", path, names)
        : FAIL(
              "%s (normalized %s): not found, searched: %s", path, normalized,
              names);
    Mem_Free(names);
    return result;
}

VFS *Vfs_Create(const ALLOCATOR *const allocator)
{
    VFS *const vfs = Allocator_AllocZ(allocator, sizeof(*vfs));
    Array_Init(&vfs->mounts, allocator);
    vfs->allocator = allocator;
    return vfs;
}

void Vfs_Destroy(VFS *const vfs)
{
    for (int32_t i = 0; i < vfs->mounts.count; i++) {
        VFS_SOURCE *const source = vfs->mounts.items[i].source;
        source->funcs->close(source);
        M_FreeText(vfs, vfs->mounts.items[i].mount_at);
    }
    Array_Free(&vfs->mounts);
    Allocator_Free(vfs->allocator, vfs, sizeof(*vfs));
}

void Vfs_MountSource(
    VFS *const vfs, const char *const mount_at, VFS_SOURCE *const source,
    const int32_t priority)
{
    Array_Append(
        &vfs->mounts,
        ((VFS_MOUNT) {
            .source = source,
            .mount_at = Str_DupWith(vfs->allocator, mount_at),
            .priority = priority,
        }));
}

RESULT Vfs_MountDir(
    VFS *const vfs, const char *const mount_at, const char *const path,
    const int32_t priority, const bool writable)
{
    VFS_SOURCE *source;
    MUST(VfsDirSource_Create(path, writable, vfs->allocator, &source));
    Vfs_MountSource(vfs, mount_at, source, priority);
    return OK;
}

bool Vfs_Exists(const VFS *const vfs, const char *const path)
{
    char *normalized;
    const RESULT result = VfsNormalize_Path(path, vfs->allocator, &normalized);
    if (!IS_OK(result)) {
        Result_Ignore(result);
        return false;
    }
    const bool found = M_Find(vfs, normalized, false) != nullptr;
    M_FreeText(vfs, normalized);
    return found;
}

RESULT Vfs_Open(
    VFS *const vfs, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    char *normalized;
    MUST(VfsNormalize_Path(path, vfs->allocator, &normalized), "%s", path);
    const VFS_MOUNT *const mount = M_Find(vfs, normalized, false);
    if (mount == nullptr) {
        const RESULT result = M_NotFound(vfs, path, normalized);
        M_FreeText(vfs, normalized);
        return result;
    }
    RESULT result = mount->source->funcs->open(
        mount->source, VfsNormalize_Below(normalized, mount->mount_at), mode,
        out);
    M_FreeText(vfs, normalized);
    MUST(result, "%s", path);

    if (mode == VFS_IN_MEMORY && vfs->hook != nullptr) {
        result = vfs->hook(vfs->hook_context, path, out);
        if (!IS_OK(result)) {
            Reader_Close(out);
            return result;
        }
    }
    return OK;
}

void Vfs_SetHook(VFS *const vfs, const VFS_HOOK hook, void *const context)
{
    vfs->hook = hook;
    vfs->hook_context = context;
}

RESULT Vfs_Write(
    VFS *const vfs, const char *const path, const void *const data,
    const size_t size)
{
    char *normalized;
    MUST(VfsNormalize_Path(path, vfs->allocator, &normalized), "%s", path);
    const VFS_MOUNT *const mount = M_Find(vfs, normalized, true);
    const size_t below = mount != nullptr
        ? (size_t)(VfsNormalize_Below(normalized, mount->mount_at) - normalized)
        : 0;
    M_FreeText(vfs, normalized);
    FAIL_IF(mount == nullptr, "%s: no writable source covers it", path);
    // The source makes missing parts in path's case. Both forms have one
    // length.
    char *spelled;
    MUST(VfsNormalize_PathKeepCase(path, vfs->allocator, &spelled), "%s", path);
    const RESULT result =
        mount->source->funcs->write(mount->source, spelled + below, data, size);
    M_FreeText(vfs, spelled);
    return Result_Prefix(result, "%s", path);
}

RESULT Vfs_Remove(VFS *const vfs, const char *const name)
{
    FAIL_IF(
        name[0] == '\0' || strpbrk(name, "/\\:") != nullptr
            || strcmp(name, ".") == 0 || strcmp(name, "..") == 0,
        "%s: not a file name", name);
    char *normalized;
    MUST(VfsNormalize_Path(name, vfs->allocator, &normalized), "%s", name);
    const VFS_MOUNT *const mount = M_Find(vfs, normalized, true);
    RESULT result;
    if (mount == nullptr || mount->mount_at[0] != '\0') {
        result = FAIL("no writable source holds it");
    } else if (mount->source->funcs->remove == nullptr) {
        result = FAIL("source does not support removal");
    } else {
        result = mount->source->funcs->remove(mount->source, normalized);
    }
    M_FreeText(vfs, normalized);
    return Result_Prefix(result, "%s", name);
}

static void M_ListVisit(void *const user, const char *const name)
{
    M_LIST_VISIT *const visit = user;
    if (name[0] == '\0' || strchr(name, '\\') != nullptr) {
        return;
    }
    char *normalized;
    if (!Result_Ignore(
            VfsNormalize_Path(name, visit->vfs->allocator, &normalized))) {
        return;
    }
    M_LISTED_ARRAY *const listed = visit->listed;
    for (int32_t i = 0; i < listed->count; i++) {
        M_LISTED *const entry = &listed->items[i];
        if (strcmp(entry->normalized, normalized) == 0) {
            if (visit->priority >= entry->priority) {
                M_FreeText(visit->vfs, entry->name);
                entry->name = Str_DupWith(visit->vfs->allocator, name);
                entry->priority = visit->priority;
            }
            M_FreeText(visit->vfs, normalized);
            return;
        }
    }
    Array_Append(
        listed,
        ((M_LISTED) {
            .name = Str_DupWith(visit->vfs->allocator, name),
            .normalized = normalized,
            .priority = visit->priority,
        }));
}

static int M_CompareListed(const void *const a, const void *const b)
{
    return strcmp(((const M_LISTED *)a)->name, ((const M_LISTED *)b)->name);
}

void Vfs_List(
    const VFS *const vfs, const char *const dir,
    void (*const visit)(void *user, const char *name), void *const user)
{
    char *normalized;
    if (!Result_Ignore(VfsNormalize_Path(dir, vfs->allocator, &normalized))) {
        return;
    }
    M_LISTED_ARRAY listed;
    Array_Init(&listed, vfs->allocator);
    for (int32_t i = 0; i < vfs->mounts.count; i++) {
        const VFS_MOUNT *const mount = &vfs->mounts.items[i];
        const char *const below =
            VfsNormalize_Below(normalized, mount->mount_at);
        if (below == nullptr) {
            continue;
        }
        M_LIST_VISIT state = {
            .vfs = vfs,
            .priority = mount->priority,
            .listed = &listed,
        };
        mount->source->funcs->list(mount->source, below, M_ListVisit, &state);
    }
    qsort(
        listed.items, (size_t)listed.count, sizeof(M_LISTED), M_CompareListed);
    for (int32_t i = 0; i < listed.count; i++) {
        visit(user, listed.items[i].name);
        M_FreeText(vfs, listed.items[i].name);
        M_FreeText(vfs, listed.items[i].normalized);
    }
    Array_Free(&listed);
    M_FreeText(vfs, normalized);
}
