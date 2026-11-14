#include "th07/files.h"

#include "core/archive.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/patch.h"
#include "core/vfs.h"

#include "stg/pbg4.h"

#include <string.h>

#define M_VERSION_FILE "th07_0100b.ver"
#define M_PATCH_PRIORITY 100
#define M_ARCHIVE_PRIORITY 16

const char *const g_FilesArchives[FILES_ARCHIVE_COUNT] = { "th07.dat" };

static VFS *m_Vfs = nullptr;
static const PATCH_STACK *m_Patches = nullptr;

// Returns whether name is the ANM file of a menu, a screen or the text
// rows, which only change what is drawn.
static bool M_IsInterfaceAnm(const char *const name)
{
    static const char *const prefixes[] = { "title", "result", "music",
                                            "staff" };
    if (strcmp(name, "front.anm") == 0 || strcmp(name, "text.anm") == 0) {
        return true;
    }
    for (size_t i = 0; i < ARRAY_SIZE(prefixes); i++) {
        if (Str_StartsWith(name, prefixes[i])) {
            return true;
        }
    }
    return false;
}

static PATCH_FILE_KIND M_Classify(const char *const path)
{
    const char *const slash = strrchr(path, '/');
    const char *const name = slash != nullptr ? slash + 1 : path;
    if (Str_EndsWith(name, ".ecl") || Str_EndsWith(name, ".std")
        || Str_EndsWith(name, ".sht")
        || (Str_StartsWith(name, "msg") && Str_EndsWith(name, ".dat"))) {
        return PATCH_FILE_GAMEPLAY;
    }
    if (Str_EndsWith(name, ".anm") && !M_IsInterfaceAnm(name)) {
        return PATCH_FILE_IGNORED;
    }
    return PATCH_FILE_DATA;
}

static RESULT M_MountArchive(const char *const name, const int32_t priority)
{
    READER file;
    MUST(Vfs_Open(m_Vfs, name, VFS_STREAM, &file));
    ARCHIVE *archive;
    MUST(Archive_Open(&g_Pbg4Format, &file, &g_HeapAllocator, &archive));
    if (Archive_Find(archive, M_VERSION_FILE) == nullptr) {
        Archive_Close(archive);
        return FAIL("%s has no %s", name, M_VERSION_FILE);
    }
    Vfs_MountSource(m_Vfs, Archive_ToSource(archive), priority);
    return OK;
}

const PATCH_GAME g_PatchGame = {
    .build = "v1.00b",
    .layout = PATCH_LAYOUT_PATHS,
    .classify = M_Classify,
};

RESULT Files_Init(const char *const data_dir)
{
    m_Vfs = Vfs_Create(&g_HeapAllocator);
    RESULT result = Vfs_MountDir(m_Vfs, data_dir, 0);
    if (IS_OK(result)) {
        result = M_MountArchive(g_FilesArchives[0], M_ARCHIVE_PRIORITY);
    }
    if (!IS_OK(result)) {
        Vfs_Destroy(m_Vfs);
        m_Vfs = nullptr;
    }
    return result;
}

void Files_SetPatches(const PATCH_STACK *const patches)
{
    m_Patches = patches;
    if (patches != nullptr) {
        Vfs_MountSource(
            m_Vfs, Patch_CreateSource(patches, &g_HeapAllocator),
            M_PATCH_PRIORITY);
    }
}

void Files_Shutdown(void)
{
    if (m_Vfs != nullptr) {
        Vfs_Destroy(m_Vfs);
        m_Vfs = nullptr;
    }
    m_Patches = nullptr;
}

RESULT Files_Load(const char *const path, READER *const out)
{
    return Vfs_Open(m_Vfs, path, VFS_IN_MEMORY, out);
}

RESULT Files_Stream(const char *const path, READER *const out)
{
    return Vfs_Open(m_Vfs, path, VFS_STREAM, out);
}

JSON_VALUE *Files_LoadDiff(
    const char *const path, const ALLOCATOR *const allocator)
{
    return Patch_LoadDiff(m_Patches, path, allocator);
}

bool Files_IsPatched(const char *const path)
{
    return Patch_Touches(m_Patches, path);
}
