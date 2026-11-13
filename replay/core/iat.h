// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <windows.h>

#include <stdint.h>

// Replaces module's import of name from dll with hook and stores the old
// address in *original. Returns whether module imports it.
bool Iat_Patch(
    HMODULE module, const char *dll, const char *name, uintptr_t hook,
    uintptr_t *original);
