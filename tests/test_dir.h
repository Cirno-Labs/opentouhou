// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Gives each test case an empty directory of its own. The directories are
// made in the directory that meson passes in TEST_DIR, which is inside the
// build directory, so that no test touches a file anywhere else.

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_EnumerationResult SDLCALL M_TestDirRemove(
    void *const userdata, const char *const directory, const char *const name)
{
    char *const path = Str_Format("%s%s", directory, name);
    SDL_PathInfo info;
    if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
        SDL_EnumerateDirectory(path, M_TestDirRemove, nullptr);
    }
    SDL_RemovePath(path);
    Mem_Free(path);
    return SDL_ENUM_CONTINUE;
}

static bool M_TestDirIsInTmp(const char *const root)
{
    char *const path = Str_Dup(root);
    for (char *c = path; *c != '\0'; c++) {
        if (*c == '\\') {
            *c = '/';
        }
    }
    const bool in_tmp = strstr(path, "/tests/tmp/") != nullptr;
    Mem_Free(path);
    return in_tmp;
}

// Returns the path of an empty directory for the test case name. Mem_Free
// releases the path.
static char *TestDir_Prepare(const char *const name)
{
    const char *const root = getenv("TEST_DIR");
    if (root == nullptr || !M_TestDirIsInTmp(root)) {
        fprintf(stderr, "TEST_DIR must name a directory in tests/tmp/\n");
        abort();
    }
    char *const directory = Str_Format("%s/%s", root, name);
    SDL_EnumerateDirectory(directory, M_TestDirRemove, nullptr);
    if (!Result_Log(File_MakeDir(directory), LOG_LEVEL_ERROR)) {
        abort();
    }
    return directory;
}

// Returns directory/name. Mem_Free releases the path.
[[maybe_unused]] static char *TestDir_Join(
    const char *const directory, const char *const name)
{
    return Str_Format("%s/%s", directory, name);
}

// Returns the file:// URL of the absolute path: "file://" and the path, or
// "file:///" and the path for a Windows path with a drive, such as
// "C:/dir". Mem_Free releases the URL.
[[maybe_unused]] static char *TestDir_FileUrl(const char *const path)
{
    return Str_Format("file://%s%s", path[0] == '/' ? "" : "/", path);
}
