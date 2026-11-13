#include "core/patch/repo.h"

#include "patch_server.h"
#include "test.h"
#include "test_dir.h"

#define M_ARENA_SIZE ((size_t)64 * 1024 * 1024)

typedef struct {
    char *dir;
    ARENA arena;
    PATCH_NETWORK *network;
    PATCH_REMOTE_LIST list;
} M_CASE;

// Opens the network whose first repository is <dir>/first.
static void M_Open(M_CASE *const test)
{
    Arena_Init(&test->arena, "test", M_ARENA_SIZE);
    char *const dir_url = TestDir_FileUrl(test->dir);
    char *const url = Str_Format("%s/first/repo.js", dir_url);
    Mem_Free(dir_url);
    CHECK(Result_Log(
        PatchRepo_Open(url, Server_Fetch, &test->arena, &test->network),
        LOG_LEVEL_ERROR));
    Mem_Free(url);
    Array_Init(&test->list, Arena_Allocator(&test->arena));
}

static void M_Close(M_CASE *const test)
{
    Arena_Free(&test->arena);
    Mem_Free(test->dir);
}

static void M_CheckList(const M_CASE *const test, const char *const expected)
{
    char *joined = Str_Dup("");
    for (int32_t i = 0; i < test->list.count; i++) {
        char *const longer = Str_Format(
            "%s%s%s", joined, i > 0 ? " " : "", test->list.items[i]->id);
        Mem_Free(joined);
        joined = longer;
    }
    CHECK_EQ_STR(joined, expected);
    Mem_Free(joined);
}

TEST(open_reads_the_first_repository)
{
    M_CASE test = { .dir = TestDir_Prepare("open") };
    Server_AddRepo(test.dir, "first", nullptr, 0, "\"lang_en\": \"English\"");
    M_Open(&test);
    const PATCH_REPO *const first = PatchRepo_GetFirst(test.network);
    CHECK_EQ_STR(first->id, "first");
    CHECK_EQ_STR(first->title, "Repository first");
    CHECK_EQ_INT(first->server_count, 1);
    CHECK(PatchRepo_HasPatch(first, "lang_en"));
    CHECK(!PatchRepo_HasPatch(first, "lang_de"));
    M_Close(&test);
}

TEST(open_of_a_missing_repository_fails)
{
    char *const dir = TestDir_Prepare("open_missing");
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    char *const dir_url = TestDir_FileUrl(dir);
    char *const url = Str_Format("%s/none/", dir_url);
    Mem_Free(dir_url);
    PATCH_NETWORK *network;
    const RESULT result = PatchRepo_Open(url, Server_Fetch, &arena, &network);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    Mem_Free(url);
    Arena_Free(&arena);
    Mem_Free(dir);
}

TEST(discovery_walks_neighbors_breadth_first)
{
    M_CASE test = { .dir = TestDir_Prepare("discovery") };
    const char *const first_neighbors[] = { "near", "dead" };
    const char *const near_neighbors[] = { "far", "first" };
    Server_AddRepo(test.dir, "first", first_neighbors, 2, "");
    Server_AddRepo(test.dir, "near", near_neighbors, 2, "\"p\": \"\"");
    Server_AddRepo(test.dir, "far", nullptr, 0, "\"p\": \"\", \"q\": \"\"");
    M_Open(&test);
    CHECK(PatchRepo_FindRepo(test.network, "far") != nullptr);
    CHECK(PatchRepo_FindRepo(test.network, "dead") == nullptr);
    const PATCH_REPO *const p = PatchRepo_FindPatch(test.network, nullptr, "p");
    CHECK(p != nullptr && strcmp(p->id, "near") == 0);
    const PATCH_REPO *const q = PatchRepo_FindPatch(test.network, nullptr, "q");
    CHECK(q != nullptr && strcmp(q->id, "far") == 0);
    CHECK(PatchRepo_FindPatch(test.network, nullptr, "r") == nullptr);
    M_Close(&test);
}

