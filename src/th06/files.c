#include "th06/files.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/patch.h"
#include "core/vfs.h"

#include "stg/pbg3.h"

#include <string.h>

// Mounts the patches above the six archives, which mount at 11 to 16.
#define M_PATCH_PRIORITY 100

const char *const g_FilesArchives[FILES_ARCHIVE_COUNT] = {
    "紅魔郷IN.DAT", "紅魔郷MD.DAT", "紅魔郷ST.DAT",
    "紅魔郷TL.DAT", "紅魔郷CM.DAT", "紅魔郷ED.DAT",
};

static VFS *m_Vfs = nullptr;
static const PATCH_STACK *m_Patches = nullptr;

// Returns whether name is the ANM file of a menu, a screen or the text
// rows, which only change what is drawn.
static bool M_IsInterfaceAnm(const char *const name)
{
    static const char *const prefixes[] = {
        "title", "select", "slpl", "result", "music", "staff",
    };
    if (strcmp(name, "front.anm") == 0 || strcmp(name, "text.anm") == 0
        || strcmp(name, "replay00.anm") == 0) {
        return true;
    }
    for (size_t i = 0; i < ARRAY_SIZE(prefixes); i++) {
        if (Str_StartsWith(name, prefixes[i])) {
            return true;
        }
    }
    return false;
}

static PATCH_FILE_KIND M_Classify(const char *const name)
{
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
    VFS_SOURCE *source;
    MUST(Pbg3_Open(&file, name, &g_HeapAllocator, &source));
    if (!source->funcs->contains(source, "ver0102.dat")) {
        source->funcs->close(source);
        return FAIL("%s has no ver0102.dat", name);
    }
    Vfs_MountSource(m_Vfs, source, priority);
    return OK;
}

const PATCH_GAME g_PatchGame = {
    .build = "v1.02h",
    .layout = PATCH_LAYOUT_FLAT,
    .classify = M_Classify,
};

RESULT Files_Init(const char *const data_dir)
{
    m_Vfs = Vfs_Create(&g_HeapAllocator);
    RESULT result = Vfs_MountDir(m_Vfs, data_dir, 0);
    // All six archives mount at once: only ver0102.dat appears in more
    // than one of them, so a game file never needs a specific archive.
    for (size_t i = 0; IS_OK(result) && i < ARRAY_SIZE(g_FilesArchives); i++) {
        result = M_MountArchive(g_FilesArchives[i], (int32_t)(16 - i));
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
