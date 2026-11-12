#pragma once

#include "core/base/arena.h"
#include "core/base/result.h"
#include "core/json/types.h"

// Finds the newest release of a game or of othpatch in the release list
// of a GitHub repository, from
// https://api.github.com/repos/<owner>/<name>/releases.

// Holds a version of the form "X.Y.Z".
typedef struct {
    int32_t major;
    int32_t minor;
    int32_t patch;
} RELEASE_VERSION;

// Names the media type that the API URL of a package gives the file for.
#define RELEASE_ACCEPT "application/octet-stream"

// Names the releases of one program and its package for one platform.
typedef struct {
    // Holds the name of the program in messages, such as "th06" or
    // "othpatch".
    const char *program;
    // Holds the part of a tag before "X.Y.Z", such as "th06-v" or
    // "othpatch-v".
    const char *tag_prefix;
    // Hold the parts of the package name before and after "X.Y.Z", such as
    // "oth06-" and "-Linux.zip", or "othpatch-" and "-Windows.exe".
    const char *package_prefix;
    const char *package_suffix;
} RELEASE_NAMES;

// Holds the newest release of a program.
typedef struct {
    RELEASE_VERSION version;
    // Holds the name of the package for the platform, such as
    // "oth06-X.Y.Z-Linux.zip".
    const char *package;
    // Holds the API URL of the package, which gives the file itself when
    // fetched with RELEASE_ACCEPT, or nullptr when the release has no such
    // package. Unlike the browser URL, it also works for a private
    // repository.
    const char *url;
    // Holds the SHA-256 digest of the package as hexadecimal text, or
    // nullptr when GitHub gives none.
    const char *sha256;
} RELEASE;

// Returns whether text is a version of three decimal numbers separated by
// '.', such as "1.2.0", and writes it into out. A development build, such
// as "1.2.0-3-gabcdef0" or "unknown", has no such version.
bool Release_ParseVersion(const char *text, RELEASE_VERSION *out);

// Returns a number below 0, 0 or above 0 when a is older than b, the same
// as b or newer than b.
int32_t Release_CompareVersions(RELEASE_VERSION a, RELEASE_VERSION b);

// Finds the newest release of the program that names describes in list,
// the JSON array of the releases API. Only the releases tagged
// "<tag_prefix>X.Y.Z" count; drafts and pre-releases are skipped. The
// strings of out come from list and arena. Reports failure when list is not
// an array or has no release of the program.
RESULT Release_FindNewest(
    const JSON_VALUE *list, const RELEASE_NAMES *names, ARENA *arena,
    RELEASE *out);
