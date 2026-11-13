// SPDX-License-Identifier: GPL-3.0-only

#include "core/io/file.h"

#include "core/base/string.h"
#include "core/io/backend.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static SDL_EnumerationResult SDLCALL M_AddName(
    void *const userdata, const char *const directory, const char *const name)
{
    FILE_LIST *const list = userdata;
    if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
        const size_t size = strlen(name) + 1;
        char *const copy = Allocator_Alloc(list->allocator, size);
        memcpy(copy, name, size);
        Array_Append(list, copy);
    }
    return SDL_ENUM_CONTINUE;
}

static bool M_IsSeparator(const char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

// Returns the length of the root at the start of path: the separators of
// an absolute path, and on Windows also a drive such as "C:" or the server
// and share of a UNC path such as "//server/share".
static size_t M_RootLength(const char *const path)
{
    size_t length = 0;
#ifdef _WIN32
    if (path[0] != '\0' && path[1] == ':') {
        length = 2;
    } else if (M_IsSeparator(path[0]) && M_IsSeparator(path[1])) {
        length = 2;
        for (int32_t part = 0; part < 2; part++) {
            while (M_IsSeparator(path[length])) {
                length++;
            }
            while (path[length] != '\0' && !M_IsSeparator(path[length])) {
                length++;
            }
        }
    }
#endif
    while (M_IsSeparator(path[length])) {
        length++;
    }
    return length;
}

static bool M_CreateDir(const char *const path)
{
    char *const system_path = IoBackend_SystemPath(path);
    const bool made = SDL_CreateDirectory(system_path);
    Mem_Free(system_path);
    return made;
}

static int M_CompareNames(const void *const a, const void *const b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

bool File_Exists(const char *const path)
{
    char *const system_path = IoBackend_SystemPath(path);
    const bool exists = SDL_GetPathInfo(system_path, nullptr);
    Mem_Free(system_path);
    return exists;
}

RESULT File_GetType(const char *const path, FILE_TYPE *const type)
{
    char *const system_path = IoBackend_SystemPath(path);
    SDL_PathInfo info;
    const bool queried = SDL_GetPathInfo(system_path, &info);
    Mem_Free(system_path);
    FAIL_IF(!queried, "Cannot query %s: %s", path, SDL_GetError());
    switch (info.type) {
    case SDL_PATHTYPE_FILE:
        *type = FILE_TYPE_FILE;
        break;
    case SDL_PATHTYPE_DIRECTORY:
        *type = FILE_TYPE_DIR;
        break;
    default:
        *type = FILE_TYPE_OTHER;
        break;
    }
    return OK;
}

// Creates each missing parent before the directory, because on Windows
// SDL_CreateDirectory cannot create the parents of a path in the extended
// form, such as "\\?\C:\dir".
RESULT File_MakeDir(const char *const path)
{
    char *const parent = Str_Dup(path);
    for (size_t i = M_RootLength(path); parent[i] != '\0'; i++) {
        if (M_IsSeparator(parent[i])) {
            parent[i] = '\0';
            const bool made = File_Exists(parent) || M_CreateDir(parent);
            parent[i] = path[i];
            if (!made) {
                Mem_Free(parent);
                return FAIL("Cannot create %s: %s", path, SDL_GetError());
            }
        }
    }
    Mem_Free(parent);
    FAIL_IF(!M_CreateDir(path), "Cannot create %s: %s", path, SDL_GetError());
    return OK;
}

RESULT File_Rename(const char *const from, const char *const to)
{
    char *const system_from = IoBackend_SystemPath(from);
    char *const system_to = IoBackend_SystemPath(to);
    const bool renamed = SDL_RenamePath(system_from, system_to);
    Mem_Free(system_to);
    Mem_Free(system_from);
    FAIL_IF(!renamed, "Cannot rename %s to %s: %s", from, to, SDL_GetError());
    return OK;
}

RESULT File_Remove(const char *const path)
{
    char *const system_path = IoBackend_SystemPath(path);
    const bool removed =
        !SDL_GetPathInfo(system_path, nullptr) || SDL_RemovePath(system_path);
    Mem_Free(system_path);
    FAIL_IF(!removed, "Cannot remove %s: %s", path, SDL_GetError());
    return OK;
}

RESULT File_ListDir(
    const char *const path, const ALLOCATOR *const allocator,
    FILE_LIST *const list)
{
    Array_Init(list, allocator);
    char *const system_path = IoBackend_SystemPath(path);
    const bool listed = SDL_EnumerateDirectory(system_path, M_AddName, list);
    Mem_Free(system_path);
    if (!listed) {
        const RESULT result = FAIL("Cannot list %s: %s", path, SDL_GetError());
        File_FreeList(list);
        return result;
    }
    if (list->count > 1) {
        qsort(
            list->items, (size_t)list->count, sizeof(list->items[0]),
            M_CompareNames);
    }
    return OK;
}

RESULT File_CreateText(const char *const path, FILE **const out)
{
    char *const system_path = IoBackend_SystemPath(path);
    *out = IoBackend_CreateText(system_path);
    const int error = errno;
    Mem_Free(system_path);
    FAIL_IF(*out == nullptr, "Cannot create %s: %s", path, strerror(error));
    return OK;
}

void File_FreeList(FILE_LIST *const list)
{
    for (int32_t i = 0; i < list->count; i++) {
        char *const name = list->items[i];
        Allocator_Free(list->allocator, name, strlen(name) + 1);
    }
    Array_Free(list);
}
