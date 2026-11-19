#include "fonts.h"

#include "core/archive.h"
#include "core/archive/zip.h"
#include "core/base/array.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/text/font_registry.h"
#include "core/vfs.h"

#include "builtin_files.h"

#include <string.h>

typedef ARRAY(char *) M_NAMES;

static void M_Collect(void *const user, const char *const name)
{
    M_NAMES *const names = user;
    const char *const dot = strrchr(name, '.');
    if (dot != nullptr
        && (Str_EqualIgnoreCase(dot, ".ttf") || Str_EqualIgnoreCase(dot, ".ttc")
            || Str_EqualIgnoreCase(dot, ".otf")
            || Str_EqualIgnoreCase(dot, ".otc"))) {
        Array_Append(names, Str_Dup(name));
    }
}

static RESULT M_Add(VFS *const files, const char *const path)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader));
    const size_t size = Reader_Remaining(&reader);
    const void *const data = Reader_Borrow(&reader, size);
    RESULT result = Reader_Result(&reader);
    if (IS_OK(result)) {
        result = FontRegistry_Add(path, data, size);
    }
    Reader_Close(&reader);
    return result;
}

RESULT ShellFonts_Reset(const bool use_system)
{
    FontRegistry_Reset(use_system);
    READER reader;
    Reader_OpenBuffer(
        &reader, m_BuiltinFiles, sizeof(m_BuiltinFiles), "built-in files");
    ARCHIVE *archive;
    MUST(Archive_Open(&g_ZipFormat, &reader, &g_HeapAllocator, &archive));
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    Vfs_MountSource(files, "", Archive_ToSource(archive), 0);
    ShellFonts_AddDir(files, "fonts");
    Vfs_Destroy(files);
    return OK;
}

void ShellFonts_AddDir(VFS *const files, const char *const dir)
{
    M_NAMES names;
    Array_Init(&names, &g_HeapAllocator);
    Vfs_List(files, dir, M_Collect, &names);
    for (int32_t i = 0; i < names.count; i++) {
        char *const path = Str_Format("%s/%s", dir, names.items[i]);
        Result_Log(M_Add(files, path), LOG_LEVEL_WARNING);
        Mem_Free(path);
        Mem_Free(names.items[i]);
    }
    Array_Free(&names);
}
