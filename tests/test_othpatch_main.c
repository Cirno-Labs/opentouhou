#include "othpatch/main.h"

#include "othpatch_server.h"
#include "test.h"
#include "test_dir.h"

typedef struct {
    char *dir;
    char *server;
    char *patches;
    char *repo;
    char *output;
} M_CASE;

static M_CASE M_Prepare(const char *const name)
{
    M_CASE test = { .dir = TestDir_Prepare(name) };
    test.server = TestDir_Join(test.dir, "server");
    test.patches = TestDir_Join(test.dir, "patches");
    test.repo = Str_Format("file://%s/thpatch/repo.js", test.server);
    return test;
}

// Adds the network of a thpatch with two language packs that depend on a
// base patch of another repository.
static void M_AddNetwork(const M_CASE *const test)
{
    const char *const neighbors[] = { "nmlgc" };
    Server_AddRepo(
        test->server, "thpatch", neighbors, 1,
        "\"lang_en\": \"English\", \"lang_de\": \"German\", \"tool\": \"\"");
    Server_AddRepo(test->server, "nmlgc", nullptr, 0, "\"base\": \"\"");
    const char *const packs[] = { "lang_en", "lang_de" };
    for (int i = 0; i < 2; i++) {
        Server_AddPatch(
            test->server, "thpatch", packs[i],
            "\"dependencies\": [\"nmlgc/base\"]");
        char *const entry = Server_Entry("th06/msg1.dat.jdiff", packs[i]);
        char *const files = Str_Format("{%s}", entry);
        Server_AddFile(test->server, "thpatch", packs[i], "files.js", files);
        Server_AddFile(
            test->server, "thpatch", packs[i], "th06/msg1.dat.jdiff", packs[i]);
        Mem_Free(files);
        Mem_Free(entry);
    }
    Server_AddPatch(test->server, "nmlgc", "base", "");
    Server_AddFile(test->server, "nmlgc", "base", "files.js", "{}");
}

// Runs othpatch with --dir and --repo of the test before the arguments,
// and keeps what it writes in test->output.
static int M_Run(M_CASE *const test, const int count, ...)
{
    char *argv[16] = { "othpatch", "--dir", test->patches, "--repo",
                       test->repo };
    int argc = 5;
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        argv[argc++] = va_arg(args, char *);
    }
    va_end(args);
    FILE *const out = tmpfile();
    const int code = Main_Run(argc, argv, out);
    const long size = ftell(out);
    rewind(out);
    Mem_Free(test->output);
    test->output = Mem_AllocZ((size_t)size + 1);
    CHECK(fread(test->output, 1, (size_t)size, out) == (size_t)size);
    fclose(out);
    return code;
}

static void M_CheckOutput(const M_CASE *const test, const char *const text)
{
    if (strstr(test->output, text) == nullptr) {
        TEST_FAIL("output has no \"%s\":\n%s", text, test->output);
    }
}

static void M_Close(M_CASE *const test)
{
    Mem_Free(test->output);
    Mem_Free(test->repo);
    Mem_Free(test->patches);
    Mem_Free(test->server);
    Mem_Free(test->dir);
}

TEST(language_code_installs_the_pack_and_its_dependencies)
{
    M_CASE test = M_Prepare("install");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "Language: thpatch/lang_en (Patch lang_en), named on");
    M_CheckOutput(&test, "nmlgc/base: 0 downloaded, 0 up to date, 0 removed\n");
    M_CheckOutput(&test, "thpatch/lang_en: 1 downloaded, 0 up to date");
    M_CheckOutput(&test, "Done: 2 patches, 1 files downloaded");
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/th06/msg1.dat.jdiff"));
    CHECK(Server_Exists(test.patches, "nmlgc/base/patch.js"));
    M_Close(&test);
}

// SDL reads the locale from the environment on Linux only. macOS and
// Windows ask the system, so these tests cannot choose the locale there.
#ifdef __linux__
static void M_SetLocale(const char *const lang)
{
    SDL_Environment *const environment = SDL_GetEnvironment();
    SDL_UnsetEnvironmentVariable(environment, "LC_ALL");
    SDL_UnsetEnvironmentVariable(environment, "LC_MESSAGES");
    SDL_UnsetEnvironmentVariable(environment, "LANGUAGE");
    SDL_SetEnvironmentVariable(environment, "LANG", lang, true);
}

TEST(no_name_picks_the_language_of_the_locale)
{
    M_CASE test = M_Prepare("locale");
    M_AddNetwork(&test);
    M_SetLocale("de_AT.UTF-8");
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckOutput(
        &test, "thpatch/lang_de (Patch lang_de), from the system locale de_AT");
    M_Close(&test);
}

TEST(no_name_without_a_pack_for_the_locale_picks_english)
{
    M_CASE test = M_Prepare("locale_fallback");
    M_AddNetwork(&test);
    M_SetLocale("fr_FR.UTF-8");
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckOutput(
        &test,
        "thpatch/lang_en (Patch lang_en), the system locale fr_FR "
        "has no language pack");
    M_Close(&test);
}
#endif

