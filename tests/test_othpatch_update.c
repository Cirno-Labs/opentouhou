#include "othpatch/update.h"

#include "othpatch_server.h"
#include "test.h"
#include "test_dir.h"

#define M_ARENA_SIZE ((size_t)64 * 1024 * 1024)

static const char *const m_Games[] = { "th06" };

#define M_MAX_REPORTS 16

typedef struct {
    char *dir;
    char *server;
    char *patches;
    ARENA arena;
    REPO_PATCH_LIST list;
    // Hold the reports of the last update, done and total each.
    int32_t reports[M_MAX_REPORTS][2];
    int32_t report_count;
} M_CASE;

static void M_Record(
    void *const context, const int32_t done, const int32_t total)
{
    M_CASE *const test = context;
    if (test->report_count < M_MAX_REPORTS) {
        test->reports[test->report_count][0] = done;
        test->reports[test->report_count][1] = total;
    }
    test->report_count++;
}

// Prepares a server with the repository "first" that has the patch "p",
// whose files.js has the given members, in the directory dir. M_Close
// releases dir.
static M_CASE M_PrepareIn(char *const dir, const char *const files)
{
    M_CASE test = { .dir = dir };
    test.server = TestDir_Join(test.dir, "server");
    test.patches = TestDir_Join(test.dir, "patches");
    Server_AddRepo(test.server, "first", nullptr, 0, "\"p\": \"\"");
    Server_AddPatch(test.server, "first", "p", "");
    char *const text = Str_Format("{%s}", files);
    Server_AddFile(test.server, "first", "p", "files.js", text);
    Mem_Free(text);
    return test;
}

static M_CASE M_Prepare(const char *const name, const char *const files)
{
    return M_PrepareIn(TestDir_Prepare(name), files);
}

static RESULT M_Update(M_CASE *const test, UPDATE_COUNTS *const counts)
{
    Arena_Init(&test->arena, "test", M_ARENA_SIZE);
    Array_Init(&test->list, Arena_Allocator(&test->arena));
    char *const server_url = TestDir_FileUrl(test->server);
    char *const url = Str_Format("%s/first/", server_url);
    Mem_Free(server_url);
    REPO_NETWORK *network;
    RESULT result = Repo_Open(url, &test->arena, &network);
    Mem_Free(url);
    if (IS_OK(result)) {
        result = Repo_AddPatch(network, "first/p", &test->list);
    }
    const UPDATE_PROGRESS progress = { .report = M_Record, .context = test };
    test->report_count = 0;
    if (IS_OK(result)) {
        result = Update_Patch(
            test->patches, test->list.items[0], m_Games, 1, &progress, counts);
    }
    Arena_Free(&test->arena);
    return result;
}

static void M_Close(M_CASE *const test)
{
    Mem_Free(test->patches);
    Mem_Free(test->server);
    Mem_Free(test->dir);
}

static void M_CheckFile(
    const M_CASE *const test, const char *const path, const char *const content)
{
    char *const data = Server_Read(test->patches, path);
    CHECK_EQ_STR(data, content);
    Mem_Free(data);
}

TEST(update_downloads_shared_and_game_files_only)
{
    char *const shared = Server_Entry("font.ttf", "font");
    char *const game = Server_Entry("th06/msg1.dat.jdiff", "msg");
    char *const other = Server_Entry("th07/msg1.dat.jdiff", "other");
    char *const files = Str_Format("%s, %s, %s", shared, game, other);
    M_CASE test = M_Prepare("download", files);
    Server_AddFile(test.server, "first", "p", "font.ttf", "font");
    Server_AddFile(test.server, "first", "p", "th06/msg1.dat.jdiff", "msg");
    Server_AddFile(test.server, "first", "p", "th07/msg1.dat.jdiff", "other");
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 2);
    CHECK_EQ_INT(counts.up_to_date, 0);
    CHECK_EQ_INT(counts.removed, 0);
    M_CheckFile(&test, "first/p/font.ttf", "font");
    M_CheckFile(&test, "first/p/th06/msg1.dat.jdiff", "msg");
    CHECK(!Server_Exists(test.patches, "first/p/th07"));
    CHECK(Server_Exists(test.patches, "first/repo.js"));
    CHECK(Server_Exists(test.patches, "first/p/patch.js"));
    char *const files_js = Server_Read(test.patches, "first/p/files.js");
    CHECK(files_js != nullptr && strstr(files_js, "th07/msg1") != nullptr);
    Mem_Free(files_js);
    M_Close(&test);
    Mem_Free(files);
    Mem_Free(other);
    Mem_Free(game);
    Mem_Free(shared);
}

