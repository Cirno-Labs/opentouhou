#include "othpatch/install.h"
#include "othpatch/main.h"
#include "othpatch/sha256.h"

#include "othpatch_server.h"
#include "test.h"
#include "test_dir.h"
#include "zip_builder.h"

#include <sys/stat.h>

typedef struct {
    char *dir;
    char *server;
    char *patches;
    char *program;
    char *repo;
    char *releases;
    char *othpatch_releases;
    // Holds the version of the running othpatch.
    const char *version;
    // Holds the answer to the question of othpatch, or nullptr for a run
    // without a terminal.
    const char *answer;
    char *output;
} M_CASE;

// Prepares a test whose othpatch is in a folder of its own, with the file
// beside_path beside it, or with nothing beside it when beside_path is
// nullptr.
static M_CASE M_PrepareBeside(
    const char *const name, const char *const beside_path)
{
    M_CASE test = { .dir = TestDir_Prepare(name) };
    test.server = TestDir_Join(test.dir, "server");
    test.patches = TestDir_Join(test.dir, "patches");
    test.program = TestDir_Join(test.dir, "program");
    test.repo = Str_Format("file://%s/thpatch/repo.js", test.server);
    test.releases = Str_Format("file://%s/releases.json", test.server);
    test.othpatch_releases =
        Str_Format("file://%s/othpatch_releases.json", test.server);
    test.version = "0.1.0";
    PatchBuilder_Write(test.program, Install_GetPlatform()->othpatch, "");
    PatchBuilder_Write(
        test.server, "othpatch_releases.json",
        "[{\"tag_name\": \"othpatch-v0.1.0\", \"draft\": false, "
        "\"prerelease\": false, \"assets\": []}]");
    if (beside_path != nullptr) {
        PatchBuilder_Write(test.program, beside_path, "");
    }
    return test;
}

static M_CASE M_Prepare(const char *const name)
{
    return M_PrepareBeside(name, "oth06");
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
    char *argv[16] = {
        "othpatch",
        "--dir",
        test->patches,
        "--repo",
        test->repo,
        "--releases",
        test->releases,
        "--othpatch-releases",
        test->othpatch_releases,
    };
    int argc = 9;
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        argv[argc++] = va_arg(args, char *);
    }
    va_end(args);
    FILE *in = nullptr;
    if (test->answer != nullptr) {
        in = tmpfile();
        fputs(test->answer, in);
        rewind(in);
    }
    FILE *const out = tmpfile();
    const int code =
        Main_Run(argc, argv, test->version, test->program, in, out);
    if (in != nullptr) {
        fclose(in);
    }
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
    Mem_Free(test->othpatch_releases);
    Mem_Free(test->releases);
    Mem_Free(test->repo);
    Mem_Free(test->program);
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
    M_CheckOutput(&test, "nmlgc/base: up to date\n");
    M_CheckOutput(&test, "thpatch/lang_en: 1 downloaded, 0 up to date");
    M_CheckOutput(&test, "Done: 2 patches, 1 files downloaded");
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/th06/msg1.dat.jdiff"));
    CHECK(Server_Exists(test.patches, "nmlgc/base/patch.js"));
    M_Close(&test);
}

TEST(game_beside_othpatch_is_downloaded)
{
    M_CASE test = M_PrepareBeside("beside", "oth06.AppImage");
    M_AddNetwork(&test);
    char *const present = Server_Entry("th06/msg1.dat.jdiff", "lang_en");
    char *const absent = Server_Entry("th07/msg1.dat.jdiff", "other");
    char *const entries = Str_Format("{%s, %s}", present, absent);
    Server_AddFile(test.server, "thpatch", "lang_en", "files.js", entries);
    Server_AddFile(
        test.server, "thpatch", "lang_en", "th07/msg1.dat.jdiff", "other");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "\nGames: th06 (beside othpatch)\nLanguage: ");
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/th06/msg1.dat.jdiff"));
    CHECK(!Server_Exists(test.patches, "thpatch/lang_en/th07"));
    M_Close(&test);
    Mem_Free(entries);
    Mem_Free(absent);
    Mem_Free(present);
}