TEST(language_switch_removes_the_other_pack)
{
    M_CASE test = M_Prepare("switch");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    CHECK(Server_Exists(test.patches, "thpatch/lang_de/patch.js"));
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test, "thpatch/lang_de: removed, thpatch/lang_en replaces it");
    CHECK(!Server_Exists(test.patches, "thpatch/lang_de"));
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/patch.js"));
    M_Close(&test);
}

TEST(run_updates_installed_patches)
{
    M_CASE test = M_Prepare("installed");
    M_AddNetwork(&test);
    Server_AddPatch(test.server, "thpatch", "tool", "");
    char *const entry = Server_Entry("tool.js", "v1");
    char *const files = Str_Format("{%s}", entry);
    Server_AddFile(test.server, "thpatch", "tool", "files.js", files);
    Server_AddFile(test.server, "thpatch", "tool", "tool.js", "v1");
    CHECK_EQ_INT(M_Run(&test, 1, "tool"), 0);
    M_CheckOutput(&test, "Patch: thpatch/tool");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "thpatch/tool: 0 downloaded, 1 up to date");
    M_CheckOutput(&test, "Done: 3 patches");
    PatchBuilder_Write(
        test.patches, "thpatch/tool/patch.js", "{\"update\": false}");
    PatchBuilder_Write(test.patches, "thpatch/tool/tool.js", "changed");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "Done: 2 patches");
    char *const kept = Server_Read(test.patches, "thpatch/tool/tool.js");
    CHECK_EQ_STR(kept, "changed");
    SDL_free(kept);
    M_Close(&test);
    Mem_Free(files);
    Mem_Free(entry);
}

TEST(run_keeps_installed_patches_that_the_network_lacks)
{
    M_CASE test = M_Prepare("unknown_installed");
    M_AddNetwork(&test);
    PatchBuilder_Write(test.patches, "mine/local/patch.js", "{}");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "mine/local: left as it is, cannot update it: ");
    CHECK(Server_Exists(test.patches, "mine/local/patch.js"));
    M_Close(&test);
}

TEST(list_prints_the_language_packs_sorted)
{
    M_CASE test = M_Prepare("list");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "--list"), 0);
    CHECK_EQ_STR(
        test.output,
        "lang_de              German\n"
        "lang_en              English\n");
    M_Close(&test);
}

TEST(failure_of_the_first_repository_is_one_message)
{
    M_CASE test = M_Prepare("no_repo");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 1);
    M_CheckOutput(&test, "othpatch: cannot read the repository: ");
    M_Close(&test);
}

TEST(failure_of_a_missing_patch_is_one_message)
{
    M_CASE test = M_Prepare("missing_patch");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "ja"), 1);
    M_CheckOutput(
        &test,
        "othpatch: lang_ja: no repository in the network has a patch "
        "lang_ja\n");
    M_Close(&test);
}

TEST(failure_of_a_missing_dependency_names_it)
{
    M_CASE test = M_Prepare("missing_dependency");
    M_AddNetwork(&test);
    Server_AddPatch(
        test.server, "thpatch", "lang_en",
        "\"dependencies\": [\"nmlgc/gone\"]");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 1);
    M_CheckOutput(
        &test,
        "othpatch: lang_en: dependency nmlgc/gone of thpatch/lang_en: "
        "repository nmlgc has no patch gone\n");
    M_Close(&test);
}

TEST(failure_of_a_download_is_counted)
{
    M_CASE test = M_Prepare("download_failure");
    M_AddNetwork(&test);
    Server_AddFile(
        test.server, "thpatch", "lang_en", "th06/msg1.dat.jdiff", "x");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 1);
    M_CheckOutput(&test, "thpatch/lang_en: failed: 1 files did not arrive: ");
    M_CheckOutput(
        &test, "othpatch: 1 patch failed; run othpatch again to retry\n");
    M_Close(&test);
}

TEST(failed_switch_keeps_the_old_pack)
{
    M_CASE test = M_Prepare("switch_failure");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    Server_AddFile(
        test.server, "thpatch", "lang_en", "th06/msg1.dat.jdiff", "x");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 1);
    CHECK(Server_Exists(test.patches, "thpatch/lang_de/patch.js"));
    M_Close(&test);
}

TEST(bad_arguments_fail_with_one_message)
{
    M_CASE test = M_Prepare("arguments");
    CHECK_EQ_INT(M_Run(&test, 1, "--bogus"), 1);
    CHECK_EQ_STR(
        test.output,
        "othpatch: unknown option --bogus; --help lists the options\n");
    CHECK_EQ_INT(M_Run(&test, 2, "en", "de"), 1);
    CHECK_EQ_STR(test.output, "othpatch: name one patch only, not en and de\n");
    char *argv[] = { "othpatch", "--dir" };
    FILE *const out = tmpfile();
    CHECK_EQ_INT(Main_Run(2, argv, out), 1);
    fclose(out);
    M_Close(&test);
}

TEST(help_and_version_print_and_succeed)
{
    M_CASE test = M_Prepare("help");
    CHECK_EQ_INT(M_Run(&test, 1, "--help"), 0);
    M_CheckOutput(&test, "Usage: othpatch");
    CHECK_EQ_INT(M_Run(&test, 1, "--version"), 0);
    M_CheckOutput(&test, "othpatch ");
    M_Close(&test);
}
