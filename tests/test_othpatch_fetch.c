#include "othpatch/fetch.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include "patch_builder.h"
#include "test.h"
#include "test_dir.h"

static FETCH M_Fetch(const char *const url)
{
    FETCH fetch = { .url = url };
    Fetch_Run(&fetch, 1);
    return fetch;
}

TEST(file_url_reads_the_file_and_ignores_the_query)
{
    char *const dir = TestDir_Prepare("file");
    PatchBuilder_Write(dir, "a b/c.txt", "hello");
    char *const dir_url = TestDir_FileUrl(dir);
    char *const url = Str_Format("%s/a%%20b/c.txt?crc32=1234", dir_url);
    Mem_Free(dir_url);
    FETCH fetch = M_Fetch(url);
    CHECK(Result_Log(fetch.result, LOG_LEVEL_ERROR));
    fetch.result = OK;
    CHECK_EQ_INT(fetch.size, 5);
    CHECK(fetch.data != nullptr && memcmp(fetch.data, "hello", 5) == 0);
    Fetch_Release(&fetch);
    Mem_Free(url);
    Mem_Free(dir);
}

TEST(file_url_of_an_empty_file_gives_no_bytes)
{
    char *const dir = TestDir_Prepare("empty");
    PatchBuilder_Write(dir, "empty", "");
    char *const dir_url = TestDir_FileUrl(dir);
    char *const url =
        Str_Format("file://localhost%s/empty", dir_url + strlen("file://"));
    Mem_Free(dir_url);
    FETCH fetch = M_Fetch(url);
    CHECK(Result_Log(fetch.result, LOG_LEVEL_ERROR));
    fetch.result = OK;
    CHECK_EQ_INT(fetch.size, 0);
    CHECK(fetch.data != nullptr);
    Fetch_Release(&fetch);
    Mem_Free(url);
    Mem_Free(dir);
}

TEST(file_url_of_a_missing_file_fails)
{
    char *const dir = TestDir_Prepare("missing");
    char *const dir_url = TestDir_FileUrl(dir);
    char *const url = Str_Format("%s/missing", dir_url);
    Mem_Free(dir_url);
    FETCH fetch = M_Fetch(url);
    CHECK(!IS_OK(fetch.result));
    CHECK(fetch.data == nullptr);
    Fetch_Release(&fetch);
    Mem_Free(url);
    Mem_Free(dir);
}

TEST(file_url_with_a_host_or_a_bad_escape_fails)
{
    const char *const urls[] = {
        "file://server/share/file",
        "file:///tmp/%zz",
        "file:///tmp/%00",
        "file:///tmp/%4",
    };
    for (size_t i = 0; i < ARRAY_SIZE(urls); i++) {
        FETCH fetch = M_Fetch(urls[i]);
        CHECK(!IS_OK(fetch.result));
        Fetch_Release(&fetch);
    }
}

TEST(other_schemes_fail)
{
    FETCH fetch = M_Fetch("ftp://example.com/file");
    CHECK(!IS_OK(fetch.result));
    CHECK(strstr(fetch.result.message, "only http, https and file") != nullptr);
    Fetch_Release(&fetch);
}

TEST(run_fetches_more_urls_than_transfers)
{
    char *const dir = TestDir_Prepare("many");
    enum { COUNT = FETCH_MAX_TRANSFERS * 2 + 3 };
    FETCH fetches[COUNT];
    char *urls[COUNT];
    for (int i = 0; i < COUNT; i++) {
        char name[16];
        char content[16];
        snprintf(name, sizeof(name), "f%d", i);
        snprintf(content, sizeof(content), "content %d", i);
        PatchBuilder_Write(dir, name, content);
        char *const dir_url = TestDir_FileUrl(dir);
        urls[i] = Str_Format("%s/%s", dir_url, name);
        Mem_Free(dir_url);
        fetches[i] = (FETCH) { .url = urls[i] };
    }
    Fetch_Run(fetches, COUNT);
    for (int i = 0; i < COUNT; i++) {
        char content[16];
        snprintf(content, sizeof(content), "content %d", i);
        CHECK(IS_OK(fetches[i].result));
        CHECK(
            fetches[i].size == strlen(content)
            && memcmp(fetches[i].data, content, fetches[i].size) == 0);
        Fetch_Release(&fetches[i]);
        Mem_Free(urls[i]);
    }
    Mem_Free(dir);
}