TEST(game_bundle_beside_othpatch_is_downloaded)
{
    M_CASE test =
        M_PrepareBeside("beside_bundle", "oth06.app/Contents/MacOS/oth06");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "Games: th06 (beside othpatch)\n");
    M_Close(&test);
}

TEST(game_in_the_patches_folder_is_downloaded)
{
    M_CASE test = M_PrepareBeside("downloaded", nullptr);
    M_AddNetwork(&test);
    PatchBuilder_Write(test.patches, "mine/local/patch.js", "{}");
    PatchBuilder_Write(test.patches, "mine/local/th06/old.txt", "");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "Games: th06 (in the patches folder)\n");
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/th06/msg1.dat.jdiff"));
    M_Close(&test);
}

TEST(game_named_with_an_option_is_downloaded)
{
    M_CASE test = M_PrepareBeside("named_game", nullptr);
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 3, "--game", "th06", "en"), 0);
    M_CheckOutput(&test, "Games: th06 (named on the command line)\n");
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/th06/msg1.dat.jdiff"));
    M_Close(&test);
}

TEST(failure_of_an_unknown_game_is_one_message)
{
    M_CASE test = M_Prepare("unknown_game");
    CHECK_EQ_INT(M_Run(&test, 3, "--game", "th99", "en"), 1);
    CHECK_EQ_STR(
        test.output, "othpatch: unknown game th99; othpatch knows th06\n");
    M_Close(&test);
}

TEST(failure_without_a_game_is_one_message)
{
    M_CASE test = M_PrepareBeside("no_game", "oth07");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 1);
    M_CheckOutput(
        &test,
        "othpatch: no opentouhou game is beside othpatch; name one with "
        "--game th06\n");
    CHECK(!Server_Exists(test.patches, "thpatch"));
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
    M_CheckOutput(&test, "thpatch/tool: up to date\n");
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
    CHECK_EQ_INT(
        Main_Run(2, argv, test.version, test.program, nullptr, out), 1);
    fclose(out);
    M_Close(&test);
}

TEST(help_and_version_print_and_succeed)
{
    M_CASE test = M_Prepare("help");
    CHECK_EQ_INT(M_Run(&test, 1, "--help"), 0);
    M_CheckOutput(&test, "Usage: othpatch");
    CHECK_EQ_INT(M_Run(&test, 1, "--version"), 0);
    M_CheckOutput(&test, "othpatch 0.1.0\n");
    M_Close(&test);
}

// Writes a game beside othpatch, a shell script in the form of the
// platform's packages that prints version.
static void M_WriteGame(const M_CASE *const test, const char *const version)
{
    ARENA arena;
    Arena_Init(&arena, "main test", 1 << 16);
    const char *const path = Install_GetGamePath(
        Install_GetPlatform(), test->program, "th06", &arena);
    const char *const relative = path + strlen(test->program) + 1;
    char *const script = Str_Format("#!/bin/sh\necho \"th06 %s\"\n", version);
    PatchBuilder_Write(test->program, relative, script);
    CHECK(chmod(path, 0755) == 0);
    Mem_Free(script);
    Arena_Free(&arena);
}

// Returns the contents of the game beside othpatch. SDL_free releases
// them.
static char *M_ReadGame(const M_CASE *const test)
{
    ARENA arena;
    Arena_Init(&arena, "main test", 1 << 16);
    char *const data = SDL_LoadFile(
        Install_GetGamePath(
            Install_GetPlatform(), test->program, "th06", &arena),
        nullptr);
    Arena_Free(&arena);
    return data;
}

// Returns an othpatch that prints version, a shell script.
static char *M_Othpatch(const char *const version)
{
    return Str_Format("#!/bin/sh\necho \"othpatch %s\"\n", version);
}

static void M_CheckOthpatch(const M_CASE *const test, const char *const text)
{
    char *const othpatch =
        Server_Read(test->program, Install_GetPlatform()->othpatch);
    CHECK_EQ_STR(othpatch, text);
    SDL_free(othpatch);
}

