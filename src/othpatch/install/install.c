// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/install.h"

#include "othpatch/game.h"
#include "othpatch/install/backend.h"
#include "othpatch/zip.h"

#include "core/base/memory.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define M_FILE_TYPE_MASK 0170000
#define M_FILE_TYPE_LINK 0120000

const INSTALL_PLATFORM g_InstallLinux = {
    .name = "Linux",
    .game_suffix = ".AppImage",
    .othpatch = "othpatch",
};

const INSTALL_PLATFORM g_InstallWindows = {
    .name = "Windows",
    .game_suffix = ".exe",
    .othpatch = "othpatch.exe",
    .move_aside = true,
};

const INSTALL_PLATFORM g_InstallMacOs = {
    .name = "macOS",
    .game_suffix = ".app",
    .othpatch = "othpatch",
    .bundle = true,
    .move_aside = true,
};

PRINTF_FORMAT(2, 3)
static char *M_Format(ARENA *const arena, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    const int size = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    char *const text = Arena_Alloc(arena, (size_t)size + 1);
    vsnprintf(text, (size_t)size + 1, fmt, args);
    va_end(args);
    return text;
}

// Returns the name of the game in a folder, such as "oth06.AppImage".
static char *M_GameName(
    const INSTALL_PLATFORM *const platform, const char *const game,
    ARENA *const arena)
{
    char product[GAME_NAME_SIZE];
    Game_GetProductName(game, product);
    return M_Format(arena, "%s%s", product, platform->game_suffix);
}

// Removes the file or the folder at path with everything in it.
static RESULT M_RemoveTree(const char *const path, ARENA *const arena)
{
    if (!File_Exists(path)) {
        return OK;
    }
    FILE_LIST names;
    if (Result_Ignore(File_ListDir(path, Arena_Allocator(arena), &names))) {
        for (int32_t i = 0; i < names.count; i++) {
            MUST(M_RemoveTree(
                Path_Join(Arena_Allocator(arena), path, names.items[i]),
                arena));
        }
    }
    return File_Remove(path);
}

static RESULT M_WriteFile(
    const char *const path, const uint8_t *const data, const size_t size,
    const bool executable)
{
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    Writer_WriteBytes(&writer, data, size);
    MUST(Writer_Close(&writer));
    return InstallBackend_SetExecutable(path, executable);
}

// Puts the file or the folder staged at "<path>.new" in the place of path.
static RESULT M_Swap(
    const INSTALL_PLATFORM *const platform, const char *const path,
    ARENA *const arena)
{
    const char *const staged = M_Format(arena, "%s.new", path);
    if (!platform->move_aside || !File_Exists(path)) {
        return File_Rename(staged, path);
    }
    const char *const old = M_Format(arena, "%s.old", path);
    MUST(M_RemoveTree(old, arena));
    MUST(File_Rename(path, old));
    const RESULT placed = File_Rename(staged, path);
    if (!IS_OK(placed)) {
        Result_Ignore(File_Rename(old, path));
        return placed;
    }
    Result_Ignore(M_RemoveTree(old, arena));
    return OK;
}

static RESULT M_Unpack(
    const uint8_t *const package, const size_t size,
    const ZIP_ENTRY *const entry, const char *const path, const bool executable)
{
    uint8_t *data;
    size_t data_size;
    MUST(Zip_Read(package, size, entry, &data, &data_size));
    const RESULT result = M_WriteFile(path, data, data_size, executable);
    Mem_Free(data);
    return result;
}

static bool M_IsSafePath(const char *const path)
{
    if (path[0] == '\0' || strchr(path, '\\') != nullptr
        || strchr(path, ':') != nullptr) {
        return false;
    }
    const char *part = path;
    while (true) {
        const size_t length = strcspn(part, "/");
        const bool dots = (length == 1 && part[0] == '.')
            || (length == 2 && part[0] == '.' && part[1] == '.');
        if (dots || (length == 0 && part[length] != '\0')) {
            return false;
        }
        if (part[length] == '\0') {
            return true;
        }
        part += length + 1;
    }
}

// Writes every entry below the folder prefix of the archive into the folder
// staged.
static RESULT M_UnpackBundle(
    const uint8_t *const package, const size_t size,
    const ZIP_ENTRY_LIST *const entries, const char *const prefix,
    const char *const staged, ARENA *const arena)
{
    MUST(M_RemoveTree(staged, arena));
    MUST(File_MakeDir(staged));
    int32_t count = 0;
    for (int32_t i = 0; i < entries->count; i++) {
        const ZIP_ENTRY *const entry = &entries->items[i];
        if (strncmp(entry->name, prefix, strlen(prefix)) != 0) {
            continue;
        }
        const char *const relative = entry->name + strlen(prefix);
        if (relative[0] == '\0') {
            continue;
        }
        FAIL_IF(!M_IsSafePath(relative), "%s is not a safe path", entry->name);
        FAIL_IF(
            (entry->mode & M_FILE_TYPE_MASK) == M_FILE_TYPE_LINK,
            "%s is a symbolic link, which is not supported", entry->name);
        char *const path = Path_Join(Arena_Allocator(arena), staged, relative);
        const size_t length = strlen(path);
        if (path[length - 1] == '/') {
            path[length - 1] = '\0';
            MUST(File_MakeDir(path));
            continue;
        }
        char *const parent = M_Format(arena, "%s", path);
        *strrchr(parent, '/') = '\0';
        MUST(File_MakeDir(parent));
        MUST(M_Unpack(package, size, entry, path, (entry->mode & 0111) != 0));
        count++;
    }
    FAIL_IF(count == 0, "the package has no %s", prefix);
    return OK;
}

