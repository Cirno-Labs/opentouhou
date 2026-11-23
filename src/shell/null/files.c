#include "shell/null.h"

#include "core/vfs.h"
#include "shell/data.h"
#include "shell/game.h"

static VFS *m_Files;
static VFS *m_UserFiles;

static VFS *M_Get(VFS **const files)
{
    if (*files == nullptr) {
        *files = Vfs_Create(&g_HeapAllocator);
    }
    return *files;
}

RESULT NullShell_OpenFiles(
    const char *const data_dir, const char *const user_dir)
{
    NullShell_CloseFiles();
    if (data_dir != nullptr) {
        MUST(ShellData_Mount(M_Get(&m_Files), data_dir, nullptr));
    }
    if (user_dir != nullptr) {
        MUST(Vfs_MountDir(M_Get(&m_UserFiles), "", user_dir, 0, true));
    }
    return OK;
}

void NullShell_CloseFiles(void)
{
    if (m_Files != nullptr) {
        Vfs_Destroy(m_Files);
    }
    if (m_UserFiles != nullptr) {
        Vfs_Destroy(m_UserFiles);
    }
    m_Files = nullptr;
    m_UserFiles = nullptr;
}

VFS *Shell_GetFiles(void)
{
    return M_Get(&m_Files);
}

VFS *Shell_GetUserFiles(void)
{
    return M_Get(&m_UserFiles);
}

const PATCH_STACK *Shell_GetPatches(void)
{
    return nullptr;
}