// Writes the package of release version to the server, with the game
// that prints version and an othpatch that prints othpatch_version, and
// returns its digest.
static void M_WritePackage(
    const M_CASE *const test, const char *const version,
    const char *const othpatch_version, char digest[SHA256_HEX_SIZE])
{
    const INSTALL_PLATFORM *const platform = Install_GetPlatform();
    char *const script = Str_Format("#!/bin/sh\necho \"th06 %s\"\n", version);
    char *const othpatch = M_Othpatch(othpatch_version);
    char *const game = platform->bundle
        ? Str_Format("oth06%s/Contents/MacOS/oth06", platform->game_suffix)
        : Str_Format("oth06%s", platform->game_suffix);
    const ZIP_BUILDER_ENTRY entries[] = {
        { game, script, strlen(script), 0100755, true },
        { platform->othpatch, othpatch, strlen(othpatch), 0100755, true },
        { "README.txt", "readme", 6, 0100644, true },
    };
    const ZIP_BUILDER_ARCHIVE archive = ZipBuilder_Build(entries, 3);
    char *const path =
        Str_Format("%s/oth06-%s-%s.zip", test->server, version, platform->name);
    CHECK(SDL_SaveFile(path, archive.data, archive.size));
    Sha256_ComputeHex(archive.data, archive.size, digest);
    Mem_Free(path);
    Mem_Free(archive.data);
    Mem_Free(game);
    Mem_Free(othpatch);
    Mem_Free(script);
}

// Writes the release list with one release of th06, tagged
// th06-v<version>, whose package has digest, or no digest when digest is
// nullptr. A release without assets has no package.
static void M_WriteReleases(
    const M_CASE *const test, const char *const version,
    const char *const digest, const bool assets)
{
    const char *const platform = Install_GetPlatform()->name;
    char *const digest_member = digest != nullptr
        ? Str_Format(", \"digest\": \"sha256:%s\"", digest)
        : Str_Dup("");
    char *const asset = assets
        ? Str_Format(
              "{\"name\": \"oth06-%s-%s.zip\", \"url\": "
              "\"file://%s/oth06-%s-%s.zip\"%s}",
              version, platform, test->server, version, platform, digest_member)
        : Str_Dup("");
    char *const text = Str_Format(
        "[{\"tag_name\": \"th06-v9.9.9\", \"draft\": false, "
        "\"prerelease\": true, \"assets\": []}, "
        "{\"tag_name\": \"th06-v%s\", \"draft\": false, \"prerelease\": "
        "false, \"assets\": [%s]}]",
        version, asset);
    PatchBuilder_Write(test->server, "releases.json", text);
    Mem_Free(text);
    Mem_Free(asset);
    Mem_Free(digest_member);
}

static M_CASE M_PrepareUpdate(
    const char *const name, const char *const installed,
    const char *const released)
{
    M_CASE test = M_PrepareBeside(name, nullptr);
    M_AddNetwork(&test);
    M_WriteGame(&test, installed);
    char digest[SHA256_HEX_SIZE];
    M_WritePackage(&test, released, "0.5.0", digest);
    M_WriteReleases(&test, released, digest, true);
    return test;
}

static void M_CheckGame(const M_CASE *const test, const char *const version)
{
    char *const game = M_ReadGame(test);
    char *const expected = Str_Format("#!/bin/sh\necho \"th06 %s\"\n", version);
    CHECK_EQ_STR(game, expected);
    Mem_Free(expected);
    SDL_free(game);
}

TEST(newer_release_without_a_terminal_is_only_announced)
{
    M_CASE test = M_PrepareUpdate("update_notice", "0.1.0", "0.2.0");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "Done: 2 patches");
    M_CheckOutput(
        &test,
        "Checking for updates...\n"
        "oth06 0.2.0 is available (you have 0.1.0); run othpatch --update "
        "to install it\n");
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(update_option_installs_the_game_and_othpatch)
{
    M_CASE test = M_PrepareUpdate("update_option", "0.1.0", "0.10.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        "oth06 0.10.0 is available (you have 0.1.0); installing it\n"
        "oth06 0.10.0 is installed\n"
        "othpatch 0.5.0 is installed too; the next run uses it\n"
        "othpatch 0.5.0 is up to date\n");
    M_CheckGame(&test, "0.10.0");
    char *const othpatch = M_Othpatch("0.5.0");
    M_CheckOthpatch(&test, othpatch);
    Mem_Free(othpatch);
    CHECK(!Server_Exists(test.program, "README.txt"));
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "oth06 0.10.0 is up to date\n");
    M_Close(&test);
}

