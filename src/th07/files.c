#include "th07/files.h"

#include "shell/game.h"
#include "core/archive.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/patch.h"
#include "core/vfs.h"

#include "stg/pbg4.h"

#include <string.h>

#define M_VERSION_FILE "th07_0100b.ver"
#define M_MUSIC_FILE "thbgm.dat"
#define M_MUSIC_MAGIC "ZWAV"
#define M_MUSIC_VERSION 1
#define M_MUSIC_GAME_VERSION 0x700
#define M_PATCH_PRIORITY 100
#define M_ARCHIVE_PRIORITY 11

const char *const g_FilesArchives[FILES_ARCHIVE_COUNT] = { "th07.dat" };
const char *const g_FilesData[FILES_DATA_COUNT] = { "th07.dat", M_MUSIC_FILE };

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
    MUST(Vfs_Open(Shell_GetFiles(), name, VFS_STREAM, &file));
    ARCHIVE *archive;
    MUST(Archive_Open(&g_Pbg4Format, &file, &g_HeapAllocator, &archive));
    if (Archive_Find(archive, M_VERSION_FILE) == nullptr) {
        Archive_Close(archive);
        return FAIL("%s has no %s", name, M_VERSION_FILE);
    }
    Vfs_MountSource(Shell_GetFiles(), "", Archive_ToSource(archive), priority);
    return OK;
}

// Checks the header of thbgm.dat: "ZWAV", version 1, game version 0x700.
static RESULT M_CheckMusic(void)
{
    READER file;
    MUST(Vfs_Open(Shell_GetFiles(), M_MUSIC_FILE, VFS_STREAM, &file));
    char magic[4];
    Reader_ReadBytes(&file, magic, sizeof(magic));
    const uint32_t version = Reader_ReadU32(&file);
    const uint32_t game_version = Reader_ReadU32(&file);
    Reader_Close(&file);
    MUST(Reader_Result(&file));
    FAIL_IF(
        memcmp(magic, M_MUSIC_MAGIC, sizeof(magic)) != 0
            || version != M_MUSIC_VERSION
            || game_version != M_MUSIC_GAME_VERSION,
        "%s is not the music of th07 1.00", M_MUSIC_FILE);
    return OK;
}

const PATCH_GAME g_PatchGame = {
    .build = "v1.00b",
    .layout = PATCH_LAYOUT_PATHS,
    .classify = M_Classify,
};

RESULT Files_Mount(void)
{
    MUST(M_MountArchive(g_FilesArchives[0], M_ARCHIVE_PRIORITY));
    return M_CheckMusic();
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
