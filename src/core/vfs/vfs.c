#include "core/vfs.h"

#include "core/base/array.h"
#include "core/base/string.h"
#include "core/vfs/dir_source.h"
#include "core/vfs/normalize.h"

#include <string.h>

typedef struct {
    VFS_SOURCE *source;
    int32_t priority;
} VFS_MOUNT;

typedef ARRAY(VFS_MOUNT) VFS_MOUNT_ARRAY;

struct VFS {
    VFS_MOUNT_ARRAY mounts;
    VFS_HOOK hook;
    void *hook_context;
    const ALLOCATOR *allocator;
};

// Returns the highest-priority mounted source that has path, breaking a tie
// between equal priorities in favour of the one mounted later, or nullptr
// when none has it.
static VFS_SOURCE *M_Find(const VFS *const vfs, const char *const path)
{
    VFS_SOURCE *best = nullptr;
    int32_t best_priority = 0;
    for (int32_t i = 0; i < vfs->mounts.count; i++) {
        const VFS_MOUNT *const mount = &vfs->mounts.items[i];
        if (!mount->source->funcs->contains(mount->source, path)) {
            continue;
        }
        if (best == nullptr || mount->priority >= best_priority) {
            best = mount->source;
            best_priority = mount->priority;
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
              "%s (normalised %s): not found, searched: %s", path, normalized,
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
    }
    Array_Free(&vfs->mounts);
    Allocator_Free(vfs->allocator, vfs, sizeof(*vfs));
}

void Vfs_MountSource(
    VFS *const vfs, VFS_SOURCE *const source, const int32_t priority)
{
    Array_Append(&vfs->mounts, ((VFS_MOUNT) { source, priority }));
}

RESULT Vfs_MountDir(
    VFS *const vfs, const char *const path, const int32_t priority)
{
    VFS_SOURCE *source;
    MUST(DirSource_Create(path, vfs->allocator, &source));
    Vfs_MountSource(vfs, source, priority);
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
    const bool found = M_Find(vfs, normalized) != nullptr;
    Allocator_Free(vfs->allocator, normalized, strlen(normalized) + 1);
    return found;
}

RESULT Vfs_Open(
    VFS *const vfs, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    char *normalized;
    MUST(VfsNormalize_Path(path, vfs->allocator, &normalized), "%s", path);
    VFS_SOURCE *const source = M_Find(vfs, normalized);
    if (source == nullptr) {
        const RESULT result = M_NotFound(vfs, path, normalized);
        Allocator_Free(vfs->allocator, normalized, strlen(normalized) + 1);
        return result;
    }
    RESULT result = source->funcs->open(source, normalized, mode, out);
    Allocator_Free(vfs->allocator, normalized, strlen(normalized) + 1);
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
