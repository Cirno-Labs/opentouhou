#pragma once

#include "core/base/result.h"

#include <windows.h>

// Creates a fresh temporary directory and writes its full path to out_dir.
RESULT Workdir_CreateTemp(
    const wchar_t *game_id, wchar_t *out_dir, size_t out_dir_capacity);

// Copies the file at source into dir under the same name.
RESULT Workdir_CopyIn(const wchar_t *dir, const wchar_t *source);

// Copies the file name from dir into out_dir. Reports failure when dir has
// no such file.
RESULT Workdir_CopyOut(
    const wchar_t *dir, const wchar_t *name, const wchar_t *out_dir);

// Removes a directory that Workdir_CreateTemp created, with its content.
void Workdir_RemoveTemp(const wchar_t *dir);
