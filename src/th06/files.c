#include "th06/files.h"

#include "stg/pbg3.h"

#include "shell/game.h"
#include "core/archive.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/patch.h"
#include "core/vfs.h"

#include <string.h>

// Mounts the patches above the six archives, which mount at 11 to 16.
#define M_PATCH_PRIORITY 100

const char *const g_FilesArchives[FILES_ARCHIVE_COUNT] = {
    "紅魔郷IN.DAT", "紅魔郷MD.DAT", "紅魔郷ST.DAT",
    "紅魔郷TL.DAT", "紅魔郷CM.DAT", "紅魔郷ED.DAT",
};

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
    MUST(Vfs_Open(Shell_GetFiles(), name, VFS_STREAM, &file));
    ARCHIVE *archive;
    MUST(Archive_Open(&g_Pbg3Format, &file, &g_HeapAllocator, &archive));
    if (Archive_Find(archive, "ver0102.dat") == nullptr) {
        Archive_Close(archive);
        return FAIL("%s has no ver0102.dat", name);
    }
    Vfs_MountSource(Shell_GetFiles(), "", Archive_ToSource(archive), priority);
    return OK;
}

const PATCH_GAME g_PatchGame = {
    .build = "v1.02h",
    .layout = PATCH_LAYOUT_FLAT,
    .classify = M_Classify,
};

RESULT Files_Mount(void)
{
    // All six archives mount at once: only ver0102.dat appears in more
    // than one of them, so a game file never needs a specific archive.
    for (size_t i = 0; i < ARRAY_SIZE(g_FilesArchives); i++) {
        MUST(M_MountArchive(g_FilesArchives[i], (int32_t)(16 - i)));
    }
    return OK;
}

void Files_SetPatches(const PATCH_STACK *const patches)
{
    m_Patches = patches;
    if (patches != nullptr) {
        Vfs_MountSource(
            Shell_GetFiles(), "", Patch_CreateSource(patches, &g_HeapAllocator),
            M_PATCH_PRIORITY);
    }
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
