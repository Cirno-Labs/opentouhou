#pragma once

#include "replay/harness/backend.h"

#include "core/vfs/types.h"

// Returns a read-only VFS source of count staged files, a later file
// hiding an earlier one of the same path. files must stay valid until the
// source closes.
VFS_SOURCE *Staged_CreateSource(const STAGED_FILE *files, size_t count);
