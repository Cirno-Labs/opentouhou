#include "core/patch/manager.h"

#include "patch_server.h"
#include "test.h"
#include "test_dir.h"

#include <stdarg.h>
#include <string.h>

static const char *const m_Games[] = { "th06" };

typedef struct {
    char *dir;
    char *server;
    char *patches;
    PATCH_MANAGER *manager;
    // Holds the events of the test, one line each.
    char *log;
} M_CASE;

PRINTF_FORMAT(2, 3)
static void M_Log(M_CASE *const test, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const line = Str_FormatV(fmt, args);
    va_end(args);
    char *const longer = Str_Format("%s%s\n", test->log, line);
    Mem_Free(line);
    Mem_Free(test->log);
    test->log = longer;
}

static void M_Started(
    void *const user, const PATCH_ENTRY *const patch, const int32_t files)
{
    M_Log(user, "started %s (%s) %d", patch->id, patch->title, files);
}

static void M_FileDone(
    void *const user, const PATCH_ENTRY *const patch, const char *const file)
{
    M_Log(user, "file %s %s", patch->id, file);
}

static void M_Done(
    void *const user, const PATCH_ENTRY *const patch,
    const PATCH_COUNTS *const counts)
{
    M_Log(
        user, "done %s %d %d %d", patch->id, counts->downloaded,
        counts->up_to_date, counts->removed);
}

static void M_Failed(
    void *const user, const PATCH_ENTRY *const patch, const char *const reason)
{
    M_Log(user, "failed %s: %s", patch->id, reason);
}

static void M_Queued(void *const user, const PATCH_ENTRY *const patch)
{
    M_Log(user, "queued %s", patch->id);
}

static void M_Kept(
    void *const user, const char *const id, const char *const reason)
{
    M_Log(user, "kept %s", id);
}

static void M_Removed(void *const user, const char *const id)
{
    M_Log(user, "removed %s", id);
}

static PATCH_EVENTS M_Events(M_CASE *const test)
{
    return (PATCH_EVENTS) {
        .user = test,
        .patch_queued = M_Queued,
        .patch_started = M_Started,
        .file_done = M_FileDone,
        .patch_done = M_Done,
        .patch_failed = M_Failed,
        .patch_kept = M_Kept,
        .patch_removed = M_Removed,
    };
}

// Adds the patch name to the repository repo of the server, with members
// for patch.js and one file below th06/.
static void M_AddPatch(
    const M_CASE *const test, const char *const repo, const char *const name,
    const char *const members)
{
    Server_AddPatch(test->server, repo, name, members);
    char *const file = Str_Format("th06/%s.txt", name);
    char *const entry = Server_Entry(file, name);
    char *const files = Str_Format("{%s}", entry);
    Server_AddFile(test->server, repo, name, "files.js", files);
    Server_AddFile(test->server, repo, name, file, name);
    Mem_Free(files);
    Mem_Free(entry);
    Mem_Free(file);
}

// Prepares a server with the repository "thpatch", whose patch tool needs lib
// and lang_en, and the repository "b", a neighbor of "thpatch" that has a tool
// too.
static M_CASE M_Prepare(const char *const name)
{
    M_CASE test = { .dir = TestDir_Prepare(name), .log = Str_Dup("") };
    test.server = TestDir_Join(test.dir, "server");
    test.patches = TestDir_Join(test.dir, "patches");
    const char *const neighbors[] = { "b" };
    Server_AddRepo(
        test.server, "thpatch", neighbors, 1,
        "\"tool\": \"\", \"lib\": \"\", \"lang_en\": \"English\", \"own\": "
        "\"\"");
    Server_AddRepo(test.server, "b", nullptr, 0, "\"tool\": \"\"");
    M_AddPatch(
        &test, "thpatch", "tool", "\"dependencies\": [\"lib\", \"lang_en\"]");
    M_AddPatch(&test, "thpatch", "lib", "");
    M_AddPatch(&test, "thpatch", "lang_en", "");
    M_AddPatch(&test, "thpatch", "own", "");
    M_AddPatch(&test, "b", "tool", "");
    char *const server_url = TestDir_FileUrl(test.server);
    char *const url = Str_Format("%s/thpatch/", server_url);
    test.manager =
        PatchManager_Open(test.patches, url, m_Games, 1, Server_Fetch);
    Mem_Free(url);
    Mem_Free(server_url);
    return test;
}

static void M_Close(M_CASE *const test)
{
    PatchManager_Close(test->manager);
    Mem_Free(test->log);
    Mem_Free(test->patches);
    Mem_Free(test->server);
    Mem_Free(test->dir);
}

static void M_CheckLog(M_CASE *const test, const char *const expected)
{
    CHECK_EQ_STR(test->log, expected);
    Mem_Free(test->log);
    test->log = Str_Dup("");
}

static void M_CheckFailure(const RESULT result, const char *const message)
{
    CHECK(!IS_OK(result));
    CHECK_EQ_STR(result.message, message);
    Result_Ignore(result);
}

