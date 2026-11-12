#include "othpatch/release.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/json/parse.h"

#include "test.h"

typedef struct {
    ARENA arena;
    JSON_VALUE *list;
    RELEASE release;
} M_CASE;

// Returns a release of the list in JSON.
static char *M_Release(
    const char *const tag, const bool draft, const bool prerelease,
    const char *const assets)
{
    return Str_Format(
        "{\"tag_name\": \"%s\", \"draft\": %s, \"prerelease\": %s, "
        "\"assets\": [%s]}",
        tag, draft ? "true" : "false", prerelease ? "true" : "false", assets);
}

static char *M_Asset(const char *const name, const char *const digest)
{
    return Str_Format(
        "{\"name\": \"%s\", \"url\": "
        "\"https://example.com/%s\"%s%s%s}",
        name, name, digest != nullptr ? ", \"digest\": \"" : "",
        digest != nullptr ? digest : "", digest != nullptr ? "\"" : "");
}

static RESULT M_FindNames(
    M_CASE *const test, const char *const text,
    const RELEASE_NAMES *const names)
{
    Arena_Init(&test->arena, "release test", 1 << 20);
    CHECK(Result_Log(
        Json_Parse(
            text, strlen(text), Arena_Allocator(&test->arena), &test->list),
        LOG_LEVEL_ERROR));
    return Release_FindNewest(test->list, names, &test->arena, &test->release);
}

static RESULT M_Find(M_CASE *const test, const char *const text)
{
    const RELEASE_NAMES names = {
        .program = "th06",
        .tag_prefix = "th06-v",
        .package_prefix = "oth06-",
        .package_suffix = "-Linux.zip",
    };
    return M_FindNames(test, text, &names);
}

static void M_CheckVersion(
    const RELEASE_VERSION version, const int32_t major, const int32_t minor,
    const int32_t patch)
{
    CHECK_EQ_INT(version.major, major);
    CHECK_EQ_INT(version.minor, minor);
    CHECK_EQ_INT(version.patch, patch);
}

TEST(plain_versions_parse)
{
    RELEASE_VERSION version;
    CHECK(Release_ParseVersion("1.20.3", &version));
    M_CheckVersion(version, 1, 20, 3);
    CHECK(Release_ParseVersion("0.0.0", &version));
}

TEST(development_versions_do_not_parse)
{
    RELEASE_VERSION version;
    CHECK(!Release_ParseVersion("1.2.0-3-gabcdef0", &version));
    CHECK(!Release_ParseVersion("1.2.0-dirty", &version));
    CHECK(!Release_ParseVersion("unknown", &version));
    CHECK(!Release_ParseVersion("abcdef0", &version));
    CHECK(!Release_ParseVersion("1.2", &version));
    CHECK(!Release_ParseVersion("1..2", &version));
    CHECK(!Release_ParseVersion("v1.2.0", &version));
    CHECK(!Release_ParseVersion("", &version));
    CHECK(!Release_ParseVersion("1.2.99999999999", &version));
}

TEST(versions_compare_by_number)
{
    const RELEASE_VERSION a = { 0, 9, 0 };
    const RELEASE_VERSION b = { 0, 10, 0 };
    const RELEASE_VERSION c = { 1, 0, 0 };
    CHECK(Release_CompareVersions(a, b) < 0);
    CHECK(Release_CompareVersions(b, a) > 0);
    CHECK(Release_CompareVersions(c, b) > 0);
    CHECK(Release_CompareVersions(b, b) == 0);
}

