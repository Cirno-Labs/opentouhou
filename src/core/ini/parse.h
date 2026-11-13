#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/ini/types.h"

// Reads path and parses it. A file that does not exist gives an empty INI
// and OK, so a first run with no settings file works like an empty one.
// Fails only when the file exists but cannot be read, or its text fails to
// parse (see Ini_Parse). The INI and everything it owns come from allocator;
// Ini_Free releases it.
RESULT Ini_Load(const char *path, const ALLOCATOR *allocator, INI **out);

// Parses size bytes of INI text. On success, *out is a new INI built from
// allocator. Fails only when the text is not valid UTF-8; every other
// problem in the text is a line kept verbatim, logged as a warning.
RESULT Ini_Parse(
    const char *text, size_t size, const ALLOCATOR *allocator, INI **out);