TEST(install_brings_the_patch_and_what_it_needs)
{
    M_CASE test = M_Prepare("install");
    const PATCH_EVENTS events = M_Events(&test);
    CHECK_EQ_INT(PatchManager_GetCount(test.manager), 0);
    CHECK(Result_Log(
        PatchManager_Install(test.manager, "thpatch/tool", &events),
        LOG_LEVEL_ERROR));
    M_CheckLog(
        &test,
        "queued thpatch/tool\n"
        "queued thpatch/lang_en\n"
        "queued thpatch/lib\n"
        "started thpatch/tool (Patch tool) 1\n"
        "file thpatch/tool th06/tool.txt\n"
        "done thpatch/tool 1 0 0\n"
        "started thpatch/lang_en (Patch lang_en) 1\n"
        "file thpatch/lang_en th06/lang_en.txt\n"
        "done thpatch/lang_en 1 0 0\n"
        "started thpatch/lib (Patch lib) 1\n"
        "file thpatch/lib th06/lib.txt\n"
        "done thpatch/lib 1 0 0\n");
    CHECK(Server_Exists(test.patches, "thpatch/tool/th06/tool.txt"));
    CHECK_EQ_INT(PatchManager_GetCount(test.manager), 3);
    CHECK_EQ_STR(PatchManager_Get(test.manager, 2)->id, "thpatch/tool");
    CHECK_EQ_STR(PatchManager_Get(test.manager, 2)->title, "Patch tool");
    M_Close(&test);
}

TEST(install_of_a_bare_name_of_two_repositories_names_each)
{
    M_CASE test = M_Prepare("ambiguous");
    const PATCH_EVENTS events = M_Events(&test);
    M_CheckFailure(
        PatchManager_Install(test.manager, "tool", &events),
        "2 repositories have a patch tool: thpatch/tool, b/tool. Name one as "
        "<repo>/<patch>");
    CHECK(Result_Log(
        PatchManager_Install(test.manager, "own", &events), LOG_LEVEL_ERROR));
    CHECK(Server_Exists(test.patches, "thpatch/own/patch.js"));
    M_CheckFailure(
        PatchManager_Install(test.manager, "thpatch/gone", &events),
        "could not install thpatch/gone: repository thpatch has no patch gone");
    M_CheckFailure(
        PatchManager_Install(test.manager, "gone", &events),
        "could not install gone: no repository in the network has a patch "
        "gone");
    M_Close(&test);
}

TEST(install_names_the_full_id_of_a_bare_name)
{
    M_CASE test = M_Prepare("bare_dependency");
    Server_AddPatch(
        test.server, "thpatch", "own", "\"dependencies\": [\"b/gone\"]");
    const PATCH_EVENTS events = M_Events(&test);
    M_CheckFailure(
        PatchManager_Install(test.manager, "own", &events),
        "could not install thpatch/own: dependency b/gone of thpatch/own: "
        "repository b has no patch gone");
    M_CheckLog(&test, "");
    CHECK(!Server_Exists(test.patches, "thpatch"));
    M_Close(&test);
}

