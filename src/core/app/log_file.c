// SPDX-License-Identifier: GPL-3.0-only

#include "log_file.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/path.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

static FILE *m_File = nullptr;
static char *m_Path = nullptr;

// Opens path for writing as text. fopen reads a path in the ANSI code page
// on Windows, so the path goes to _wfopen as UTF-16 there.
static FILE *M_Open(const char *const path)
{
#ifdef _WIN32
    wchar_t *const wide = (wchar_t *)SDL_iconv_string(
        "UTF-16LE", "UTF-8", path, strlen(path) + 1);
    if (wide == nullptr) {
        errno = EINVAL;
        return nullptr;
    }
    FILE *const file = _wfopen(wide, L"w");
    SDL_free(wide);
    return file;
#else
    return fopen(path, "w");
#endif
}

RESULT AppLogFile_Open(const char *const user_dir, const char *const game_id)
{
    AppLogFile_Close();
    char *const name = Str_Format("o%s.log", game_id);
    char *const path = Path_Join(&g_HeapAllocator, user_dir, name);
    Mem_Free(name);
    FILE *const file = M_Open(path);
    if (file == nullptr) {
        const RESULT result =
            FAIL("Cannot create %s: %s", path, strerror(errno));
        Mem_Free(path);
        return result;
    }
    m_File = file;
    m_Path = path;
    Log_SetFile(file);
    return OK;
}

const char *AppLogFile_GetPath(void)
{
    return m_Path;
}

void AppLogFile_Close(void)
{
    if (m_File == nullptr) {
        return;
    }
    Log_SetFile(nullptr);
    fclose(m_File);
    m_File = nullptr;
    Mem_Free(m_Path);
    m_Path = nullptr;
}
