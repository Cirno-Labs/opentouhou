#include "stg/replay_files.h"

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

static void M_FreeData(M_FILE *const file)
{
    if (file->data != nullptr) {
        Allocator_Free(&g_HeapAllocator, file->data, file->size);
    }
}

static void M_Free(M_FILE *const file)
{
    Mem_Free(file->path);
    M_FreeData(file);
}

void ReplayFiles_Add(
    const char *const path, uint8_t *const data, const size_t size)
{
    M_FILE *file = M_Find(path);
    if (file == nullptr) {
        m_Files = Mem_ReallocArr(m_Files, (size_t)m_Count + 1, sizeof(M_FILE));
        file = &m_Files[m_Count++];
        file->path = Str_Dup(path);
    } else {
        M_FreeData(file);
    }
    file->data = data;
    file->size = size;
}

void ReplayFiles_Remove(const char *const path)
{
    M_FILE *const file = M_Find(path);
    if (file == nullptr) {
        return;
    }
    M_Free(file);
    *file = m_Files[--m_Count];
}

void ReplayFiles_Clear(void)
{
    for (int32_t i = 0; i < m_Count; i++) {
        M_Free(&m_Files[i]);
    }
    Mem_Free(m_Files);
    m_Files = nullptr;
    m_Count = 0;
}

const uint8_t *ReplayFiles_Find(const char *const path, size_t *const size)
{
    const M_FILE *const file = M_Find(path);
    if (file == nullptr) {
        return nullptr;
    }
    *size = file->size;
    return file->data;
}
