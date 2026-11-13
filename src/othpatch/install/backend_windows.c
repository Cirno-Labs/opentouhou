// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/install.h"
#include "othpatch/install/backend.h"

const INSTALL_PLATFORM *Install_GetPlatform(void)
{
    return &g_InstallWindows;
}

// Windows runs a file by its name and has no executable bit.
RESULT InstallBackend_SetExecutable(
    const char *const path, const bool executable)
{
    return OK;
}
