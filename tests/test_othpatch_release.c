// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/release.h"

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/json/parse.h"

#include "test.h"

#define M_DIGEST                                                               \
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

typedef struct {
    ARENA arena;
    JSON_VALUE *index;
    RELEASE release;
} M_CASE;

// Returns a release of the index in JSON, with the files in JSON.
static char *M_Release(const char *const version, const char *const files)
{
    return Str_Format(
        "{\"version\": \"%s\", \"date\": \"2026-11-12\", \"changes\": \"\", "
        "\"files\": {%s}}",
        version, files);
}

static char *M_File(const char *const platform, const char *const name)
{
    return Str_Format(
        "\"%s\": {\"name\": \"%s\", \"url\": \"https://example.com/%s\", "
        "\"size\": 1, \"sha256\": \"" M_DIGEST "\"}",
        platform, name, name);
}

static RESULT M_FindIn(
    M_CASE *const test, const char *const text, const char *const program,
    const char *const platform)
{
    Arena_Init(&test->arena, "release test", 1 << 20);
    CHECK(Result_Log(
        Json_Parse(
            text, strlen(text), Arena_Allocator(&test->arena), &test->index),
        LOG_LEVEL_ERROR));
    return Release_FindNewest(test->index, program, platform, &test->release);
}

// Finds the newest oth06 release for Linux in an index whose programs are
// the JSON object members programs.
static RESULT M_Find(M_CASE *const test, const char *const programs)
{
    char *const text =
        Str_Format("{\"format\": 1, \"programs\": {%s}}", programs);
    const RESULT result = M_FindIn(test, text, "oth06", "Linux");
    Mem_Free(text);
    return result;
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
    CHECK(!Release_ParseVersion("1.2.0-dev+abcdef0", &version));
    CHECK(!Release_ParseVersion("1.2.0-dev", &version));
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
    char *const linux_file = M_File("Linux", "oth06-0.10.0-Linux.zip");
    char *const windows_file = M_File("Windows", "oth06-0.10.0-Windows.zip");
    char *const files = Str_Format("%s, %s", windows_file, linux_file);
    char *const old = M_Release("0.9.0", "");
    char *const newest = M_Release("0.10.0", files);
    char *const other_game = M_Release("9.0.0", "");
    char *const programs = Str_Format(
        "\"oth07\": [%s], \"oth06\": [%s, %s], \"othpatch\": [%s]", other_game,
        old, newest, other_game);
    M_CASE test;
    CHECK(Result_Log(M_Find(&test, programs), LOG_LEVEL_ERROR));
    M_CheckVersion(test.release.version, 0, 10, 0);
    CHECK_EQ_STR(test.release.package, "oth06-0.10.0-Linux.zip");
    CHECK_EQ_STR(
        test.release.url, "https://example.com/oth06-0.10.0-Linux.zip");
    CHECK_EQ_STR(test.release.sha256, M_DIGEST);
    Arena_Free(&test.arena);
    Mem_Free(programs);
    Mem_Free(other_game);
    Mem_Free(newest);
    Mem_Free(old);
    Mem_Free(files);
    Mem_Free(windows_file);
    Mem_Free(linux_file);
}

TEST(releases_without_a_plain_version_are_skipped)
{
    char *const dev = M_Release("0.3.0-dev+abcdef0", "");
    char *const plain = M_Release("0.1.0", "");
    char *const programs = Str_Format(
        "\"oth06\": [%s, {\"date\": \"2026-11-12\"}, %s]", dev, plain);
    M_CASE test;
    CHECK(Result_Log(M_Find(&test, programs), LOG_LEVEL_ERROR));
    M_CheckVersion(test.release.version, 0, 1, 0);
    Arena_Free(&test.arena);
    Mem_Free(programs);
    Mem_Free(plain);
    Mem_Free(dev);
}

TEST(release_without_the_platform_has_no_package)
{
    char *const windows_file = M_File("Windows", "oth06-0.1.0-Windows.zip");
    char *const release = M_Release("0.1.0", windows_file);
    char *const programs = Str_Format("\"oth06\": [%s]", release);
    M_CASE test;
    CHECK(Result_Log(M_Find(&test, programs), LOG_LEVEL_ERROR));
    M_CheckVersion(test.release.version, 0, 1, 0);
    CHECK(test.release.package == nullptr);
    CHECK(test.release.url == nullptr);
    CHECK(test.release.sha256 == nullptr);
    Arena_Free(&test.arena);
    Mem_Free(programs);
    Mem_Free(release);
    Mem_Free(windows_file);
}

TEST(file_without_a_sha256_digest_has_none)
{
    const char *const programs =
        "\"oth06\": [{\"version\": \"0.1.0\", \"files\": {\"Linux\": "
        "{\"name\": \"oth06-0.1.0-Linux.zip\", \"url\": \"https://x/\"}}}]";
    M_CASE test;
    CHECK(Result_Log(M_Find(&test, programs), LOG_LEVEL_ERROR));
    CHECK(test.release.url != nullptr);
    CHECK(test.release.sha256 == nullptr);
    Arena_Free(&test.arena);
}

TEST(index_without_a_release_of_the_game_fails)
{
    char *const release = M_Release("1.0.0-rc1", "");
    char *const programs =
        Str_Format("\"oth06\": [%s], \"othpatch\": [%s]", release, release);
    M_CASE test;
    const RESULT result = M_Find(&test, programs);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "no release of oth06") != nullptr);
    Result_Ignore(result);
    Arena_Free(&test.arena);
    Mem_Free(programs);
    Mem_Free(release);
}

TEST(newest_release_of_othpatch_wins)
{
    char *const windows_file = M_File("Windows", "othpatch-1.2.0-Windows.exe");
    char *const game = M_Release("9.0.0", "");
    char *const old = M_Release("1.1.0", "");
    char *const newest = M_Release("1.2.0", windows_file);
    char *const text = Str_Format(
        "{\"format\": 1, \"programs\": {\"oth06\": [%s], \"othpatch\": [%s, "
        "%s]}}",
        game, newest, old);
    M_CASE test;
    CHECK(Result_Log(
        M_FindIn(&test, text, "othpatch", "Windows"), LOG_LEVEL_ERROR));
    M_CheckVersion(test.release.version, 1, 2, 0);
    CHECK_EQ_STR(test.release.package, "othpatch-1.2.0-Windows.exe");
    CHECK_EQ_STR(
        test.release.url, "https://example.com/othpatch-1.2.0-Windows.exe");
    Arena_Free(&test.arena);
    Mem_Free(text);
    Mem_Free(newest);
    Mem_Free(old);
    Mem_Free(game);
    Mem_Free(windows_file);
}

TEST(text_that_is_not_an_index_fails)
{
    const char *const texts[] = {
        "[]",
        "{\"message\": \"Not Found\"}",
        "{\"format\": 2, \"programs\": {\"oth06\": []}}",
    };
    for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
        M_CASE test;
        const RESULT result = M_FindIn(&test, texts[i], "oth06", "Linux");
        CHECK(!IS_OK(result));
        CHECK(strstr(result.message, "not a release index") != nullptr);
        Result_Ignore(result);
        Arena_Free(&test.arena);
    }
}
