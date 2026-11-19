#pragma once

#include "backend.h"

#include "core/base/result.h"

#include <windows.h>

// Lays work_dir over game_dir in the main module's file imports, hiding
// the private paths and serving the staged files.
RESULT Overlay_Install(
    const wchar_t *game_dir, const wchar_t *work_dir,
    const char *const *private_paths, size_t count, const STAGED_FILE *staged,
    size_t staged_count);
