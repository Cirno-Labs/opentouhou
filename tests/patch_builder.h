// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Builds patches folders for the core/patch tests, in the test directories
// of test_dir.h.

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include "test.h"
#include "test_dir.h"

#include <string.h>

// Writes content to root/path, and creates the directories above it.
[[maybe_unused]] static void PatchBuilder_Write(
    const char *const root, const char *const path, const char *const content)
{
    char *const full = Str_Format("%s/%s", root, path);
    char *const slash = strrchr(full, '/');
    *slash = '\0';
    if (!Result_Log(File_MakeDir(full), LOG_LEVEL_ERROR)) {
        TEST_FAIL("cannot create %s", full);
    }
    *slash = '/';
    TestDir_Write(full, content, strlen(content));
    Mem_Free(full);
}

// Writes root/<id>/patch.js with the given text.
[[maybe_unused]] static void PatchBuilder_AddPatch(
    const char *const root, const char *const id, const char *const patch_js)
{
    char *const path = Str_Format("%s/patch.js", id);
    PatchBuilder_Write(root, path, patch_js);
    Mem_Free(path);
}
