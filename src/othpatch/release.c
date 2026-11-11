#include "othpatch/release.h"

#include "core/base/string.h"
#include "core/json/get.h"

#include <stdio.h>
#include <string.h>

#define M_DIGEST_PREFIX "sha256:"

static bool M_ParseNumber(const char **const text, int32_t *const out)
{
    const char *cursor = *text;
    int64_t value = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10 + (*cursor - '0');
        if (value > INT32_MAX) {
            return false;
        }
        cursor++;
    }
    if (cursor == *text) {
        return false;
    }
    *out = (int32_t)value;
    *text = cursor;
    return true;
}

bool Release_ParseVersion(const char *const text, RELEASE_VERSION *const out)
{
    const char *cursor = text;
    RELEASE_VERSION version;
    if (M_ParseNumber(&cursor, &version.major) && *cursor++ == '.'
        && M_ParseNumber(&cursor, &version.minor) && *cursor++ == '.'
        && M_ParseNumber(&cursor, &version.patch) && *cursor == '\0') {
        *out = version;
        return true;
    }
    return false;
}

int32_t Release_CompareVersions(
    const RELEASE_VERSION a, const RELEASE_VERSION b)
{
    if (a.major != b.major) {
        return a.major < b.major ? -1 : 1;
    }
    if (a.minor != b.minor) {
        return a.minor < b.minor ? -1 : 1;
    }
    if (a.patch != b.patch) {
        return a.patch < b.patch ? -1 : 1;
    }
    return 0;
}

// Returns whether release is a published release of game, and writes its
// version into out.
static bool M_ReadRelease(
    const JSON_VALUE *const release, const char *const game,
    RELEASE_VERSION *const out)
{
    if (Json_GetBool(Json_Get(release, "draft"), false)
        || Json_GetBool(Json_Get(release, "prerelease"), false)) {
        return false;
    }
    const char *const tag = Json_GetString(Json_Get(release, "tag_name"), "");
    const size_t length = strlen(game);
    return strncmp(tag, game, length) == 0
        && strncmp(tag + length, "-v", 2) == 0
        && Release_ParseVersion(tag + length + 2, out);
}

static void M_FindPackage(const JSON_VALUE *const release, RELEASE *const out)
{
    const JSON_VALUE *const assets = Json_Get(release, "assets");
    for (int32_t i = 0; Json_At(assets, i) != nullptr; i++) {
        const JSON_VALUE *const asset = Json_At(assets, i);
        const char *const name = Json_GetString(Json_Get(asset, "name"), "");
        if (strcmp(name, out->package) != 0) {
            continue;
        }
        out->url =
            Json_GetString(Json_Get(asset, "browser_download_url"), nullptr);
        const char *const digest =
            Json_GetString(Json_Get(asset, "digest"), "");
        if (Str_StartsWith(digest, M_DIGEST_PREFIX)) {
            out->sha256 = digest + strlen(M_DIGEST_PREFIX);
        }
        return;
    }
}

RESULT Release_FindNewest(
    const JSON_VALUE *const list, const char *const game,
    const char *const platform, ARENA *const arena, RELEASE *const out)
{
    *out = (RELEASE) {};
    FAIL_IF(
        list == nullptr || list->type != JSON_ARRAY,
        "the release list is not a JSON array");
    const JSON_VALUE *newest = nullptr;
    for (int32_t i = 0; i < list->array.count; i++) {
        const JSON_VALUE *const release = &list->array.items[i];
        RELEASE_VERSION version;
        if (M_ReadRelease(release, game, &version)
            && (newest == nullptr
                || Release_CompareVersions(version, out->version) > 0)) {
            newest = release;
            out->version = version;
        }
    }
    FAIL_IF(newest == nullptr, "the repository has no release of %s", game);
    const int size = snprintf(
        nullptr, 0, "o%s-%d.%d.%d-%s.zip", game, out->version.major,
        out->version.minor, out->version.patch, platform);
    char *const package = Arena_Alloc(arena, (size_t)size + 1);
    snprintf(
        package, (size_t)size + 1, "o%s-%d.%d.%d-%s.zip", game,
        out->version.major, out->version.minor, out->version.patch, platform);
    out->package = package;
    M_FindPackage(newest, out);
    return OK;
}