// Checks that the reports of the last update start at 0 of total, never go
// down, and end at last of total.
static void M_CheckReports(
    const M_CASE *const test, const int32_t last, const int32_t total)
{
    CHECK(test->report_count >= 2 && test->report_count <= M_MAX_REPORTS);
    CHECK_EQ_INT(test->reports[0][0], 0);
    for (int32_t i = 0; i < test->report_count; i++) {
        CHECK_EQ_INT(test->reports[i][1], total);
        CHECK(i == 0 || test->reports[i][0] >= test->reports[i - 1][0]);
    }
    CHECK_EQ_INT(test->reports[test->report_count - 1][0], last);
}

TEST(update_reports_the_files_as_they_arrive)
{
    char *const kept = Server_Entry("kept", "kept");
    char *const first = Server_Entry("th06/a", "a");
    char *const second = Server_Entry("th06/b", "b");
    char *const files = Str_Format("%s, %s, %s", kept, first, second);
    M_CASE test = M_Prepare("progress", files);
    PatchBuilder_Write(test.patches, "first/p/kept", "kept");
    Server_AddFile(test.server, "first", "p", "th06/a", "a");
    Server_AddFile(test.server, "first", "p", "th06/b", "b");
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    M_CheckReports(&test, 2, 2);
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(test.report_count, 0);
    M_Close(&test);
    Mem_Free(files);
    Mem_Free(second);
    Mem_Free(first);
    Mem_Free(kept);
}

TEST(update_skips_files_with_the_right_crc)
{
    char *const files = Server_Entry("th06/a.png", "image");
    M_CASE test = M_Prepare("crc_skip", files);
    PatchBuilder_Write(test.patches, "first/p/th06/a.png", "image");
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 0);
    CHECK_EQ_INT(counts.up_to_date, 1);
    M_CheckFile(&test, "first/p/th06/a.png", "image");
    M_Close(&test);
    Mem_Free(files);
}

TEST(update_replaces_a_file_with_the_wrong_crc)
{
    char *const files = Server_Entry("th06/a.png", "new");
    M_CASE test = M_Prepare("crc_replace", files);
    Server_AddFile(test.server, "first", "p", "th06/a.png", "new");
    PatchBuilder_Write(test.patches, "first/p/th06/a.png", "old");
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 1);
    CHECK_EQ_INT(counts.up_to_date, 0);
    M_CheckFile(&test, "first/p/th06/a.png", "new");
    M_Close(&test);
    Mem_Free(files);
}

TEST(update_rejects_a_download_with_a_bad_crc)
{
    char *const good = Server_Entry("th06/good", "good");
    char *const bad = Server_Entry("th06/bad", "expected");
    char *const files = Str_Format("%s, %s", good, bad);
    M_CASE test = M_Prepare("crc_bad", files);
    Server_AddFile(test.server, "first", "p", "th06/good", "good");
    Server_AddFile(test.server, "first", "p", "th06/bad", "corrupt");
    PatchBuilder_Write(test.patches, "first/p/th06/good", "old");
    UPDATE_COUNTS counts;
    const RESULT result = M_Update(&test, &counts);
    CHECK(!IS_OK(result));
    CHECK(
        result.message != nullptr
        && strstr(result.message, "1 files did not arrive") != nullptr
        && strstr(result.message, "CRC-32") != nullptr);
    Result_Ignore(result);
    M_CheckReports(&test, 1, 2);
    M_CheckFile(&test, "first/p/th06/good", "old");
    CHECK(!Server_Exists(test.patches, "first/p/th06/bad"));
    CHECK(!Server_Exists(test.patches, "first/p/th06/good.part"));
    CHECK(!Server_Exists(test.patches, "first/p/th06/bad.part"));
    CHECK(!Server_Exists(test.patches, "first/p/files.js"));
    M_Close(&test);
    Mem_Free(files);
    Mem_Free(bad);
    Mem_Free(good);
}

