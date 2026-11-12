#include "othpatch/release.h"

#include "core/json/get.h"

#define M_FORMAT 1

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

RESULT Release_FindNewest(
    const JSON_VALUE *const index, const char *const program,
    const char *const platform, RELEASE *const out)
{
    *out = (RELEASE) {};
    FAIL_IF(
        index == nullptr || index->type != JSON_OBJECT
            || Json_GetInt(Json_Get(index, "format"), 0) != M_FORMAT,
        "the release list is not a release index of format %d", M_FORMAT);
    const JSON_VALUE *const releases =
        Json_Get(Json_Get(index, "programs"), program);
    const JSON_VALUE *newest = nullptr;
    for (int32_t i = 0; Json_At(releases, i) != nullptr; i++) {
        const JSON_VALUE *const release = Json_At(releases, i);
        RELEASE_VERSION version;
        if (Release_ParseVersion(
                Json_GetString(Json_Get(release, "version"), ""), &version)
            && (newest == nullptr
                || Release_CompareVersions(version, out->version) > 0)) {
            newest = release;
            out->version = version;
        }
    }
    FAIL_IF(newest == nullptr, "the index has no release of %s", program);
    const JSON_VALUE *const file =
        Json_Get(Json_Get(newest, "files"), platform);
    out->package = Json_GetString(Json_Get(file, "name"), nullptr);
    out->url = Json_GetString(Json_Get(file, "url"), nullptr);
    if (out->package == nullptr || out->url == nullptr) {
        out->package = nullptr;
        out->url = nullptr;
    }
    out->sha256 = Json_GetString(Json_Get(file, "sha256"), nullptr);
    return OK;
}
