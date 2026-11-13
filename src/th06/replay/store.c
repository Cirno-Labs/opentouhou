// SPDX-License-Identifier: GPL-3.0-only

#include "th06/replay/file.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include <string.h>

typedef struct {
    char *path;
    uint8_t *data;
    size_t size;
} M_FILE;

static M_FILE *m_Files;
static int32_t m_Count;

static M_FILE *M_Find(const char *const path)
{
    for (int32_t i = 0; i < m_Count; i++) {
        if (strcmp(m_Files[i].path, path) == 0) {
            return &m_Files[i];
        }
    }
    return nullptr;
}

static void M_Free(M_FILE *const file)
{
    Mem_Free(file->path);
    Mem_Free(file->data);
}

void Replay_AddFile(
    const char *const path, const void *const data, const size_t size)
{
    M_FILE *file = M_Find(path);
    if (file == nullptr) {
        m_Files = Mem_ReallocArr(m_Files, (size_t)m_Count + 1, sizeof(M_FILE));
        file = &m_Files[m_Count++];
        file->path = Str_Dup(path);
    } else {
        Mem_Free(file->data);
    }
    file->data = Mem_Dup(data, size);
    file->size = size;
}

void Replay_RemoveFile(const char *const path)
{
    M_FILE *const file = M_Find(path);
    if (file == nullptr) {
        return;
    }
    M_Free(file);
    *file = m_Files[--m_Count];
}

void Replay_ClearFiles(void)
{
    for (int32_t i = 0; i < m_Count; i++) {
        M_Free(&m_Files[i]);
    }
    Mem_Free(m_Files);
    m_Files = nullptr;
    m_Count = 0;
}

const uint8_t *Replay_FindFile(const char *const path, size_t *const size)
{
    const M_FILE *const file = M_Find(path);
    if (file == nullptr) {
        return nullptr;
    }
    *size = file->size;
    return file->data;
}