static RESULT M_InstallGame(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const char *const game, const uint8_t *const package, const size_t size,
    const ZIP_ENTRY_LIST *const entries, ARENA *const arena)
{
    const char *const name = M_GameName(platform, game, arena);
    const char *const path = Path_Join(Arena_Allocator(arena), dir, name);
    const char *const staged = M_Format(arena, "%s.new", path);
    if (platform->bundle) {
        const char *const prefix = M_Format(arena, "%s/", name);
        MUST(M_UnpackBundle(package, size, entries, prefix, staged, arena));
    } else {
        const ZIP_ENTRY *const entry = Zip_Find(entries, name);
        FAIL_IF(entry == nullptr, "the package has no %s", name);
        MUST(M_Unpack(package, size, entry, staged, true));
    }
    return M_Swap(platform, path, arena);
}

// Writes data, the bytes of othpatch, beside the othpatch in dir as
// "<name>.new" and returns the path of that file.
static RESULT M_StageOthpatch(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const uint8_t *const data, const size_t size, ARENA *const arena,
    const char **const out)
{
    const char *const path =
        Path_Join(Arena_Allocator(arena), dir, platform->othpatch);
    *out = M_Format(arena, "%s.new", path);
    return M_WriteFile(*out, data, size, true);
}

// Installs the othpatch of a package when its version is newer than
// *current. A package othpatch that gives no version is skipped.
static RESULT M_InstallNewerOthpatch(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const uint8_t *const package, const size_t size,
    const ZIP_ENTRY *const entry, RELEASE_VERSION *const current,
    ARENA *const arena, bool *const replaced)
{
    uint8_t *data;
    size_t data_size;
    MUST(Zip_Read(package, size, entry, &data, &data_size));
    const char *staged;
    const RESULT written =
        M_StageOthpatch(platform, dir, data, data_size, arena, &staged);
    Mem_Free(data);
    MUST(written);
    char text[GAME_VERSION_SIZE];
    RELEASE_VERSION version;
    if (Result_Ignore(Game_GetVersion(staged, text))
        && Release_ParseVersion(text, &version)
        && Release_CompareVersions(version, *current) > 0) {
        MUST(M_Swap(
            platform,
            Path_Join(Arena_Allocator(arena), dir, platform->othpatch), arena));
        *current = version;
        *replaced = true;
        return OK;
    }
    return File_Remove(staged);
}

char *Install_GetGamePath(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const char *const game, ARENA *const arena)
{
    const char *const name = M_GameName(platform, game, arena);
    if (!platform->bundle) {
        return Path_Join(Arena_Allocator(arena), dir, name);
    }
    char product[GAME_NAME_SIZE];
    Game_GetProductName(game, product);
    return Path_Join(
        Arena_Allocator(arena), dir,
        M_Format(arena, "%s/Contents/MacOS/%s", name, product));
}

RESULT Install_Package(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const char *const game, const uint8_t *const package, const size_t size,
    RELEASE_VERSION *const othpatch_version, ARENA *const arena,
    bool *const othpatch_replaced)
{
    *othpatch_replaced = false;
    ZIP_ENTRY_LIST entries;
    MUST(Zip_List(package, size, arena, &entries), "the package");
    const ZIP_ENTRY *const othpatch = Zip_Find(&entries, platform->othpatch);
    MUST(M_InstallGame(platform, dir, game, package, size, &entries, arena));
    if (othpatch != nullptr && othpatch_version != nullptr) {
        MUST(
            M_InstallNewerOthpatch(
                platform, dir, package, size, othpatch, othpatch_version, arena,
                othpatch_replaced),
            "%s", platform->othpatch);
    }
    return OK;
}

RESULT Install_Othpatch(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const uint8_t *const data, const size_t size, ARENA *const arena)
{
    const char *staged;
    MUST(M_StageOthpatch(platform, dir, data, size, arena, &staged));
    return M_Swap(
        platform, Path_Join(Arena_Allocator(arena), dir, platform->othpatch),
        arena);
}

void Install_RemoveLeftovers(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const char *const *const games, const int32_t game_count,
    ARENA *const arena)
{
    for (int32_t i = -1; i < game_count; i++) {
        const char *const name =
            i < 0 ? platform->othpatch : M_GameName(platform, games[i], arena);
        static const char *const suffixes[] = { ".old", ".new" };
        for (size_t k = 0; k < ARRAY_SIZE(suffixes); k++) {
            const char *const path = Path_Join(
                Arena_Allocator(arena), dir,
                M_Format(arena, "%s%s", name, suffixes[k]));
            Result_Ignore(M_RemoveTree(path, arena));
        }
    }
}
