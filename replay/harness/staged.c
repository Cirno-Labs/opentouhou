#include "replay/harness/staged.h"

#include "core/base/memory.h"
#include "core/io/reader.h"
#include "core/vfs/normalize.h"

#include <string.h>

typedef struct {
    VFS_SOURCE base;
    const STAGED_FILE *files;
    size_t count;
} M_SOURCE;

static const STAGED_FILE *M_Find(
    const VFS_SOURCE *const base, const char *const path)
{
    const M_SOURCE *const source = (const M_SOURCE *)base;
    for (size_t i = source->count; i > 0; i--) {
        if (strcmp(source->files[i - 1].path, path) == 0) {
            return &source->files[i - 1];
        }
    }
    return nullptr;
}

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    return M_Find(source, path) != nullptr;
}

static RESULT M_Open(
    VFS_SOURCE *const source, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    const STAGED_FILE *const file = M_Find(source, path);
    FAIL_IF(file == nullptr, "%s: not staged", path);
    Reader_OpenBuffer(out, file->data, file->size, path);
    return OK;
}

static void M_List(
    const VFS_SOURCE *const base, const char *const dir,
    void (*const visit)(void *user, const char *name), void *const user)
{
    const M_SOURCE *const source = (const M_SOURCE *)base;
    for (size_t i = 0; i < source->count; i++) {
        const char *const name =
            VfsNormalize_NameBelow(source->files[i].path, dir);
        if (name != nullptr) {
            visit(user, name);
        }
    }
}

static void M_Close(VFS_SOURCE *const source)
{
    Mem_Free(source);
}

static const VFS_SOURCE_FUNCS M_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

VFS_SOURCE *Staged_CreateSource(
    const STAGED_FILE *const files, const size_t count)
{
    M_SOURCE *const source = Mem_Alloc(sizeof(*source));
    *source = (M_SOURCE) {
        .base = { .funcs = &M_Funcs, .name = "case" },
        .files = files,
        .count = count,
    };
    return &source->base;
}