TEST(update_removes_null_entries)
{
    char *const kept = Server_Entry("th06/kept", "kept");
    char *const files =
        Str_Format("%s, \"th06/gone\": null, \"th06/never\": null", kept);
    M_CASE test = M_Prepare("null", files);
    Server_AddFile(test.server, "first", "p", "th06/kept", "kept");
    PatchBuilder_Write(test.patches, "first/p/th06/gone", "old");
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 1);
    CHECK_EQ_INT(counts.removed, 1);
    CHECK(!Server_Exists(test.patches, "first/p/th06/gone"));
    M_Close(&test);
    Mem_Free(files);
    Mem_Free(kept);
}

TEST(update_replaces_an_interrupted_part_file)
{
    char *const a = Server_Entry("th06/a", "a");
    char *const b = Server_Entry("th06/b", "b");
    char *const files = Str_Format("%s, %s", a, b);
    M_CASE test = M_Prepare("part", files);
    Server_AddFile(test.server, "first", "p", "th06/a", "a");
    PatchBuilder_Write(test.patches, "first/p/th06/a.part", "half");
    PatchBuilder_Write(test.patches, "first/p/th06/b", "b");
    PatchBuilder_Write(test.patches, "first/p/th06/b.part", "stale");
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 1);
    CHECK_EQ_INT(counts.up_to_date, 1);
    M_CheckFile(&test, "first/p/th06/a", "a");
    CHECK(!Server_Exists(test.patches, "first/p/th06/a.part"));
    CHECK(!Server_Exists(test.patches, "first/p/th06/b.part"));
    M_Close(&test);
    Mem_Free(files);
    Mem_Free(b);
    Mem_Free(a);
}

TEST(update_leaves_out_unsafe_paths)
{
    char *const up = Server_Entry("th06/../../escape", "x");
    char *const dot = Server_Entry("th06/./x", "x");
    char *const back = Server_Entry("th06\\\\x", "x");
    char *const files = Str_Format("%s, %s, %s", up, dot, back);
    M_CASE test = M_Prepare("unsafe", files);
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 0);
    CHECK(!Server_Exists(test.patches, "first/escape"));
    M_Close(&test);
    Mem_Free(files);
    Mem_Free(back);
    Mem_Free(dot);
    Mem_Free(up);
}

TEST(update_escapes_file_urls)
{
    char *const files = Server_Entry("th06/a b%.txt", "spaced");
    M_CASE test = M_Prepare("escape", files);
    Server_AddFile(test.server, "first", "p", "th06/a b%.txt", "spaced");
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    M_CheckFile(&test, "first/p/th06/a b%.txt", "spaced");
    M_Close(&test);
    Mem_Free(files);
}

