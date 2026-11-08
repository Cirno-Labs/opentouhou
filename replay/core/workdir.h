#pragma once

#include "core/base/result.h"

#include <windows.h>

// Creates a fresh temporary directory and writes its full path to out_dir.
RESULT Workdir_CreateTemp(
    const wchar_t *game_id, wchar_t *out_dir, size_t out_dir_capacity);

// Removes a directory that Workdir_CreateTemp created, with its content.
void Workdir_RemoveTemp(const wchar_t *dir);