TEST(find_patch_prefers_the_given_repository)
{
    M_CASE test = { .dir = TestDir_Prepare("prefer") };
    const char *const neighbors[] = { "a", "b" };
    Server_AddRepo(test.dir, "first", neighbors, 2, "");
    Server_AddRepo(test.dir, "a", nullptr, 0, "\"p\": \"\"");
    Server_AddRepo(test.dir, "b", nullptr, 0, "\"p\": \"\"");
    M_Open(&test);
    const PATCH_REPO *const b = PatchRepo_FindRepo(test.network, "b");
    CHECK(PatchRepo_FindPatch(test.network, b, "p") == b);
    const PATCH_REPO *const any =
        PatchRepo_FindPatch(test.network, nullptr, "p");
    CHECK(any != nullptr && strcmp(any->id, "a") == 0);
    M_Close(&test);
}

TEST(add_puts_dependencies_first)
{
    M_CASE test = { .dir = TestDir_Prepare("dependencies") };
    const char *const neighbors[] = { "other" };
    Server_AddRepo(
        test.dir, "first", neighbors, 1,
        "\"lang_en\": \"\", \"base\": \"\", \"script\": \"\"");
    Server_AddRepo(
        test.dir, "other", nullptr, 0, "\"base\": \"\", \"tool\": \"\"");
    Server_AddPatch(
        test.dir, "first", "lang_en",
        "\"dependencies\": [\"other/base\", \"script\", \"tool\"]");
    Server_AddPatch(
        test.dir, "first", "script", "\"dependencies\": [\"base\"]");
    Server_AddPatch(test.dir, "first", "base", "");
    Server_AddPatch(test.dir, "other", "base", "");
    Server_AddPatch(test.dir, "other", "tool", "");
    M_Open(&test);
    CHECK(Result_Log(
        PatchRepo_AddPatch(test.network, "lang_en", &test.list),
        LOG_LEVEL_ERROR));
    M_CheckList(
        &test, "other/base first/base first/script other/tool first/lang_en");
    const PATCH_REMOTE *const lang = test.list.items[test.list.count - 1];
    CHECK_EQ_STR(lang->title, "Patch lang_en");
    CHECK_EQ_INT(lang->server_count, 1);
    M_Close(&test);
}

TEST(add_keeps_patches_that_the_list_holds)
{
    M_CASE test = { .dir = TestDir_Prepare("again") };
    Server_AddRepo(test.dir, "first", nullptr, 0, "\"a\": \"\", \"b\": \"\"");
    Server_AddPatch(test.dir, "first", "a", "\"dependencies\": [\"b\"]");
    Server_AddPatch(test.dir, "first", "b", "");
    M_Open(&test);
    CHECK(Result_Log(
        PatchRepo_AddPatch(test.network, "first/b", &test.list),
        LOG_LEVEL_ERROR));
    CHECK(Result_Log(
        PatchRepo_AddPatch(test.network, "first/a", &test.list),
        LOG_LEVEL_ERROR));
    M_CheckList(&test, "first/b first/a");
    M_Close(&test);
}

TEST(add_leaves_out_a_dependency_cycle)
{
    M_CASE test = { .dir = TestDir_Prepare("cycle") };
    Server_AddRepo(test.dir, "first", nullptr, 0, "\"a\": \"\", \"b\": \"\"");
    Server_AddPatch(test.dir, "first", "a", "\"dependencies\": [\"b\"]");
    Server_AddPatch(test.dir, "first", "b", "\"dependencies\": [\"a\"]");
    M_Open(&test);
    CHECK(Result_Log(
        PatchRepo_AddPatch(test.network, "a", &test.list), LOG_LEVEL_ERROR));
    M_CheckList(&test, "first/b first/a");
    M_Close(&test);
}

TEST(add_with_a_missing_dependency_fails_and_keeps_the_list)
{
    M_CASE test = { .dir = TestDir_Prepare("missing_dependency") };
    Server_AddRepo(
        test.dir, "first", nullptr, 0, "\"a\": \"\", \"b\": \"\", \"c\": \"\"");
    Server_AddPatch(
        test.dir, "first", "a", "\"dependencies\": [\"b\", \"gone\"]");
    Server_AddPatch(test.dir, "first", "b", "");
    Server_AddPatch(test.dir, "first", "c", "");
    M_Open(&test);
    CHECK(Result_Log(
        PatchRepo_AddPatch(test.network, "c", &test.list), LOG_LEVEL_ERROR));
    const RESULT result = PatchRepo_AddPatch(test.network, "a", &test.list);
    CHECK(!IS_OK(result));
    CHECK(
        result.message != nullptr
        && strstr(result.message, "dependency gone of first/a") != nullptr);
    Result_Ignore(result);
    M_CheckList(&test, "first/c");
    M_Close(&test);
}