TEST(list_patches_lists_the_first_repository_in_order)
{
    M_CASE test = M_Prepare("list");
    const PATCH_LISTED *listed;
    int32_t count;
    CHECK(Result_Log(
        PatchManager_ListPatches(test.manager, &listed, &count),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(count, 4);
    CHECK_EQ_STR(listed[0].id, "thpatch/lang_en");
    CHECK_EQ_STR(listed[0].title, "English");
    CHECK_EQ_STR(listed[1].id, "thpatch/lib");
    CHECK_EQ_STR(listed[1].title, "");
    CHECK_EQ_STR(listed[2].id, "thpatch/own");
    CHECK_EQ_STR(listed[3].id, "thpatch/tool");
    CHECK(!Server_Exists(test.patches, "thpatch"));
    M_Close(&test);
}

TEST(list_patches_fails_without_the_first_repository)
{
    M_CASE test = M_Prepare("list_none");
    PatchManager_Close(test.manager);
    test.manager = PatchManager_Open(
        test.patches, "file:///nowhere/", m_Games, 1, Server_Fetch);
    const PATCH_LISTED *listed;
    int32_t count;
    const RESULT result =
        PatchManager_ListPatches(test.manager, &listed, &count);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK_EQ_INT(count, 0);
    M_Close(&test);
}

TEST(install_of_a_language_pack_without_game_files_fails)
{
    M_CASE test = M_Prepare("no_game_files");
    char *const entry = Server_Entry("th07/lang_en.txt", "x");
    char *const files = Str_Format("{%s}", entry);
    Server_AddFile(test.server, "thpatch", "lang_en", "files.js", files);
    Mem_Free(files);
    Mem_Free(entry);
    const PATCH_EVENTS events = M_Events(&test);
    M_CheckFailure(
        PatchManager_Install(test.manager, "thpatch/lang_en", &events),
        "Patch lang_en (thpatch/lang_en) has no files for oth06");
    M_CheckLog(&test, "");
    CHECK(!Server_Exists(test.patches, "thpatch"));
    CHECK(Result_Log(
        PatchManager_Install(test.manager, "thpatch/own", &events),
        LOG_LEVEL_ERROR));
    M_Close(&test);
}

TEST(install_reports_the_patch_that_failed)
{
    M_CASE test = M_Prepare("failed");
    char *const entry = Server_Entry("th06/missing.txt", "x");
    char *const files = Str_Format("{%s}", entry);
    Server_AddFile(test.server, "thpatch", "lib", "files.js", files);
    Mem_Free(files);
    Mem_Free(entry);
    const PATCH_EVENTS events = M_Events(&test);
    const RESULT result =
        PatchManager_Install(test.manager, "thpatch/tool", &events);
    CHECK(!IS_OK(result));
    CHECK(Str_StartsWith(
        result.message,
        "could not download thpatch/lib: 1 files did not "
        "arrive: "));
    Result_Ignore(result);
    CHECK(
        strstr(
            test.log,
            "started thpatch/lib (Patch lib) 1\n"
            "failed thpatch/lib: 1 files did not arrive: ")
        != nullptr);
    CHECK(strstr(test.log, "done thpatch/lib") == nullptr);
    CHECK(!Server_Exists(test.patches, "thpatch/lib/patch.js"));
    M_Close(&test);
}

TEST(update_skips_what_this_manager_updated_and_keeps_the_unknown)
{
    M_CASE test = M_Prepare("update");
    const PATCH_EVENTS events = M_Events(&test);
    PatchBuilder_Write(test.patches, "mine/local/patch.js", "{}");
    PatchBuilder_Write(
        test.patches, "thpatch/off/patch.js", "{\"update\": false}");
    CHECK(Result_Log(
        PatchManager_Install(test.manager, "thpatch/lib", &events),
        LOG_LEVEL_ERROR));
    M_CheckLog(
        &test,
        "queued thpatch/lib\n"
        "kept mine/local\n"
        "started thpatch/lib (Patch lib) 1\n"
        "file thpatch/lib th06/lib.txt\n"
        "done thpatch/lib 1 0 0\n");
    CHECK(Result_Log(
        PatchManager_Update(test.manager, &events), LOG_LEVEL_ERROR));
    M_CheckLog(&test, "kept mine/local\n");
    PatchManager_Close(test.manager);
    char *const server_url = TestDir_FileUrl(test.server);
    char *const url = Str_Format("%s/thpatch/repo.js", server_url);
    test.manager =
        PatchManager_Open(test.patches, url, m_Games, 1, Server_Fetch);
    Mem_Free(url);
    Mem_Free(server_url);
    CHECK(Result_Log(
        PatchManager_Update(test.manager, &events), LOG_LEVEL_ERROR));
    M_CheckLog(
        &test, "kept mine/local\nqueued thpatch/lib\ndone thpatch/lib 0 1 0\n");
    M_Close(&test);
}

TEST(update_without_a_patch_that_updates_fetches_nothing)
{
    M_CASE test = M_Prepare("update_none");
    const PATCH_EVENTS events = M_Events(&test);
    PatchBuilder_Write(
        test.patches, "thpatch/off/patch.js", "{\"update\": false}");
    PatchManager_Close(test.manager);
    test.manager = PatchManager_Open(
        test.patches, "file:///nowhere/", m_Games, 1, Server_Fetch);
    CHECK(Result_Log(
        PatchManager_Update(test.manager, &events), LOG_LEVEL_ERROR));
    M_CheckLog(&test, "");
    M_Close(&test);
}

TEST(remove_takes_what_only_it_needs_but_no_language_pack)
{
    M_CASE test = M_Prepare("remove");
    const PATCH_EVENTS events = M_Events(&test);
    CHECK(Result_Log(
        PatchManager_Install(test.manager, "thpatch/tool", &events),
        LOG_LEVEL_ERROR));
    Mem_Free(test.log);
    test.log = Str_Dup("");
    M_CheckFailure(
        PatchManager_Remove(test.manager, "thpatch/lib", &events),
        "thpatch/tool needs thpatch/lib");
    M_CheckFailure(
        PatchManager_Remove(test.manager, "thpatch/gone", &events),
        "there is no installed patch thpatch/gone");
    CHECK(Result_Log(
        PatchManager_Remove(test.manager, "thpatch/tool", &events),
        LOG_LEVEL_ERROR));
    M_CheckLog(&test, "removed thpatch/lib\nremoved thpatch/tool\n");
    CHECK(!Server_Exists(test.patches, "thpatch/tool"));
    CHECK(!Server_Exists(test.patches, "thpatch/lib/patch.js"));
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/patch.js"));
    CHECK_EQ_INT(PatchManager_GetCount(test.manager), 1);
    M_Close(&test);
}
