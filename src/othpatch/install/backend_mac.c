#include "othpatch/install.h"
#include "othpatch/install/backend.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

const INSTALL_PLATFORM *Install_GetPlatform(void)
{
    return &g_InstallMacOs;
}

RESULT InstallBackend_SetExecutable(
    const char *const path, const bool executable)
{
    FAIL_IF(
        chmod(path, executable ? 0755 : 0644) != 0,
        "cannot set the permissions of %s: %s", path, strerror(errno));
    return OK;
}
