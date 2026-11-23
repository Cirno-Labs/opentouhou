#include "shell/data.h"

#include "core/archive/hdi.h"
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

#define M_ZIP_SIGNATURE 0x4B50

static bool M_IsImage(const char *const name)
{
    const size_t size = strlen(name);
    return size >= 4 && Str_EqualIgnoreCase(name + size - 4, ".hdi");
}

// Returns the directory of the shallowest entry whose last component is
// marker, or nullptr when there is none.
static char *M_FindMarker(
    const ARCHIVE *const archive, const char *const marker)
{
    const char *found = nullptr;
    size_t found_size = 0;
    for (int32_t i = 0; i < Archive_GetCount(archive); i++) {
        const char *const name =
            ArchiveEntry_GetName(Archive_GetEntry(archive, i));
        const char *const slash = strrchr(name, '/');
        const char *const last = slash != nullptr ? slash + 1 : name;
        const size_t size = (size_t)(last - name);
        if (Str_EqualIgnoreCase(last, marker)
            && (found == nullptr || size < found_size)) {
            found = name;
            found_size = size;
        }
    }
    if (found == nullptr) {
        return nullptr;
    }
    char *const out = Mem_Alloc(found_size + 1);
    memcpy(out, found, found_size);
    out[found_size] = '\0';
    return out;
}

// Replaces *archive, a zip, with the .hdi that it holds, when it holds one.
static RESULT M_OpenHeldImage(const char *const path, ARCHIVE **const archive)
{
    const ARCHIVE_ENTRY *image = nullptr;
    for (int32_t i = 0; i < Archive_GetCount(*archive); i++) {
        const ARCHIVE_ENTRY *const entry = Archive_GetEntry(*archive, i);
        if (!M_IsImage(ArchiveEntry_GetName(entry))) {
            continue;
        }
        FAIL_IF(
            image != nullptr, "%s holds more than one .hdi: %s and %s", path,
            ArchiveEntry_GetName(image), ArchiveEntry_GetName(entry));
        image = entry;
    }
    if (image == nullptr) {
        return OK;
    }
    uint8_t *data;
    MUST(Archive_Read(*archive, image, &data));
    char *const name = Str_Format("%s/%s", path, ArchiveEntry_GetName(image));
    READER reader;
    Reader_OpenOwnedBuffer(
        &reader, data, ArchiveEntry_GetSize(image), &g_HeapAllocator, name);
    Mem_Free(name);
    Archive_Close(*archive);
    *archive = nullptr;
    return Archive_Open(&g_HdiFormat, &reader, &g_HeapAllocator, archive);
}

static RESULT M_OpenFile(const char *const path, ARCHIVE **const out)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const bool zip =
        Reader_Size(&reader) >= 2 && Reader_ReadU16(&reader) == M_ZIP_SIGNATURE;
    Reader_Seek(&reader, 0);
    MUST(Archive_Open(
        zip ? &g_ZipFormat : &g_HdiFormat, &reader, &g_HeapAllocator, out));
    const RESULT result = zip ? M_OpenHeldImage(path, out) : OK;
    if (!IS_OK(result) && *out != nullptr) {
        Archive_Close(*out);
    }
    return result;
}

RESULT ShellData_Mount(
    VFS *const files, const char *const path, const char *const marker)
{
    FILE_TYPE type;
    if (!IS_OK(File_GetType(path, &type)) || type != FILE_TYPE_FILE) {
        return Vfs_MountDir(files, "", path, 0, false);
    }
    ARCHIVE *archive;
    MUST(M_OpenFile(path, &archive));
    char *root = marker != nullptr ? M_FindMarker(archive, marker) : nullptr;
    if (root == nullptr) {
        root = ShellData_FindRoot(archive);
    }
    Vfs_MountSource(files, "", Archive_ToSource(archive, root), 0);
    Mem_Free(root);
    return OK;
}