TEST(add_of_an_unknown_repository_or_patch_fails)
{
    M_CASE test = { .dir = TestDir_Prepare("unknown") };
    Server_AddRepo(test.dir, "first", nullptr, 0, "\"a\": \"\"");
    M_Open(&test);
    const char *const ids[] = { "nowhere/a", "first/b", "b" };
    for (size_t i = 0; i < ARRAY_SIZE(ids); i++) {
        const RESULT result =
            PatchRepo_AddPatch(test.network, ids[i], &test.list);
        CHECK(!IS_OK(result));
        Result_Ignore(result);
    }
    CHECK_EQ_INT(test.list.count, 0);
    M_Close(&test);
}

TEST(add_of_a_listed_patch_without_patch_js_fails)
{
    M_CASE test = { .dir = TestDir_Prepare("no_patch_js") };
    Server_AddRepo(test.dir, "first", nullptr, 0, "\"a\": \"\"");
    M_Open(&test);
    const RESULT result = PatchRepo_AddPatch(test.network, "a", &test.list);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    M_Close(&test);
}

TEST(unsafe_ids_are_skipped)
{
    M_CASE test = { .dir = TestDir_Prepare("unsafe") };
    const char *const neighbors[] = { "bad" };
    Server_AddRepo(test.dir, "first", neighbors, 1, "\"..\": \"\"");
    PatchBuilder_Write(
        test.dir, "bad/repo.js", "{\"id\": \"../x\", \"patches\": {}}");
    M_Open(&test);
    CHECK(PatchRepo_FindRepo(test.network, "../x") == nullptr);
    const RESULT result =
        PatchRepo_AddPatch(test.network, "first/..", &test.list);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    M_Close(&test);
}

TEST(patch_servers_come_before_the_repository_folder)
{
    M_CASE test = { .dir = TestDir_Prepare("servers") };
    Server_AddRepo(test.dir, "first", nullptr, 0, "\"a\": \"\"");
    Server_AddPatch(
        test.dir, "first", "a", "\"servers\": [\"https://example.com/a\"]");
    M_Open(&test);
    CHECK(Result_Log(
        PatchRepo_AddPatch(test.network, "a", &test.list), LOG_LEVEL_ERROR));
    const PATCH_REMOTE *const patch = test.list.items[0];
    CHECK_EQ_INT(patch->server_count, 2);
    CHECK_EQ_STR(patch->servers[0], "https://example.com/a/");
    char *const dir_url = TestDir_FileUrl(test.dir);
    char *const local = Str_Format("%s/first/a/", dir_url);
    Mem_Free(dir_url);
    CHECK_EQ_STR(patch->servers[1], local);
    Mem_Free(local);
    M_Close(&test);
}

TEST(bare_dependency_prefers_the_repository_of_its_patch)
{
    M_CASE test = { .dir = TestDir_Prepare("bare_same_repo") };
    const char *const neighbors[] = { "other" };
    Server_AddRepo(test.dir, "first", neighbors, 1, "\"base\": \"\"");
    Server_AddRepo(
        test.dir, "other", nullptr, 0, "\"base\": \"\", \"p\": \"\"");
    Server_AddPatch(test.dir, "first", "base", "");
    Server_AddPatch(test.dir, "other", "base", "");
    Server_AddPatch(test.dir, "other", "p", "\"dependencies\": [\"base\"]");
    M_Open(&test);
    CHECK(Result_Log(
        PatchRepo_AddPatch(test.network, "other/p", &test.list),
        LOG_LEVEL_ERROR));
    M_CheckList(&test, "other/base other/p");
    M_Close(&test);
}

TEST(discovery_skips_a_repository_with_a_known_id)
{
    M_CASE test = { .dir = TestDir_Prepare("known_id") };
    const char *const neighbors[] = { "copy" };
    Server_AddRepo(test.dir, "first", neighbors, 1, "");
    PatchBuilder_Write(
        test.dir, "copy/repo.js",
        "{\"id\": \"first\", \"patches\": {\"x\": \"\"}}");
    M_Open(&test);
    CHECK(PatchRepo_FindPatch(test.network, nullptr, "x") == nullptr);
    CHECK_EQ_INT(PatchRepo_GetFirst(test.network)->server_count, 1);
    M_Close(&test);
}
