#include "th06/files.h"

#include "core/base/memory.h"
#include "core/base/utils.h"
#include "core/vfs.h"

#include "stg/pbg3.h"

static const char *const m_Archives[] = {
    "紅魔郷IN.DAT", "紅魔郷MD.DAT", "紅魔郷ST.DAT",
    "紅魔郷TL.DAT", "紅魔郷CM.DAT", "紅魔郷ED.DAT",
};

static VFS *m_Vfs = nullptr;

static RESULT M_MountArchive(const char *const name, const int32_t priority)
{
    READER file;
    MUST(Vfs_Open(m_Vfs, name, VFS_STREAM, &file));
    VFS_SOURCE *source;
    MUST(Pbg3_Open(&file, name, &g_HeapAllocator, &source));
    if (!source->funcs->contains(source, "ver0102.dat")) {
        source->funcs->close(source);
        return FAIL("%s has no ver0102.dat", name);
    }
    Vfs_MountSource(m_Vfs, source, priority);
    return OK;
}

RESULT Files_Init(const char *const data_dir)
{
    m_Vfs = Vfs_Create(&g_HeapAllocator);
    RESULT result = Vfs_MountDir(m_Vfs, data_dir, 0);
    // All six archives mount at once: only ver0102.dat appears in more
    // than one of them, so a game file never needs a specific archive.
    for (size_t i = 0; IS_OK(result) && i < ARRAY_SIZE(m_Archives); i++) {
        result = M_MountArchive(m_Archives[i], (int32_t)(16 - i));
    }
    if (!IS_OK(result)) {
        Vfs_Destroy(m_Vfs);
        m_Vfs = nullptr;
    }
    return result;
}

void Files_Shutdown(void)
{
    if (m_Vfs != nullptr) {
        Vfs_Destroy(m_Vfs);
        m_Vfs = nullptr;
    }
}

RESULT Files_Load(const char *const path, READER *const out)
{
    return Vfs_Open(m_Vfs, path, VFS_IN_MEMORY, out);
}