TEST(newest_release_of_the_game_wins)
{
    char *const asset = M_Asset(
        "oth06-0.10.0-Linux.zip",
        "sha256:"
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    char *const other = M_Asset("oth06-0.10.0-Windows.zip", nullptr);
    char *const assets = Str_Format("%s, %s", other, asset);
    char *const old = M_Release("th06-v0.9.0", false, false, "");
    char *const newest = M_Release("th06-v0.10.0", false, false, assets);
    char *const other_game = M_Release("th07-v9.0.0", false, false, "");
    char *const othpatch = M_Release("othpatch-v5.0.0", false, false, "");
    char *const latest = M_Release("latest", false, true, "");
    char *const text = Str_Format(
        "[%s, %s, %s, %s, %s]", old, other_game, newest, othpatch, latest);
    M_CASE test;
    CHECK(Result_Log(M_Find(&test, text), LOG_LEVEL_ERROR));
    M_CheckVersion(test.release.version, 0, 10, 0);
    CHECK_EQ_STR(test.release.package, "oth06-0.10.0-Linux.zip");
    CHECK_EQ_STR(
        test.release.url, "https://example.com/oth06-0.10.0-Linux.zip");
    CHECK_EQ_STR(
        test.release.sha256,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    Arena_Free(&test.arena);
    Mem_Free(text);
    Mem_Free(latest);
    Mem_Free(othpatch);
    Mem_Free(other_game);
    Mem_Free(newest);
    Mem_Free(old);
    Mem_Free(assets);
    Mem_Free(other);
    Mem_Free(asset);
}

TEST(drafts_and_pre_releases_are_skipped)
{
    char *const draft = M_Release("th06-v0.3.0", true, false, "");
    char *const pre = M_Release("th06-v0.2.0", false, true, "");
    char *const plain = M_Release("th06-v0.1.0", false, false, "");
    char *const text = Str_Format("[%s, %s, %s]", draft, pre, plain);
    M_CASE test;
    CHECK(Result_Log(M_Find(&test, text), LOG_LEVEL_ERROR));
    M_CheckVersion(test.release.version, 0, 1, 0);
    CHECK(test.release.url == nullptr);
    CHECK(test.release.sha256 == nullptr);
    Arena_Free(&test.arena);
    Mem_Free(text);
    Mem_Free(plain);
    Mem_Free(pre);
    Mem_Free(draft);
}

TEST(asset_without_a_sha256_digest_has_none)
{
    char *const asset = M_Asset("oth06-0.1.0-Linux.zip", "md5:abc");
    char *const release = M_Release("th06-v0.1.0", false, false, asset);
    char *const text = Str_Format("[%s]", release);
    M_CASE test;
    CHECK(Result_Log(M_Find(&test, text), LOG_LEVEL_ERROR));
    CHECK(test.release.url != nullptr);
    CHECK(test.release.sha256 == nullptr);
    Arena_Free(&test.arena);
    Mem_Free(text);
    Mem_Free(release);
    Mem_Free(asset);
}

TEST(list_without_a_release_of_the_game_fails)
{
    char *const release = M_Release("th06-v1.0.0-rc1", false, false, "");
    char *const text = Str_Format("[%s]", release);
    M_CASE test;
    const RESULT result = M_Find(&test, text);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "no release of th06") != nullptr);
    Result_Ignore(result);
    Arena_Free(&test.arena);
    Mem_Free(text);
    Mem_Free(release);
}

TEST(newest_release_of_othpatch_wins)
{
    char *const asset = M_Asset("othpatch-1.2.0-Windows.exe", nullptr);
    char *const game = M_Release("th06-v9.0.0", false, false, "");
    char *const old = M_Release("othpatch-v1.1.0", false, false, "");
    char *const newest = M_Release("othpatch-v1.2.0", false, false, asset);
    char *const text = Str_Format("[%s, %s, %s]", game, newest, old);
    const RELEASE_NAMES names = {
        .program = "othpatch",
        .tag_prefix = "othpatch-v",
        .package_prefix = "othpatch-",
        .package_suffix = "-Windows.exe",
    };
    M_CASE test;
    CHECK(Result_Log(M_FindNames(&test, text, &names), LOG_LEVEL_ERROR));
    M_CheckVersion(test.release.version, 1, 2, 0);
    CHECK_EQ_STR(test.release.package, "othpatch-1.2.0-Windows.exe");
    CHECK_EQ_STR(
        test.release.url, "https://example.com/othpatch-1.2.0-Windows.exe");
    Arena_Free(&test.arena);
    Mem_Free(text);
    Mem_Free(newest);
    Mem_Free(old);
    Mem_Free(game);
    Mem_Free(asset);
}

TEST(list_that_is_not_an_array_fails)
{
    M_CASE test;
    CHECK(!Result_Ignore(M_Find(&test, "{\"message\": \"Not Found\"}")));
    Arena_Free(&test.arena);
}
