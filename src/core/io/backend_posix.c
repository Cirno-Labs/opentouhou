// SPDX-License-Identifier: GPL-3.0-only

#include "core/io/backend.h"

#include "core/base/string.h"

char *IoBackend_SystemPath(const char *const path)
{
    return Str_Dup(path);
}

FILE *IoBackend_CreateText(const char *const system_path)
{
    return fopen(system_path, "w");
}
