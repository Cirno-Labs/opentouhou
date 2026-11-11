#pragma once

#include "core/base/arena.h"
#include "core/base/result.h"
#include "core/json/types.h"

// Finds the newest release of a game in the release list of a GitHub
// repository, from https://api.github.com/repos/<owner>/<name>/releases.

// Holds a version of the form "X.Y.Z".
typedef struct {
    int32_t major;
    int32_t minor;
    int32_t patch;
} RELEASE_VERSION;

// Holds the newest release of a game.
typedef struct {
    RELEASE_VERSION version;
    // Holds the name of the package for the platform,
    // "o<game>-X.Y.Z-<platform>.zip".
    const char *package;
    // Holds the download URL of the package, or nullptr when the release has
    // no such package.
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

// Finds the newest release of game, such as "th06", in list, the JSON
// array of the releases API. Only the releases tagged "<game>-vX.Y.Z" count;
// drafts and pre-releases are skipped. platform is the last part of the
// package name: "Linux", "Windows" or "macOS". The strings of out come
// from list and arena. Reports failure when list is not an array or has no
// release of game.
RESULT Release_FindNewest(
    const JSON_VALUE *list, const char *game, const char *platform,
    ARENA *arena, RELEASE *out);
