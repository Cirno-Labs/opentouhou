// SPDX-License-Identifier: GPL-3.0-only

#include "core/io/file.h"

#include <SDL3/SDL.h>
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

static int M_CompareNames(const void *const a, const void *const b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

bool File_Exists(const char *const path)
{
    return SDL_GetPathInfo(path, nullptr);
}

RESULT File_MakeDir(const char *const path)
{
    FAIL_IF(
        !SDL_CreateDirectory(path), "Cannot create %s: %s", path,
        SDL_GetError());
    return OK;
}

RESULT File_Rename(const char *const from, const char *const to)
{
    FAIL_IF(
        !SDL_RenamePath(from, to), "Cannot rename %s to %s: %s", from, to,
        SDL_GetError());
    return OK;
}

RESULT File_Remove(const char *const path)
{
    FAIL_IF(
        File_Exists(path) && !SDL_RemovePath(path), "Cannot remove %s: %s",
        path, SDL_GetError());
    return OK;
}

RESULT File_ListDir(
    const char *const path, const ALLOCATOR *const allocator,
    FILE_LIST *const list)
{
    Array_Init(list, allocator);
    if (!SDL_EnumerateDirectory(path, M_AddName, list)) {
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

void File_FreeList(FILE_LIST *const list)
{
    for (int32_t i = 0; i < list->count; i++) {
        char *const name = list->items[i];
        Allocator_Free(list->allocator, name, strlen(name) + 1);
    }
    Array_Free(list);
}
