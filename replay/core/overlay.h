#pragma once

#include "core/base/result.h"

#include <windows.h>

// Patches the main module's file imports so that writes under game_dir go
// to out_dir. Both are full paths without a trailing backslash. Reports
// failure when an import is missing.
RESULT Overlay_Install(const wchar_t *game_dir, const wchar_t *out_dir);
