#include "builtin.h"

#include "core/archive.h"
#include "core/archive/zip.h"
#include "core/vfs.h"

#include <stdint.h>

extern const uint8_t g_ShellBuiltinFiles[];
extern const size_t g_ShellBuiltinFilesSize;

RESULT ShellBuiltin_Mount(VFS *const files)
{
    READER reader;
    Reader_OpenBuffer(
        &reader, g_ShellBuiltinFiles, g_ShellBuiltinFilesSize,
        "built-in files");
    ARCHIVE *archive;
    MUST(Archive_Open(&g_ZipFormat, &reader, &g_HeapAllocator, &archive));
    Vfs_MountSource(files, "", Archive_ToSource(archive, ""), INT32_MIN);
    return OK;
}
