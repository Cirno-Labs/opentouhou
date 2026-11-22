#include "shell/data.h"

#include "core/archive/zip.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/reader.h"

#include <string.h>

char *ShellData_FindRoot(const ARCHIVE *const archive)
{
    const char *root = nullptr;
    size_t root_size = 0;
    for (int32_t i = 0; i < Archive_GetCount(archive); i++) {
        const char *const name =
            ArchiveEntry_GetName(Archive_GetEntry(archive, i));
        const size_t size = strlen(name);
        if (size > 0 && name[size - 1] == '/') {
            continue;
        }
        const char *const slash = strchr(name, '/');
        if (slash == nullptr) {
            return Str_Dup("");
        }
        const size_t top_size = (size_t)(slash - name) + 1;
        if (root == nullptr) {
            root = name;
            root_size = top_size;
        } else if (
            top_size != root_size || memcmp(name, root, root_size) != 0) {
            return Str_Dup("");
        }
    }
    if (root == nullptr) {
        return Str_Dup("");
    }
    char *const out = Mem_Alloc(root_size + 1);
    memcpy(out, root, root_size);
    out[root_size] = '\0';
    return out;
}

RESULT ShellData_Mount(VFS *const files, const char *const path)
{
    FILE_TYPE type;
    if (!IS_OK(File_GetType(path, &type)) || type != FILE_TYPE_FILE) {
        return Vfs_MountDir(files, "", path, 0, false);
    }
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    ARCHIVE *archive;
    MUST(Archive_Open(&g_ZipFormat, &reader, &g_HeapAllocator, &archive));
    char *const root = ShellData_FindRoot(archive);
    Vfs_MountSource(files, "", Archive_ToSource(archive, root), 0);
    Mem_Free(root);
    return OK;
}
