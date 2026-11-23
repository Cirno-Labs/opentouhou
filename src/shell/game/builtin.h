#pragma once

#include "core/base/result.h"
#include "core/vfs/types.h"

// Mounts the zip of the game's built-in files, which the build makes from
// the files that the game lists in its meson.build, at the root of files,
// below every other source, so that game data and patches override it.
RESULT ShellBuiltin_Mount(VFS *files);
