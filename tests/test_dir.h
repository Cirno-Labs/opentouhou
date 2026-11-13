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

// Removes path and, when it is a directory, everything in it.
static void M_TestDirRemove(const char *const path)
{
    FILE_TYPE type;
    FILE_LIST list;
    if (Result_Ignore(File_GetType(path, &type)) && type == FILE_TYPE_DIR
        && Result_Ignore(File_ListDir(path, &g_HeapAllocator, &list))) {
        for (int32_t i = 0; i < list.count; i++) {
            char *const child = Str_Format("%s/%s", path, list.items[i]);
            M_TestDirRemove(child);
            Mem_Free(child);
        }
        File_FreeList(&list);
    }
    Result_Ignore(File_Remove(path));
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
    M_TestDirRemove(directory);
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
