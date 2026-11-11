#include "othpatch/install.h"

#include "othpatch/install/backend.h"
#include "othpatch/zip.h"

#include "core/base/memory.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/reader.h"
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
    const char *const name =
        M_Format(arena, "o%s%s", game, platform->game_suffix);
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

// Returns whether the file at path holds exactly size bytes of data.
static bool M_HasContents(
    const char *const path, const uint8_t *const data, const size_t size)
{
    READER reader;
    if (!Result_Ignore(Reader_OpenFile(&reader, path))) {
        return false;
    }
    bool same = Reader_Size(&reader) == size;
    uint8_t buffer[65536];
    for (size_t done = 0; same && done < size; done += sizeof(buffer)) {
        const size_t part = MIN(size - done, sizeof(buffer));
        Reader_ReadBytes(&reader, buffer, part);
        same =
            !Reader_Failed(&reader) && memcmp(buffer, data + done, part) == 0;
    }
    Reader_Close(&reader);
    Result_Ignore(Reader_Result(&reader));
    return same;
}

static RESULT M_InstallOthpatch(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const uint8_t *const package, const size_t size,
    const ZIP_ENTRY_LIST *const entries, ARENA *const arena,
    bool *const replaced)
{
    const ZIP_ENTRY *const entry = Zip_Find(entries, platform->othpatch);
    FAIL_IF(entry == nullptr, "the package has no %s", platform->othpatch);
    uint8_t *data;
    size_t data_size;
    MUST(Zip_Read(package, size, entry, &data, &data_size));
    const char *const path =
        Path_Join(Arena_Allocator(arena), dir, platform->othpatch);
    RESULT result = OK;
    if (!M_HasContents(path, data, data_size)) {
        const char *const staged = M_Format(arena, "%s.new", path);
        result = M_WriteFile(staged, data, data_size, true);
        if (IS_OK(result)) {
            result = M_Swap(platform, path, arena);
        }
        *replaced = IS_OK(result);
    }
    Mem_Free(data);
    return result;
}

char *Install_GetGamePath(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const char *const game, ARENA *const arena)
{
    const char *const name = platform->bundle
        ? M_Format(
              arena, "o%s%s/Contents/MacOS/o%s", game, platform->game_suffix,
              game)
        : M_Format(arena, "o%s%s", game, platform->game_suffix);
    return Path_Join(Arena_Allocator(arena), dir, name);
}

RESULT Install_Package(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const char *const game, const uint8_t *const package, const size_t size,
    ARENA *const arena, bool *const othpatch_replaced)
{
    *othpatch_replaced = false;
    ZIP_ENTRY_LIST entries;
    MUST(Zip_List(package, size, arena, &entries), "the package");
    FAIL_IF(
        Zip_Find(&entries, platform->othpatch) == nullptr,
        "the package has no %s", platform->othpatch);
    MUST(M_InstallGame(platform, dir, game, package, size, &entries, arena));
    MUST(
        M_InstallOthpatch(
            platform, dir, package, size, &entries, arena, othpatch_replaced),
        "%s", platform->othpatch);
    return OK;
}

void Install_RemoveLeftovers(
    const INSTALL_PLATFORM *const platform, const char *const dir,
    const char *const *const games, const int32_t game_count,
    ARENA *const arena)
{
    for (int32_t i = -1; i < game_count; i++) {
        const char *const name = i < 0
            ? platform->othpatch
            : M_Format(arena, "o%s%s", games[i], platform->game_suffix);
        static const char *const suffixes[] = { ".old", ".new" };
        for (size_t k = 0; k < ARRAY_SIZE(suffixes); k++) {
            const char *const path = Path_Join(
                Arena_Allocator(arena), dir,
                M_Format(arena, "%s%s", name, suffixes[k]));
            Result_Ignore(M_RemoveTree(path, arena));
        }
    }
}
