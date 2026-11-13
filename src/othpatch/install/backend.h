#pragma once

#include "core/base/result.h"

// Sets the permissions of the file at path: readable by everyone, writable
// by the owner, and executable by everyone when executable is true. Does
// nothing on a platform without such permissions.
RESULT InstallBackend_SetExecutable(const char *path, bool executable);