TEST(yes_at_the_question_installs_the_game)
{
    M_CASE test = M_PrepareUpdate("update_yes", "0.1.0", "0.2.0");
    test.answer = " Yes\n";
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test,
        "oth06 0.2.0 is available (you have 0.1.0). Update now? [y/N] "
        "oth06 0.2.0 is installed\n");
    M_CheckGame(&test, "0.2.0");
    M_Close(&test);
}

TEST(other_answers_at_the_question_skip_the_update)
{
    M_CASE test = M_PrepareUpdate("update_no", "0.1.0", "0.2.0");
    const char *const answers[] = { "\n", "n\n", "yess\n", "" };
    for (size_t i = 0; i < ARRAY_SIZE(answers); i++) {
        test.answer = answers[i];
        CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
        M_CheckOutput(&test, "Update now? [y/N] ");
        M_CheckGame(&test, "0.1.0");
    }
    M_Close(&test);
}

TEST(same_or_older_release_is_up_to_date)
{
    M_CASE test = M_PrepareUpdate("update_same", "0.2.0", "0.2.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test, "Checking for updates...\noth06 0.2.0 is up to date\n");
    M_Close(&test);
    test = M_PrepareUpdate("update_older", "0.3.0", "0.2.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(&test, "oth06 0.3.0 is up to date\n");
    M_CheckGame(&test, "0.3.0");
    M_Close(&test);
}

TEST(development_build_is_never_updated)
{
    M_CASE test = M_PrepareUpdate("update_dev", "0.1.0-3-gabcdef0", "0.2.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        "oth06 0.1.0-3-gabcdef0 is a development build; othpatch "
        "updates releases only\n");
    M_CheckGame(&test, "0.1.0-3-gabcdef0");
    M_Close(&test);
}

TEST(digest_mismatch_fails_the_update)
{
    M_CASE test = M_PrepareUpdate("update_digest", "0.1.0", "0.2.0");
    M_WriteReleases(
        &test, "0.2.0",
        "0000000000000000000000000000000000000000000000000000000000000000",
        true);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 1);
    M_CheckOutput(&test, "oth06: cannot install 0.2.0: oth06-0.2.0-");
    M_CheckOutput(&test, " instead of 0000");
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(release_without_a_package_is_announced)
{
    M_CASE test = M_PrepareUpdate("update_no_asset", "0.1.0", "0.2.0");
    M_WriteReleases(&test, "0.2.0", nullptr, false);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test, "oth06 0.2.0 is available, but its release has no oth06-0.2.0-");
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(release_without_a_digest_installs)
{
    M_CASE test = M_PrepareUpdate("update_no_digest", "0.1.0", "0.2.0");
    M_WriteReleases(&test, "0.2.0", nullptr, true);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckGame(&test, "0.2.0");
    M_Close(&test);
}

TEST(failed_check_does_not_fail_the_patches)
{
    M_CASE test = M_PrepareBeside("update_offline", nullptr);
    M_AddNetwork(&test);
    M_WriteGame(&test, "0.1.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(&test, "Done: 2 patches");
    M_CheckOutput(&test, "Checking for updates...\nCannot check for updates: ");
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(game_that_gives_no_version_is_skipped)
{
    M_CASE test = M_PrepareUpdate("update_broken", "0.1.0", "0.2.0");
    ARENA arena;
    Arena_Init(&arena, "main test", 1 << 16);
    CHECK(
        chmod(
            Install_GetGamePath(
                Install_GetPlatform(), test.program, "th06", &arena),
            0644)
        == 0);
    Arena_Free(&arena);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(&test, "oth06: cannot read its version: ");
    M_Close(&test);
}

TEST(leftovers_of_an_update_are_removed)
{
    M_CASE test = M_PrepareUpdate("update_leftovers", "0.1.0", "0.1.0");
    const INSTALL_PLATFORM *const platform = Install_GetPlatform();
    char *const old = Str_Format("%s.old", platform->othpatch);
    char *const staged = Str_Format("oth06%s.new", platform->game_suffix);
    PatchBuilder_Write(test.program, old, "x");
    PatchBuilder_Write(test.program, staged, "x");
    PatchBuilder_Write(test.program, "notes.old", "x");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    CHECK(!Server_Exists(test.program, old));
    CHECK(!Server_Exists(test.program, staged));
    CHECK(Server_Exists(test.program, "notes.old"));
    Mem_Free(staged);
    Mem_Free(old);
    M_Close(&test);
}

TEST(older_othpatch_of_a_game_package_is_skipped)
{
    M_CASE test = M_PrepareUpdate("update_older_othpatch", "0.1.0", "0.2.0");
    test.version = "0.6.0";
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test, "oth06 0.2.0 is installed\nothpatch 0.6.0 is up to date\n");
    M_CheckGame(&test, "0.2.0");
    M_CheckOthpatch(&test, "");
    M_Close(&test);
}

TEST(development_othpatch_is_never_replaced)
{
    M_CASE test = M_PrepareUpdate("update_dev_othpatch", "0.1.0", "0.2.0");
    test.version = "0.1.0-3-gabcdef0";
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        "oth06 0.2.0 is installed\n"
        "othpatch 0.1.0-3-gabcdef0 is a development build; othpatch updates "
        "releases only\n");
    M_CheckGame(&test, "0.2.0");
    M_CheckOthpatch(&test, "");
    M_Close(&test);
}

// Writes the release list of othpatch with release v<version>, whose
// executable for the platform holds contents and has digest, or the digest
// of contents when digest is nullptr.
static void M_WriteOthpatchRelease(
    const M_CASE *const test, const char *const version,
    const char *const contents, const char *const digest)
{
    const INSTALL_PLATFORM *const platform = Install_GetPlatform();
    char *const name = Str_Format(
        "othpatch-%s-%s%s", version, platform->name,
        platform->othpatch + strlen("othpatch"));
    PatchBuilder_Write(test->server, name, contents);
    char computed[SHA256_HEX_SIZE];
    Sha256_ComputeHex(contents, strlen(contents), computed);
    char *const text = Str_Format(
        "[{\"tag_name\": \"othpatch-v9.9.9\", \"draft\": true, "
        "\"prerelease\": false, \"assets\": []}, "
        "{\"tag_name\": \"th06-v9.9.9\", \"draft\": false, "
        "\"prerelease\": false, \"assets\": []}, "
        "{\"tag_name\": \"othpatch-v%s\", \"draft\": false, "
        "\"prerelease\": false, "
        "\"assets\": [{\"name\": \"%s\", \"url\": \"file://%s/%s\", "
        "\"digest\": \"sha256:%s\"}]}]",
        version, name, test->server, name,
        digest != nullptr ? digest : computed);
    PatchBuilder_Write(test->server, "othpatch_releases.json", text);
    Mem_Free(text);
    Mem_Free(name);
}

TEST(newer_othpatch_release_is_installed)
{
    M_CASE test = M_PrepareBeside("self_update", nullptr);
    M_AddNetwork(&test);
    M_WriteOthpatchRelease(&test, "0.2.0", "othpatch 0.2.0", nullptr);
    CHECK_EQ_INT(M_Run(&test, 3, "--game", "th06", "en"), 0);
    M_CheckOutput(
        &test,
        "Checking for updates...\n"
        "othpatch 0.2.0 is available (you have 0.1.0); run othpatch --update "
        "to install it\n");
    M_CheckOthpatch(&test, "");
    test.answer = "y\n";
    CHECK_EQ_INT(M_Run(&test, 3, "--game", "th06", "en"), 0);
    M_CheckOutput(
        &test,
        "othpatch 0.2.0 is available (you have 0.1.0). Update now? [y/N] "
        "othpatch 0.2.0 is installed; the next run uses it\n");
    M_CheckOthpatch(&test, "othpatch 0.2.0");
    M_Close(&test);
}

TEST(same_othpatch_release_is_up_to_date)
{
    M_CASE test = M_PrepareBeside("self_same", nullptr);
    M_AddNetwork(&test);
    M_WriteOthpatchRelease(&test, "0.2.0", "othpatch 0.2.0", nullptr);
    test.version = "0.2.0";
    CHECK_EQ_INT(M_Run(&test, 4, "--update", "--game", "th06", "en"), 0);
    M_CheckOutput(
        &test, "Checking for updates...\nothpatch 0.2.0 is up to date\n");
    M_CheckOthpatch(&test, "");
    M_Close(&test);
}

TEST(othpatch_digest_mismatch_fails_the_update)
{
    M_CASE test = M_PrepareBeside("self_digest", nullptr);
    M_AddNetwork(&test);
    M_WriteOthpatchRelease(
        &test, "0.2.0", "othpatch 0.2.0",
        "0000000000000000000000000000000000000000000000000000000000000000");
    CHECK_EQ_INT(M_Run(&test, 4, "--update", "--game", "th06", "en"), 1);
    M_CheckOutput(&test, "othpatch: cannot install 0.2.0: othpatch-0.2.0-");
    M_CheckOutput(&test, " instead of 0000");
    M_CheckOthpatch(&test, "");
    M_Close(&test);
}

TEST(othpatch_of_a_game_package_counts_for_its_own_release)
{
    M_CASE test = M_PrepareUpdate("self_after_game", "0.1.0", "0.2.0");
    M_WriteOthpatchRelease(&test, "0.3.0", "othpatch 0.3.0", nullptr);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        "othpatch 0.5.0 is installed too; the next run uses it\n"
        "othpatch 0.5.0 is up to date\n");
    char *const othpatch = M_Othpatch("0.5.0");
    M_CheckOthpatch(&test, othpatch);
    Mem_Free(othpatch);
    M_Close(&test);
}

TEST(failed_othpatch_check_does_not_fail_the_patches)
{
    M_CASE test = M_PrepareBeside("self_offline", nullptr);
    M_AddNetwork(&test);
    Mem_Free(test.othpatch_releases);
    test.othpatch_releases = Str_Format("file://%s/missing.json", test.server);
    CHECK_EQ_INT(M_Run(&test, 4, "--update", "--game", "th06", "en"), 0);
    M_CheckOutput(&test, "Done: 2 patches");
    M_CheckOutput(&test, "Cannot check for updates of othpatch: ");
    M_Close(&test);
}

TEST(one_release_list_serves_the_games_and_othpatch)
{
    M_CASE test = M_PrepareUpdate("shared_list", "0.1.0", "0.2.0");
    M_WriteOthpatchRelease(&test, "0.3.0", "othpatch 0.3.0", nullptr);
    char *const text = Server_Read(test.server, "releases.json");
    char *const othpatch = Server_Read(test.server, "othpatch_releases.json");
    text[strlen(text) - 1] = ',';
    // Keeps only the release othpatch-v0.3.0 of the othpatch list.
    const char *const own =
        strstr(othpatch, "{\"tag_name\": \"othpatch-v0.3.0\"");
    char *const both = Str_Format("%s%s", text, own);
    PatchBuilder_Write(test.server, "releases.json", both);
    Mem_Free(test.othpatch_releases);
    test.othpatch_releases = Str_Dup(test.releases);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test,
        "oth06 0.2.0 is available (you have 0.1.0); run othpatch --update "
        "to install it\n"
        "othpatch 0.3.0 is available (you have 0.1.0); run othpatch --update "
        "to install it\n");
    Mem_Free(both);
    SDL_free(othpatch);
    SDL_free(text);
    M_Close(&test);
}

TEST(failed_shared_release_list_is_one_message)
{
    M_CASE test = M_PrepareBeside("shared_offline", nullptr);
    M_AddNetwork(&test);
    M_WriteGame(&test, "0.1.0");
    Mem_Free(test.othpatch_releases);
    test.othpatch_releases = Str_Dup(test.releases);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(&test, "Checking for updates...\nCannot check for updates: ");
    CHECK(strstr(test.output, "of othpatch") == nullptr);
    M_Close(&test);
}