TEST(list_installed_reads_the_update_flag)
{
    char *const dir = TestDir_Prepare("installed");
    PatchBuilder_Write(dir, "r/b/patch.js", "{\"update\": false}");
    PatchBuilder_Write(dir, "r/a/patch.js", "{}");
    PatchBuilder_Write(dir, "r/c/readme.txt", "no patch");
    PatchBuilder_Write(dir, "r/repo.js", "{}");
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    UPDATE_INSTALLED_LIST list;
    CHECK(
        Result_Log(Update_ListInstalled(dir, &arena, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 2);
    if (list.count == 2) {
        CHECK_EQ_STR(list.items[0].id, "r/a");
        CHECK(list.items[0].update);
        CHECK_EQ_STR(list.items[1].id, "r/b");
        CHECK(!list.items[1].update);
    }
    Arena_Free(&arena);
    Mem_Free(dir);
}

TEST(list_installed_of_a_missing_folder_is_empty)
{
    char *const dir = TestDir_Prepare("installed_missing");
    char *const missing = TestDir_Join(dir, "missing");
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    UPDATE_INSTALLED_LIST list;
    CHECK(Result_Log(
        Update_ListInstalled(missing, &arena, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 0);
    Arena_Free(&arena);
    Mem_Free(missing);
    Mem_Free(dir);
}

TEST(remove_deletes_the_patch_files_and_keeps_others)
{
    char *const dir = TestDir_Prepare("remove");
    PatchBuilder_Write(
        dir, "r/p/files.js", "{\"th06/a\": 1, \"th06/sub/b\": 2, \"c\": null}");
    PatchBuilder_Write(dir, "r/p/patch.js", "{}");
    PatchBuilder_Write(dir, "r/p/th06/a", "a");
    PatchBuilder_Write(dir, "r/p/th06/sub/b", "b");
    PatchBuilder_Write(dir, "r/p/th06/sub/b.part", "b");
    PatchBuilder_Write(dir, "r/q/patch.js", "{}");
    PatchBuilder_Write(dir, "r/q/th06/a", "a");
    CHECK(Result_Log(Update_RemovePatch(dir, "r/p"), LOG_LEVEL_ERROR));
    CHECK(!Server_Exists(dir, "r/p"));
    CHECK(Server_Exists(dir, "r/q/th06/a"));
    PatchBuilder_Write(dir, "r/s/files.js", "{\"a\": 1}");
    PatchBuilder_Write(dir, "r/s/patch.js", "{}");
    PatchBuilder_Write(dir, "r/s/mine.txt", "mine");
    CHECK(Result_Log(Update_RemovePatch(dir, "r/s"), LOG_LEVEL_ERROR));
    CHECK(!Server_Exists(dir, "r/s/patch.js"));
    CHECK(Server_Exists(dir, "r/s/mine.txt"));
    Mem_Free(dir);
}

TEST(update_tries_the_servers_in_order)
{
    char *const a = Server_Entry("th06/a", "a");
    char *const b = Server_Entry("th06/b", "b");
    char *const files = Str_Format("%s, %s", a, b);
    M_CASE test = M_Prepare("servers", files);
    char *const server_url = TestDir_FileUrl(test.server);
    char *const members =
        Str_Format("\"servers\": [\"%s/mirror/\"]", server_url);
    Mem_Free(server_url);
    Server_AddPatch(test.server, "first", "p", members);
    Server_AddFile(test.server, "first", "p", "th06/a", "a");
    Server_AddFile(test.server, "first", "p", "th06/b", "b");
    PatchBuilder_Write(test.server, "mirror/files.js", "{}");
    PatchBuilder_Write(test.server, "mirror/th06/a", "a");
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 0);
    char *const mirror_files = Str_Format("{%s, %s}", a, b);
    PatchBuilder_Write(test.server, "mirror/files.js", mirror_files);
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 2);
    M_CheckFile(&test, "first/p/th06/a", "a");
    M_CheckFile(&test, "first/p/th06/b", "b");
    Mem_Free(mirror_files);
    M_Close(&test);
    Mem_Free(members);
    Mem_Free(files);
    Mem_Free(b);
    Mem_Free(a);
}

TEST(update_writes_below_a_folder_longer_than_max_path)
{
    char *dir = TestDir_Prepare("long_path");
    for (int i = 0; i < 8; i++) {
        char *const deeper = TestDir_Join(dir, "a_directory_with_a_long_name");
        Mem_Free(dir);
        dir = deeper;
    }
    char *const files = Server_Entry("th06/紅魔郷.msg.jdiff", "msg");
    M_CASE test = M_PrepareIn(dir, files);
    Server_AddFile(test.server, "first", "p", "th06/紅魔郷.msg.jdiff", "msg");
    CHECK(strlen(test.patches) > 260);
    UPDATE_COUNTS counts;
    CHECK(Result_Log(M_Update(&test, &counts), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(counts.downloaded, 1);
    M_CheckFile(&test, "first/p/th06/紅魔郷.msg.jdiff", "msg");
    M_Close(&test);
    Mem_Free(files);
}
