#pragma once

// Mounts archives and folders into one virtual filesystem, so the rest
// of the code opens game files by path without knowing where they live.

#include "core/vfs/mount.h"
#include "core/vfs/open.h"
#include "core/vfs/types.h"
