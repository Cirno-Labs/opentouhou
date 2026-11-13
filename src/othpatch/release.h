// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/result.h"
#include "core/json/types.h"

// Finds the newest release of a game or of othpatch in releases.json, the
// release index that tools/release_index writes to the R2 bucket.
// tools/shared/release_index.py describes its format.

// Holds a version of the form "X.Y.Z".
typedef struct {
    int32_t major;
    int32_t minor;
    int32_t patch;
} RELEASE_VERSION;

// Holds the newest release of a program.
typedef struct {
    RELEASE_VERSION version;
    // Holds the name of the package for the platform, such as
    // "oth06-X.Y.Z-Linux.zip", and its URL, or nullptr for both when the
    // release has no package for the platform.
    const char *package;
    const char *url;
    // Holds the SHA-256 digest of the package as hexadecimal text, or
    // nullptr when the index gives none.
    const char *sha256;
} RELEASE;

// Returns whether text is a version of three decimal numbers separated by
// '.', such as "1.2.0", and writes it into out. A development build, such
// as "1.2.0-dev+abcdef0" or "1.2.0-dev", has no such version.
bool Release_ParseVersion(const char *text, RELEASE_VERSION *out);

// Returns a number below 0, 0 or above 0 when a is older than b, the same
// as b or newer than b.
int32_t Release_CompareVersions(RELEASE_VERSION a, RELEASE_VERSION b);

// Finds the newest release of program, such as "oth06" or "othpatch", in
// index, and its package for platform, such as "Linux". Only the releases
// with a version "X.Y.Z" count. The strings of out come from index.
// Reports failure when index is not a release index or has no release of
// the program.
RESULT Release_FindNewest(
    const JSON_VALUE *index, const char *program, const char *platform,
    RELEASE *out);
