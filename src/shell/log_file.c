#include "log_file.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include <stdio.h>

static FILE *m_File = nullptr;
static char *m_Path = nullptr;

RESULT LogFile_Open(const char *const user_dir, const char *const game_id)
{
    LogFile_Close();
    char *const name = Str_Format("o%s.log", game_id);
    char *const path = Path_Join(&g_HeapAllocator, user_dir, name);
    Mem_Free(name);
    const RESULT result = File_CreateText(path, &m_File);
    if (!IS_OK(result)) {
        Mem_Free(path);
        return result;
    }
    m_Path = path;
    Log_SetFile(m_File);
    return OK;
}

const char *LogFile_GetPath(void)
{
    return m_Path;
}

void LogFile_Close(void)
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
