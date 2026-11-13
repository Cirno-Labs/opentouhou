// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/result.h"

#include <windows.h>

// Patches the main module's file imports so that writes under game_dir go
// to out_dir and writes elsewhere fail. Reads take out_dir's file when it
// has one. A change into a directory that only out_dir has succeeds. Both
// are full paths without a trailing backslash. Reports failure when an
// import is missing.
RESULT Overlay_Install(const wchar_t *game_dir, const wchar_t *out_dir);
