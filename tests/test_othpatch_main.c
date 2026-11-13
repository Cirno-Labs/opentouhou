#include "othpatch/install.h"
#include "othpatch/main.h"

#include "core/hash/sha256.h"

#include "fake_program.h"
#include "patch_server.h"
#include "test.h"
#include "test_dir.h"
#include "zip_builder.h"

#ifdef _WIN32
    #define M_SEPARATOR "\\"
#else
    #define M_SEPARATOR "/"
#endif

typedef struct {
    char *dir;
    char *server;
    // Holds the file:// URL of server.
    char *server_url;
    char *patches;
    char *program;
    char *repo;
    char *releases;
    // Hold the releases of oth06 and of othpatch in the index, as the JSON
    // text of the members of each array.
    char *game_entries;
    char *othpatch_entries;
    // Holds the version of the running othpatch.
    const char *version;
    // Holds the answer to the question of othpatch, or nullptr for a run
    // without a terminal.
    const char *answer;
    // Tells othpatch that its output is a terminal.
    bool terminal;
    // Holds the size of the game package in MB, as othpatch prints it.
    char package_size[16];
    char *output;
} M_CASE;

// Writes the release index of the test to the server.
static void M_WriteIndex(const M_CASE *const test)
{
    char *const text = Str_Format(
        "{\"format\": 1, \"programs\": {\"oth06\": [%s], \"othpatch\": "
        "[%s]}}",
        test->game_entries, test->othpatch_entries);
    PatchBuilder_Write(test->server, "releases.json", text);
    Mem_Free(text);
}

// Prepares a test whose othpatch is in a folder of its own, with the file
// beside_path beside it, or with nothing beside it when beside_path is
// nullptr.
static M_CASE M_PrepareBeside(
    const char *const name, const char *const beside_path)
{
    M_CASE test = { .dir = TestDir_Prepare(name) };
    test.server = TestDir_Join(test.dir, "server");
    test.server_url = TestDir_FileUrl(test.server);
    test.patches = TestDir_Join(test.dir, "patches");
    test.program = TestDir_Join(test.dir, "program");
    test.repo = Str_Format("%s/thpatch/repo.js", test.server_url);
    test.releases = Str_Format("%s/releases.json", test.server_url);
    test.game_entries = Str_Dup("{\"version\": \"0.1.0\", \"files\": {}}");
    test.othpatch_entries = Str_Dup("{\"version\": \"0.1.0\", \"files\": {}}");
    test.version = "0.1.0";
    PatchBuilder_Write(test.program, Install_GetPlatform()->othpatch, "");
    M_WriteIndex(&test);
    if (beside_path != nullptr) {
        FakeProgram_Write(test.program, beside_path, "th06 0.1.0\n", 0);
    }
    return test;
}

static M_CASE M_Prepare(const char *const name)
{
    return M_PrepareBeside(name, "oth06");
}

// Writes <root>/<repo>/<patch>/patch.js with a title and the given
// members after it, such as "\"dependencies\": [\"base\"]", or none.
static void M_AddTitledPatch(
    const char *const root, const char *const repo, const char *const patch,
    const char *const title, const char *const members)
{
    char *const text = Str_Format(
        "{\"id\": \"%s\", \"title\": \"%s\"%s%s}", patch, title,
        members[0] != '\0' ? ", " : "", members);
    char *const path = Str_Format("%s/%s/patch.js", repo, patch);
    PatchBuilder_Write(root, path, text);
    Mem_Free(path);
    Mem_Free(text);
}

// Writes the files.js of a patch with one file per entry of files, each
// "<path>" with the path as its contents, and writes the files.
static void M_AddFiles(
    const char *const root, const char *const repo, const char *const patch,
    const char *const *const files, const int count)
{
    char *list = Str_Dup("");
    for (int i = 0; i < count; i++) {
        char *const entry = Server_Entry(files[i], files[i]);
        char *const longer =
            Str_Format("%s%s%s", list, i > 0 ? ", " : "", entry);
        Mem_Free(entry);
        Mem_Free(list);
        list = longer;
        Server_AddFile(root, repo, patch, files[i], files[i]);
    }
    char *const text = Str_Format("{%s}", list);
    Server_AddFile(root, repo, patch, "files.js", text);
    Mem_Free(text);
    Mem_Free(list);
}

// Adds the network of a thpatch with two language packs, of one file each,
// that depend on a base patch of two files in another repository.
static void M_AddNetwork(const M_CASE *const test)
{
    const char *const neighbors[] = { "nmlgc" };
    Server_AddRepo(
        test->server, "thpatch", neighbors, 1,
        "\"lang_en\": \"English\", \"lang_de\": \"German\", \"tool\": \"\"");
    Server_AddRepo(test->server, "nmlgc", nullptr, 0, "\"base\": \"\"");
    const char *const packs[] = { "lang_en", "lang_de" };
    const char *const titles[] = { "English", "German" };
    for (int i = 0; i < 2; i++) {
        M_AddTitledPatch(
            test->server, "thpatch", packs[i], titles[i],
            "\"dependencies\": [\"nmlgc/base\"]");
        char *const file = Str_Format("th06/%s.jdiff", packs[i]);
        const char *const files[] = { file };
        M_AddFiles(test->server, "thpatch", packs[i], files, 1);
        Mem_Free(file);
    }
    M_AddTitledPatch(test->server, "nmlgc", "base", "Base patch", "");
    const char *const files[] = { "base.js", "th06/base.js" };
    M_AddFiles(test->server, "nmlgc", "base", files, 2);
}

// Runs othpatch with --dir, --repo and --releases of the test before the
// arguments, and keeps what it writes in test->output.
static int M_Run(M_CASE *const test, const int count, ...)
{
    char *argv[16] = {
        "othpatch", "--dir",      test->patches,  "--repo",
        test->repo, "--releases", test->releases,
    };
    int argc = 7;
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
    const int code = Main_Run(
        argc, argv, test->version, test->program, in, out, test->terminal);
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

// Returns text with each "%1$s" replaced by the config.ini of the test,
// each "%2$s" by its patches folder and each "%3$s" by the size of its
// game package. Mem_Free releases the text.
static char *M_Expand(const M_CASE *const test, const char *const text)
{
    char *const config = Str_Format(
        "%s" M_SEPARATOR "userdata" M_SEPARATOR "config.ini", test->program);
    char *expanded = Str_Dup(text);
    const char *const keys[] = { "%1$s", "%2$s", "%3$s" };
    const char *const values[] = { config, test->patches, test->package_size };
    for (int k = 0; k < 3; k++) {
        const char *found;
        while ((found = strstr(expanded, keys[k])) != nullptr) {
            char *const longer = Str_Format(
                "%.*s%s%s", (int)(found - expanded), expanded, values[k],
                found + strlen(keys[k]));
            Mem_Free(expanded);
            expanded = longer;
        }
    }
    Mem_Free(config);
    return expanded;
}

// Checks that the output has text, expanded as M_Expand does.
static void M_CheckOutput(const M_CASE *const test, const char *const text)
{
    char *const expanded = M_Expand(test, text);
    if (strstr(test->output, expanded) == nullptr) {
        TEST_FAIL("output has no \"%s\":\n%s", expanded, test->output);
    }
    Mem_Free(expanded);
}

static void M_CheckNoOutput(const M_CASE *const test, const char *const text)
{
    if (strstr(test->output, text) != nullptr) {
        TEST_FAIL("output has \"%s\":\n%s", text, test->output);
    }
}

// Checks that the output is text, expanded as M_Expand does.
static void M_CheckAll(const M_CASE *const test, const char *const text)
{
    char *const expected = M_Expand(test, text);
    CHECK_EQ_STR(test->output, expected);
    Mem_Free(expected);
}

static void M_Close(M_CASE *const test)
{
    Mem_Free(test->output);
    Mem_Free(test->othpatch_entries);
    Mem_Free(test->game_entries);
    Mem_Free(test->releases);
    Mem_Free(test->repo);
    Mem_Free(test->program);
    Mem_Free(test->patches);
    Mem_Free(test->server_url);
    Mem_Free(test->server);
    Mem_Free(test->dir);
}

// Gives the test a config.ini of its own: without portable.txt, every test
// shares the one in XDG_DATA_HOME.
static void M_MakePortable(const M_CASE *const test)
{
    PatchBuilder_Write(test->program, "portable.txt", "");
}

static char *M_ReadConfig(const M_CASE *const test)
{
    return Server_Read(test->program, "userdata/config.ini");
}

static void M_CheckConfig(const M_CASE *const test, const char *const text)
{
    char *const config = M_ReadConfig(test);
    CHECK(config != nullptr);
    CHECK_EQ_STR(config, text);
    Mem_Free(config);
}

static M_CASE M_PreparePortable(const char *const name)
{
    M_CASE test = M_Prepare(name);
    M_MakePortable(&test);
    M_AddNetwork(&test);
    return test;
}

#define M_OTHPATCH_SECTION                                                     \
    "\n"                                                                       \
    "Checking for othpatch updates...\n"                                       \
    "You already have the newest othpatch (0.1.0).\n"

#define M_ALL_DONE "\nAll done. Go yukkuri!\n"

#define M_SOMETHING_FAILED "\nSomething failed, see above.\n"

TEST(language_code_sets_up_the_pack_and_its_dependencies)
{
    M_CASE test = M_PreparePortable("install");
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "Language: German\n"
        "German (thpatch/lang_de): 1 file\n"
        "Base patch (nmlgc/base): 2 files\n"
        "Saved language = de to %1$s\n" M_OTHPATCH_SECTION M_ALL_DONE);
    CHECK(Server_Exists(test.patches, "thpatch/lang_de/th06/lang_de.jdiff"));
    CHECK(Server_Exists(test.patches, "nmlgc/base/patch.js"));
    M_CheckConfig(&test, "[patches]\nlanguage = de\n");
    M_Close(&test);
}

TEST(later_run_with_nothing_new_is_up_to_date)
{
    M_CASE test = M_PreparePortable("later");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckAll(
        &test,
        "Updating the language...\n"
        "Language: English\n"
        "Everything was already up to date.\n" M_OTHPATCH_SECTION M_ALL_DONE);
    M_CheckConfig(&test, "[patches]\nlanguage = en\n");
    M_Close(&test);
}

TEST(later_run_downloads_what_changed)
{
    M_CASE test = M_PreparePortable("later_changed");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    const char *const files[] = { "base.js", "th06/base.js", "new.js" };
    M_AddFiles(test.server, "nmlgc", "base", files, 3);
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckAll(
        &test,
        "Updating the language...\n"
        "Language: English\n"
        "Base patch (nmlgc/base): 1 file\n" M_OTHPATCH_SECTION M_ALL_DONE);
    M_Close(&test);
}

TEST(terminal_redraws_the_bars_in_place)
{
    M_CASE test = M_PreparePortable("terminal");
    test.terminal = true;
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "Language: English\n"
        "\rEnglish (thpatch/lang_en) [....................] 0/1 file"
        "\rEnglish (thpatch/lang_en) [####################] 1/1 file\n"
        "\rBase patch (nmlgc/base)   [....................] 0/2 files"
        "\rBase patch (nmlgc/base)   [##########..........] 1/2 files"
        "\rBase patch (nmlgc/base)   [####################] 2/2 files\n"
        "Saved language = en to %1$s\n" M_OTHPATCH_SECTION M_ALL_DONE);
    M_Close(&test);
}

TEST(game_beside_othpatch_is_downloaded)
{
    M_CASE test = M_PrepareBeside("beside", "oth06.AppImage");
    M_AddNetwork(&test);
    const char *const files[] = { "th06/lang_en.jdiff", "th07/lang_en.jdiff" };
    M_AddFiles(test.server, "thpatch", "lang_en", files, 2);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(&test, "English (thpatch/lang_en): 1 file\n");
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/th06/lang_en.jdiff"));
    CHECK(!Server_Exists(test.patches, "thpatch/lang_en/th07"));
    M_Close(&test);
}

TEST(game_bundle_beside_othpatch_is_downloaded)
{
    M_CASE test =
        M_PrepareBeside("beside_bundle", "oth06.app/Contents/MacOS/oth06");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/th06/lang_en.jdiff"));
    M_Close(&test);
}

TEST(game_in_the_patches_folder_is_downloaded)
{
    M_CASE test = M_PrepareBeside("downloaded", nullptr);
    M_AddNetwork(&test);
    PatchBuilder_Write(test.patches, "mine/local/patch.js", "{}");
    PatchBuilder_Write(test.patches, "mine/local/th06/old.txt", "");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/th06/lang_en.jdiff"));
    M_Close(&test);
}

TEST(failure_without_a_game_fails_the_section)
{
    M_CASE test = M_PrepareBeside("no_game", "oth07");
    M_AddNetwork(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 1);
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "There is no opentouhou game beside othpatch.\n" M_OTHPATCH_SECTION
            M_SOMETHING_FAILED);
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

TEST(first_run_sets_the_language_of_the_system)
{
    M_CASE test = M_PreparePortable("first_run");
    M_SetLocale("en_US.UTF-8");
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "System language: English\n"
        "English (thpatch/lang_en): 1 file\n"
        "Base patch (nmlgc/base): 2 files\n"
        "Saved language = en to %1$s\n" M_OTHPATCH_SECTION M_ALL_DONE
        "\n"
        "To use another language, run othpatch with its code, such as "
        "othpatch de.\n"
        "othpatch ja switches back to the original Japanese.\n"
        "othpatch --list shows every language.\n");
    M_CheckConfig(&test, "[patches]\nlanguage = en\n");
    M_SetLocale("de_AT.UTF-8");
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckOutput(&test, "Updating the language...\nLanguage: English\n");
    M_Close(&test);
}

TEST(first_run_matches_the_primary_language_of_the_locale)
{
    M_CASE test = M_PreparePortable("locale");
    M_SetLocale("de_AT.UTF-8");
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckOutput(&test, "System language: German\n");
    M_CheckConfig(&test, "[patches]\nlanguage = de\n");
    M_Close(&test);
}

TEST(first_run_without_a_pack_for_the_locale_sets_english)
{
    M_CASE test = M_PreparePortable("locale_fallback");
    M_SetLocale("fr_FR.UTF-8");
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckOutput(&test, "Setting up the language...\nLanguage: English\n");
    M_CheckConfig(&test, "[patches]\nlanguage = en\n");
    M_CheckOutput(&test, "othpatch --list shows every language.\n");
    M_Close(&test);
}

TEST(first_run_on_a_japanese_system_sets_the_original)
{
    M_CASE test = M_PreparePortable("locale_original");
    M_SetLocale("ja_JP.UTF-8");
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckOutput(
        &test,
        "Setting up the language...\n"
        "System language: original Japanese\n");
    M_CheckConfig(&test, "[patches]\nlanguage = ja\n");
    CHECK(!Server_Exists(test.patches, "thpatch"));
    M_Close(&test);
}
#endif

TEST(language_switch_keeps_the_other_pack)
{
    M_CASE test = M_PreparePortable("switch");
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    CHECK(Server_Exists(test.patches, "thpatch/lang_de/patch.js"));
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/patch.js"));
    M_CheckConfig(&test, "[patches]\nlanguage = en\n");
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "Language: English\n"
        "English (thpatch/lang_en): 1 file\n"
        "Saved language = en to %1$s\n" M_OTHPATCH_SECTION M_ALL_DONE);
    M_Close(&test);
}

TEST(no_name_keeps_the_language_of_config_ini)
{
    M_CASE test = M_PreparePortable("keep_language");
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckOutput(&test, "Updating the language...\nLanguage: German\n");
    M_CheckConfig(&test, "[patches]\nlanguage = de\n");
    M_Close(&test);
}

TEST(language_change_keeps_a_pack_that_a_patch_needs)
{
    M_CASE test = M_PreparePortable("removal_bug");
    Server_AddPatch(
        test.server, "thpatch", "tool", "\"dependencies\": [\"lang_en\"]");
    Server_AddFile(test.server, "thpatch", "tool", "files.js", "{}");
    CHECK_EQ_INT(M_Run(&test, 1, "thpatch/tool"), 0);
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/patch.js"));
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/patch.js"));
    CHECK(Server_Exists(test.patches, "thpatch/tool/patch.js"));
    M_Close(&test);
}

TEST(language_edit_keeps_the_other_lines_of_config_ini)
{
    M_CASE test = M_PreparePortable("config_edit");
    PatchBuilder_Write(
        test.program, "userdata/config.ini",
        "; my settings\n"
        "[video]\n"
        "fullscreen = true # big\n"
        "\n"
        "[patches]\n"
        "disabled = a/b\n"
        "language = de\n"
        "; after\n"
        "[screenshot]\n"
        "format = jpg\n");
    CHECK_EQ_INT(M_Run(&test, 1, "EN"), 0);
    M_CheckConfig(
        &test,
        "; my settings\n"
        "[video]\n"
        "fullscreen = true # big\n"
        "\n"
        "[patches]\n"
        "disabled = a/b\n"
        "language = en\n"
        "; after\n"
        "[screenshot]\n"
        "format = jpg\n");
    M_Close(&test);
}

TEST(language_edit_adds_the_section_or_the_file)
{
    M_CASE test = M_PreparePortable("config_add");
    PatchBuilder_Write(
        test.program, "userdata/config.ini", "[video]\nfullscreen = true\n");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckConfig(
        &test, "[video]\nfullscreen = true\n\n[patches]\nlanguage = en\n");
    M_Close(&test);

    M_CASE fresh = M_PreparePortable("config_new");
    CHECK_EQ_INT(M_Run(&fresh, 1, "de"), 0);
    M_CheckConfig(&fresh, "[patches]\nlanguage = de\n");
    M_Close(&fresh);
}

TEST(original_language_downloads_nothing)
{
    M_CASE test = M_PreparePortable("original");
    CHECK_EQ_INT(M_Run(&test, 1, "ja"), 0);
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "Language: original Japanese\n"
        "Saved language = ja to %1$s\n" M_OTHPATCH_SECTION M_ALL_DONE);
    M_CheckConfig(&test, "[patches]\nlanguage = ja\n");
    CHECK(!Server_Exists(test.patches, "thpatch"));
    M_Close(&test);
}

TEST(original_language_keeps_the_packs)
{
    M_CASE test = M_PreparePortable("original_switch");
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    CHECK_EQ_INT(M_Run(&test, 1, "ja"), 0);
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "Language: original Japanese\n"
        "Saved language = ja to %1$s\n"
        "The other language packs stay installed, so switching back "
        "downloads nothing.\n" M_OTHPATCH_SECTION M_ALL_DONE);
    CHECK(Server_Exists(test.patches, "thpatch/lang_de/patch.js"));
    CHECK_EQ_INT(M_Run(&test, 0), 0);
    M_CheckAll(
        &test,
        "Updating the language...\n"
        "Language: original Japanese\n"
        "Everything was already up to date.\n" M_OTHPATCH_SECTION M_ALL_DONE);
    M_Close(&test);
}

TEST(unknown_language_lists_the_codes)
{
    M_CASE test = M_PreparePortable("unknown_language");
    CHECK_EQ_INT(M_Run(&test, 1, "baboon"), 1);
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "There is no language \"baboon\".\n"
        "Languages for oth06: de, en, ja (original)\n" M_OTHPATCH_SECTION
            M_SOMETHING_FAILED);
    CHECK(M_ReadConfig(&test) == nullptr);
    M_Close(&test);
}

TEST(unknown_language_of_config_ini_lists_the_codes)
{
    M_CASE test = M_PreparePortable("unknown_config");
    PatchBuilder_Write(
        test.program, "userdata/config.ini", "[patches]\nlanguage = xx\n");
    CHECK_EQ_INT(M_Run(&test, 0), 1);
    M_CheckOutput(
        &test,
        "Updating the language...\n"
        "There is no language \"xx\".\n"
        "Languages for oth06: de, en, ja (original)\n");
    M_Close(&test);
}

TEST(language_without_files_for_the_game_fails)
{
    M_CASE test = M_PreparePortable("no_game_files");
    const char *const files[] = { "th07/msg1.dat.jdiff" };
    M_AddFiles(test.server, "thpatch", "lang_de", files, 1);
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 1);
    M_CheckOutput(
        &test,
        "Setting up the language...\n"
        "Language: German\n"
        "German (thpatch/lang_de) has no files for oth06.\n");
    CHECK(M_ReadConfig(&test) == nullptr);
    CHECK(!Server_Exists(test.patches, "thpatch"));
    M_Close(&test);
}

TEST(bare_name_of_two_repositories_is_ambiguous)
{
    M_CASE test = M_Prepare("ambiguous");
    const char *const neighbors[] = { "nmlgc" };
    Server_AddRepo(
        test.server, "thpatch", neighbors, 1,
        "\"lang_en\": \"English\", \"tool\": \"\"");
    Server_AddRepo(test.server, "nmlgc", nullptr, 0, "\"tool\": \"\"");
    CHECK_EQ_INT(M_Run(&test, 1, "tool"), 1);
    M_CheckOutput(
        &test,
        "Installing tool...\n"
        "2 repositories have a patch tool: thpatch/tool, nmlgc/tool. Name "
        "one as <repo>/<patch>.\n");
    M_Close(&test);
}

TEST(bare_name_of_one_repository_installs_it)
{
    M_CASE test = M_PreparePortable("bare_name");
    CHECK_EQ_INT(M_Run(&test, 1, "base"), 0);
    M_CheckAll(
        &test,
        "Installing nmlgc/base...\n"
        "Base patch (nmlgc/base): 2 files\n"
        "Installed nmlgc/base.\n" M_OTHPATCH_SECTION M_ALL_DONE);
    CHECK(Server_Exists(test.patches, "nmlgc/base/patch.js"));
    M_Close(&test);
}

// Adds thpatch/tool, a mod that needs nmlgc/base and thpatch/own, and
// installs English and the mod.
static void M_InstallTool(M_CASE *const test)
{
    M_AddNetwork(test);
    const char *const neighbors[] = { "nmlgc" };
    Server_AddRepo(
        test->server, "thpatch", neighbors, 1,
        "\"lang_en\": \"English\", \"lang_de\": \"German\", \"tool\": "
        "\"\", \"own\": \"\"");
    M_AddTitledPatch(
        test->server, "thpatch", "tool", "Tool",
        "\"dependencies\": [\"nmlgc/base\", \"own\"]");
    const char *const files[] = { "tool.js" };
    M_AddFiles(test->server, "thpatch", "tool", files, 1);
    M_AddTitledPatch(test->server, "thpatch", "own", "Own", "");
    Server_AddFile(test->server, "thpatch", "own", "files.js", "{}");
    CHECK_EQ_INT(M_Run(test, 1, "en"), 0);
    CHECK_EQ_INT(M_Run(test, 1, "thpatch/tool"), 0);
}

TEST(mod_installs_with_what_it_needs)
{
    M_CASE test = M_Prepare("mod");
    M_MakePortable(&test);
    M_InstallTool(&test);
    M_CheckAll(
        &test,
        "Installing thpatch/tool...\n"
        "Tool (thpatch/tool): 1 file\n"
        "Installed thpatch/tool.\n" M_OTHPATCH_SECTION M_ALL_DONE);
    M_CheckConfig(&test, "[patches]\nlanguage = en\n");
    M_Close(&test);
}

TEST(remove_keeps_the_dependencies_that_others_need)
{
    M_CASE test = M_Prepare("remove");
    M_MakePortable(&test);
    M_InstallTool(&test);
    CHECK_EQ_INT(M_Run(&test, 2, "--remove", "tool"), 0);
    M_CheckAll(
        &test,
        "Removing thpatch/tool...\n"
        "Removed thpatch/tool and 1 patch that only it needed.\n");
    CHECK(!Server_Exists(test.patches, "thpatch/tool"));
    CHECK(!Server_Exists(test.patches, "thpatch/own"));
    CHECK(Server_Exists(test.patches, "nmlgc/base/patch.js"));
    CHECK(Server_Exists(test.patches, "thpatch/lang_en/patch.js"));
    M_Close(&test);
}

TEST(remove_of_a_patch_that_nothing_needs_names_it_alone)
{
    M_CASE test = M_PreparePortable("remove_alone");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    CHECK_EQ_INT(M_Run(&test, 3, "--verbose", "--remove", "lang_de"), 0);
    M_CheckAll(
        &test,
        "Removing thpatch/lang_de...\n"
        "Patches folder: %2$s\n"
        "thpatch/lang_de: removed\n"
        "Removed thpatch/lang_de.\n");
    M_Close(&test);
}

TEST(remove_fails_for_a_patch_that_another_needs)
{
    M_CASE test = M_Prepare("remove_needed");
    M_MakePortable(&test);
    M_InstallTool(&test);
    CHECK_EQ_INT(M_Run(&test, 2, "--remove", "nmlgc/base"), 1);
    M_CheckAll(
        &test,
        "Removing nmlgc/base...\n"
        "thpatch/lang_en needs nmlgc/base. Remove thpatch/lang_en first.\n");
    CHECK(Server_Exists(test.patches, "nmlgc/base/patch.js"));
    CHECK_EQ_INT(M_Run(&test, 2, "--remove", "nothing"), 1);
    M_CheckAll(
        &test,
        "Removing nothing...\n"
        "There is no installed patch \"nothing\".\n"
        "othpatch --list shows the installed mods.\n");
    M_Close(&test);
}

TEST(remove_of_a_bare_name_of_two_patches_is_ambiguous)
{
    M_CASE test = M_Prepare("remove_ambiguous");
    PatchBuilder_Write(test.patches, "a/tool/patch.js", "{}");
    PatchBuilder_Write(test.patches, "b/tool/patch.js", "{}");
    CHECK_EQ_INT(M_Run(&test, 2, "--remove", "tool"), 1);
    M_CheckAll(
        &test,
        "Removing tool...\n"
        "2 installed patches are tool: a/tool, b/tool. Name one as "
        "<repo>/<patch>.\n");
    M_Close(&test);
}

TEST(run_updates_installed_patches)
{
    M_CASE test = M_PreparePortable("installed");
    M_AddTitledPatch(test.server, "thpatch", "tool", "Tool", "");
    const char *const files[] = { "tool.js" };
    M_AddFiles(test.server, "thpatch", "tool", files, 1);
    CHECK_EQ_INT(M_Run(&test, 1, "tool"), 0);
    M_CheckOutput(&test, "Installing thpatch/tool...\n");
    Server_AddFile(
        test.server, "thpatch", "tool", "files.js", "{\"tool.js\": null}");
    CHECK_EQ_INT(M_Run(&test, 2, "--verbose", "en"), 0);
    M_CheckAll(
        &test,
        "Setting up the language...\n"
        "Patches folder: %2$s\n"
        "config.ini of oth06: %1$s\n"
        "Language: English\n"
        "English (thpatch/lang_en): 1 file\n"
        "thpatch/lang_en: 1 downloaded, 0 up to date, 0 removed\n"
        "Base patch (nmlgc/base): 2 files\n"
        "nmlgc/base: 2 downloaded, 0 up to date, 0 removed\n"
        "thpatch/tool: 0 downloaded, 0 up to date, 1 removed\n"
        "Saved language = en to %1$s\n" M_OTHPATCH_SECTION M_ALL_DONE);
    PatchBuilder_Write(
        test.patches, "thpatch/tool/patch.js", "{\"update\": false}");
    PatchBuilder_Write(test.patches, "thpatch/tool/tool.js", "changed");
    CHECK_EQ_INT(M_Run(&test, 1, "--verbose"), 0);
    M_CheckOutput(
        &test,
        "Language: English\n"
        "thpatch/lang_en: up to date\n"
        "nmlgc/base: up to date\n"
        "Everything was already up to date.\n");
    M_CheckNoOutput(&test, "thpatch/tool");
    char *const kept = Server_Read(test.patches, "thpatch/tool/tool.js");
    CHECK_EQ_STR(kept, "changed");
    Mem_Free(kept);
    M_Close(&test);
}

TEST(run_keeps_installed_patches_that_the_network_lacks)
{
    M_CASE test = M_PreparePortable("unknown_installed");
    PatchBuilder_Write(test.patches, "mine/local/patch.js", "{}");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckNoOutput(&test, "mine/local");
    CHECK_EQ_INT(M_Run(&test, 2, "--verbose", "en"), 0);
    M_CheckOutput(&test, "mine/local: left as it is, cannot update it: ");
    CHECK(Server_Exists(test.patches, "mine/local/patch.js"));
    M_Close(&test);
}

TEST(list_prints_the_languages_and_the_mods)
{
    M_CASE test = M_PreparePortable("list");
    CHECK_EQ_INT(M_Run(&test, 1, "--list"), 0);
    CHECK_EQ_STR(
        test.output,
        "Languages for oth06:\n"
        "  de       German\n"
        "  en       English\n"
        "  ja       original Japanese\n"
        "Installed mods: none\n");
    M_InstallTool(&test);
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 0);
    CHECK_EQ_INT(M_Run(&test, 1, "--list"), 0);
    CHECK_EQ_STR(
        test.output,
        "Languages for oth06:\n"
        "  de       German (in use)\n"
        "  en       English (installed)\n"
        "  ja       original Japanese\n"
        "Installed mods:\n"
        "  Tool (thpatch/tool)\n");
    CHECK_EQ_INT(M_Run(&test, 1, "ja"), 0);
    CHECK_EQ_INT(M_Run(&test, 1, "--list"), 0);
    M_CheckOutput(&test, "  ja       original Japanese (in use)\n");
    M_Close(&test);
}

TEST(failure_of_the_first_repository_fails_the_section)
{
    M_CASE test = M_Prepare("no_repo");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 1);
    M_CheckOutput(
        &test,
        "Setting up the language...\n"
        "Could not download the patch list: ");
    M_CheckOutput(
        &test,
        ".\nYour files are unchanged. Run othpatch again "
        "later.\n" M_OTHPATCH_SECTION M_SOMETHING_FAILED);
    M_Close(&test);
}

TEST(failure_of_a_missing_patch_names_it)
{
    M_CASE test = M_PreparePortable("missing_patch");
    CHECK_EQ_INT(M_Run(&test, 1, "thpatch/gone"), 1);
    M_CheckAll(
        &test,
        "Installing thpatch/gone...\n"
        "Could not install thpatch/gone: repository thpatch has no patch "
        "gone.\n" M_OTHPATCH_SECTION M_SOMETHING_FAILED);
    M_Close(&test);
}

TEST(failure_of_a_missing_dependency_names_it)
{
    M_CASE test = M_PreparePortable("missing_dependency");
    M_AddTitledPatch(
        test.server, "thpatch", "lang_en", "English",
        "\"dependencies\": [\"nmlgc/gone\"]");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 1);
    M_CheckOutput(
        &test,
        "Setting up the language...\n"
        "Language: English\n"
        "Could not install thpatch/lang_en: dependency nmlgc/gone of "
        "thpatch/lang_en: repository nmlgc has no patch gone.\n");
    M_Close(&test);
}

TEST(failure_of_a_download_keeps_the_files)
{
    M_CASE test = M_PreparePortable("download_failure");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    Server_AddFile(
        test.server, "thpatch", "lang_de", "th06/lang_de.jdiff", "x");
    CHECK_EQ_INT(M_Run(&test, 1, "de"), 1);
    M_CheckOutput(
        &test,
        "Setting up the language...\n"
        "Language: German\n"
        "German (thpatch/lang_de): 0/1 file\n"
        "Could not download thpatch/lang_de: 1 files did not arrive: ");
    M_CheckOutput(
        &test,
        ".\nYour files are unchanged. Run othpatch again "
        "later.\n" M_OTHPATCH_SECTION M_SOMETHING_FAILED);
    M_CheckNoOutput(&test, "Saved language");
    M_CheckConfig(&test, "[patches]\nlanguage = en\n");
    CHECK(!Server_Exists(test.patches, "thpatch/lang_de/patch.js"));
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
    CHECK_EQ_INT(M_Run(&test, 2, "--game", "th06"), 1);
    CHECK_EQ_STR(
        test.output,
        "othpatch: unknown option --game; --help lists the options\n");
    CHECK_EQ_INT(M_Run(&test, 1, "--installed"), 1);
    CHECK_EQ_INT(M_Run(&test, 2, "--list", "en"), 1);
    CHECK_EQ_STR(test.output, "othpatch: --list takes no patch to install\n");
    char *argv[] = { "othpatch", "--dir" };
    FILE *const out = tmpfile();
    CHECK_EQ_INT(
        Main_Run(2, argv, test.version, test.program, nullptr, out, false), 1);
    fclose(out);
    M_Close(&test);
}

TEST(help_and_version_print_and_succeed)
{
    M_CASE test = M_Prepare("help");
    CHECK_EQ_INT(M_Run(&test, 1, "--help"), 0);
    M_CheckOutput(&test, "Usage: othpatch");
    M_CheckOutput(&test, "  --verbose ");
    M_CheckOutput(&test, "Developer options:\n  --dir <folder>");
    CHECK_EQ_INT(M_Run(&test, 1, "--version"), 0);
    CHECK_EQ_STR(test.output, "othpatch 0.1.0\n");
    CHECK_EQ_INT(M_Run(&test, 1, "-v"), 0);
    CHECK_EQ_STR(test.output, "othpatch 0.1.0\n");
    CHECK_EQ_INT(M_Run(&test, 1, "-h"), 0);
    M_CheckOutput(&test, "Usage: othpatch");
    M_Close(&test);
}

// Returns the path of the game beside othpatch, relative to the program
// folder. Mem_Free releases the path.
static char *M_GamePath(const M_CASE *const test)
{
    ARENA arena;
    Arena_Init(&arena, "main test", 1 << 16);
    const char *const path = Install_GetGamePath(
        Install_GetPlatform(), test->program, "th06", &arena);
    char *const relative = Str_Dup(path + strlen(test->program) + 1);
    Arena_Free(&arena);
    return relative;
}

// Writes a game beside othpatch, in the form of the platform's packages,
// that prints output and exits with status.
static void M_WriteGameProgram(
    const M_CASE *const test, const char *const output, const int status)
{
    char *const path = M_GamePath(test);
    FakeProgram_Write(test->program, path, output, status);
    Mem_Free(path);
}

// Writes a game beside othpatch that prints version.
static void M_WriteGame(const M_CASE *const test, const char *const version)
{
    char *const output = Str_Format("th06 %s\n", version);
    M_WriteGameProgram(test, output, 0);
    Mem_Free(output);
}

// Returns an othpatch that prints version.
static FAKE_PROGRAM M_Othpatch(const char *const version)
{
    char *const output = Str_Format("othpatch %s\n", version);
    const FAKE_PROGRAM program = FakeProgram_Build(output, 0);
    Mem_Free(output);
    return program;
}

static void M_CheckOthpatch(const M_CASE *const test, const char *const text)
{
    char *const othpatch =
        Server_Read(test->program, Install_GetPlatform()->othpatch);
    CHECK_EQ_STR(othpatch, text);
    Mem_Free(othpatch);
}

// Checks that the othpatch beside the game is the one that prints
// version.
static void M_CheckOthpatchVersion(
    const M_CASE *const test, const char *const version)
{
    char *const output = Str_Format("othpatch %s\n", version);
    CHECK(FakeProgram_Equals(
        test->program, Install_GetPlatform()->othpatch, output, 0));
    Mem_Free(output);
}

// Writes the package of release version to the server, with the game
// that prints version and an othpatch that prints othpatch_version, and
// returns its digest. Sets the package size of the test.
static void M_WritePackage(
    M_CASE *const test, const char *const version,
    const char *const othpatch_version, char digest[SHA256_HEX_SIZE])
{
    const INSTALL_PLATFORM *const platform = Install_GetPlatform();
    char *const output = Str_Format("th06 %s\n", version);
    const FAKE_PROGRAM script = FakeProgram_Build(output, 0);
    Mem_Free(output);
    const FAKE_PROGRAM othpatch = M_Othpatch(othpatch_version);
    char *const game = platform->bundle
        ? Str_Format("oth06%s/Contents/MacOS/oth06", platform->game_suffix)
        : Str_Format("oth06%s", platform->game_suffix);
    const ZIP_BUILDER_ENTRY entries[] = {
        { game, script.data, script.size, 0100755, true },
        { platform->othpatch, othpatch.data, othpatch.size, 0100755, true },
        { "README.txt", "readme", 6, 0100644, true },
    };
    const ZIP_BUILDER_ARCHIVE archive = ZipBuilder_Build(entries, 3);
    char *const path =
        Str_Format("%s/oth06-%s-%s.zip", test->server, version, platform->name);
    CHECK(SDL_SaveFile(path, archive.data, archive.size));
    Sha256_ComputeHex(archive.data, archive.size, digest);
    snprintf(
        test->package_size, sizeof(test->package_size), "%.1f",
        (double)archive.size / (1024.0 * 1024.0));
    Mem_Free(path);
    Mem_Free(archive.data);
    Mem_Free(game);
    Mem_Free(othpatch.data);
    Mem_Free(script.data);
}

// Writes the index with one release of oth06, whose package has digest,
// or no digest when digest is nullptr. A release without files has no
// package. A development version beside it does not count.
static void M_WriteReleases(
    M_CASE *const test, const char *const version, const char *const digest,
    const bool files)
{
    const char *const platform = Install_GetPlatform()->name;
    char *const digest_member = digest != nullptr
        ? Str_Format(", \"sha256\": \"%s\"", digest)
        : Str_Dup("");
    char *const file = files
        ? Str_Format(
              "\"%s\": {\"name\": \"oth06-%s-%s.zip\", \"url\": "
              "\"%s/oth06-%s-%s.zip\"%s}",
              platform, version, platform, test->server_url, version, platform,
              digest_member)
        : Str_Dup("");
    Mem_Free(test->game_entries);
    test->game_entries = Str_Format(
        "{\"version\": \"9.9.9-dev\", \"files\": {}}, "
        "{\"version\": \"%s\", \"othpatch\": \"0.5.0\", \"files\": {%s}}",
        version, file);
    M_WriteIndex(test);
    Mem_Free(file);
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
    char *const path = M_GamePath(test);
    char *const output = Str_Format("th06 %s\n", version);
    CHECK(FakeProgram_Equals(test->program, path, output, 0));
    Mem_Free(output);
    Mem_Free(path);
}

#define M_GAME_HEADING "\nChecking for oth06 updates...\n"

TEST(newer_release_without_a_terminal_is_only_announced)
{
    M_CASE test = M_PrepareUpdate("update_notice", "0.1.0", "0.2.0");
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test,
        M_OTHPATCH_SECTION M_GAME_HEADING
        "oth06 0.2.0 is out (you have 0.1.0).\n"
        "Kept oth06 0.1.0. Run othpatch --update to install it.\n" M_ALL_DONE);
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(update_option_installs_the_game_and_othpatch)
{
    M_CASE test = M_PrepareUpdate("update_option", "0.1.0", "0.10.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        M_OTHPATCH_SECTION M_GAME_HEADING
        "oth06 0.10.0 is out (you have 0.1.0).\n"
        "oth06 0.10.0: %3$s MB\n"
        "Updated oth06 from 0.1.0 to 0.10.0.\n"
        "Updated othpatch from 0.1.0 to 0.5.0. The next run uses the new "
        "one.\n" M_ALL_DONE);
    M_CheckGame(&test, "0.10.0");
    M_CheckOthpatchVersion(&test, "0.5.0");
    CHECK(!Server_Exists(test.program, "README.txt"));
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test, M_GAME_HEADING "You already have the newest oth06 (0.10.0).\n");
    M_Close(&test);
}

TEST(terminal_redraws_the_bar_of_a_release)
{
    M_CASE test = M_PrepareUpdate("update_terminal", "0.1.0", "0.2.0");
    test.terminal = true;
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        "oth06 0.2.0 is out (you have 0.1.0).\n"
        "\roth06 0.2.0 [####################] %3$s/%3$s MB\n"
        "Updated oth06 from 0.1.0 to 0.2.0.\n");
    M_Close(&test);
}

TEST(yes_at_the_question_installs_the_game)
{
    M_CASE test = M_PrepareUpdate("update_yes", "0.1.0", "0.2.0");
    const char *const answers[] = { " Yes\n", "\n" };
    for (size_t i = 0; i < ARRAY_SIZE(answers); i++) {
        M_WriteGame(&test, "0.1.0");
        test.answer = answers[i];
        CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
        M_CheckOutput(
            &test,
            M_GAME_HEADING
            "oth06 0.2.0 is out (you have 0.1.0).\n"
            "Install it now? [Y/n] oth06 0.2.0: %3$s MB\n"
            "Updated oth06 from 0.1.0 to 0.2.0.\n");
        M_CheckGame(&test, "0.2.0");
    }
    M_Close(&test);
}

TEST(other_answers_at_the_question_keep_the_game)
{
    M_CASE test = M_PrepareUpdate("update_no", "0.1.0", "0.2.0");
    const char *const answers[] = { "n\n", "No\n", "yess\n", "" };
    for (size_t i = 0; i < ARRAY_SIZE(answers); i++) {
        test.answer = answers[i];
        CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
        M_CheckOutput(
            &test,
            "oth06 0.2.0 is out (you have 0.1.0).\n"
            "Install it now? [Y/n] Kept oth06 0.1.0. Run othpatch again to "
            "update.\n" M_ALL_DONE);
        M_CheckGame(&test, "0.1.0");
    }
    M_Close(&test);
}

TEST(same_or_older_release_is_up_to_date)
{
    M_CASE test = M_PrepareUpdate("update_same", "0.2.0", "0.2.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        M_GAME_HEADING
        "You already have the newest oth06 (0.2.0).\n" M_ALL_DONE);
    M_Close(&test);
    test = M_PrepareUpdate("update_older", "0.3.0", "0.2.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(&test, "You already have the newest oth06 (0.3.0).\n");
    M_CheckGame(&test, "0.3.0");
    M_Close(&test);
}

TEST(development_build_is_never_updated)
{
    M_CASE test = M_PrepareUpdate("update_dev", "0.1.0-3-gabcdef0", "0.2.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        M_GAME_HEADING
        "oth06 0.1.0-3-gabcdef0 is a development build, so othpatch does not "
        "update it.\n" M_ALL_DONE);
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
    M_CheckOutput(
        &test,
        "oth06 0.2.0 is out (you have 0.1.0).\n"
        "oth06 0.2.0: %3$s MB\n"
        "Could not update oth06 to 0.2.0: oth06-0.2.0-");
    M_CheckOutput(&test, " instead of 0000");
    M_CheckOutput(&test, ".\n" M_SOMETHING_FAILED);
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(release_without_a_package_is_announced)
{
    M_CASE test = M_PrepareUpdate("update_no_asset", "0.1.0", "0.2.0");
    M_WriteReleases(&test, "0.2.0", nullptr, false);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    char *const expected = Str_Format(
        M_GAME_HEADING "oth06 0.2.0 is out, but not for %s.\n" M_ALL_DONE,
        Install_GetPlatform()->name);
    M_CheckOutput(&test, expected);
    Mem_Free(expected);
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(release_without_a_digest_fails_the_update)
{
    M_CASE test = M_PrepareUpdate("update_no_digest", "0.1.0", "0.2.0");
    M_WriteReleases(&test, "0.2.0", nullptr, true);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 1);
    M_CheckOutput(
        &test,
        "oth06 0.2.0 is out (you have 0.1.0).\n"
        "Could not update oth06 to 0.2.0: the index gives no SHA-256 digest "
        "of oth06-0.2.0-");
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(release_under_the_game_id_is_not_found)
{
    M_CASE test = M_PrepareUpdate("update_game_id", "0.1.0", "0.2.0");
    char *const text = Str_Format(
        "{\"format\": 1, \"programs\": {\"th06\": [%s], \"othpatch\": "
        "[%s]}}",
        test.game_entries, test.othpatch_entries);
    PatchBuilder_Write(test.server, "releases.json", text);
    Mem_Free(text);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 1);
    M_CheckOutput(
        &test,
        M_GAME_HEADING
        "Could not check for updates: the index has no release of "
        "oth06.\n" M_SOMETHING_FAILED);
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(failed_check_fails_each_section_but_not_the_patches)
{
    M_CASE test = M_PrepareBeside("update_offline", nullptr);
    M_AddNetwork(&test);
    Mem_Free(test.releases);
    test.releases = Str_Format("%s/missing.json", test.server_url);
    M_WriteGame(&test, "0.1.0");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 1);
    M_CheckOutput(&test, "Saved language = en to ");
    M_CheckOutput(
        &test,
        "\nChecking for othpatch updates...\nCould not check for updates: ");
    M_CheckOutput(&test, M_GAME_HEADING "Could not check for updates: ");
    M_CheckOutput(&test, ".\n" M_SOMETHING_FAILED);
    M_CheckGame(&test, "0.1.0");
    M_Close(&test);
}

TEST(game_that_gives_no_version_fails_its_section)
{
    M_CASE test = M_PrepareUpdate("update_broken", "0.1.0", "0.2.0");
    M_WriteGameProgram(&test, "", 1);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 1);
    M_CheckOutput(
        &test, M_GAME_HEADING "Could not read the version of oth06: ");
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
        &test,
        "\nChecking for othpatch updates...\n"
        "You already have the newest othpatch (0.6.0).\n" M_GAME_HEADING
        "oth06 0.2.0 is out (you have 0.1.0).\n"
        "oth06 0.2.0: %3$s MB\n"
        "Updated oth06 from 0.1.0 to 0.2.0.\n" M_ALL_DONE);
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
        "\nChecking for othpatch updates...\n"
        "othpatch 0.1.0-3-gabcdef0 is a development build, so othpatch does "
        "not update it.\n");
    M_CheckOutput(&test, "Updated oth06 from 0.1.0 to 0.2.0.\n" M_ALL_DONE);
    M_CheckGame(&test, "0.2.0");
    M_CheckOthpatch(&test, "");
    M_Close(&test);
}

// Writes the index with othpatch release <version>, whose
// executable for the platform holds contents and has digest, or the digest
// of contents when digest is nullptr.
static void M_WriteOthpatchRelease(
    M_CASE *const test, const char *const version, const char *const contents,
    const char *const digest)
{
    const INSTALL_PLATFORM *const platform = Install_GetPlatform();
    char *const name = Str_Format(
        "othpatch-%s-%s%s", version, platform->name,
        platform->othpatch + strlen("othpatch"));
    PatchBuilder_Write(test->server, name, contents);
    char computed[SHA256_HEX_SIZE];
    Sha256_ComputeHex(contents, strlen(contents), computed);
    Mem_Free(test->othpatch_entries);
    test->othpatch_entries = Str_Format(
        "{\"version\": \"9.9.9-dev\", \"files\": {}}, "
        "{\"version\": \"%s\", \"files\": {\"%s\": {\"name\": \"%s\", "
        "\"url\": \"%s/%s\", \"sha256\": \"%s\"}}}",
        version, platform->name, name, test->server_url, name,
        digest != nullptr ? digest : computed);
    M_WriteIndex(test);
    Mem_Free(name);
}

TEST(newer_othpatch_release_is_installed)
{
    M_CASE test = M_Prepare("self_update");
    M_AddNetwork(&test);
    M_WriteOthpatchRelease(&test, "0.2.0", "othpatch 0.2.0", nullptr);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test,
        "\nChecking for othpatch updates...\n"
        "othpatch 0.2.0 is out (you have 0.1.0).\n"
        "Kept othpatch 0.1.0. Run othpatch --update to install "
        "it.\n" M_ALL_DONE);
    M_CheckOthpatch(&test, "");
    test.answer = "y\n";
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test,
        "\nChecking for othpatch updates...\n"
        "othpatch 0.2.0 is out (you have 0.1.0).\n"
        "Install it now? [Y/n] othpatch 0.2.0: 0.0 MB\n"
        "Updated othpatch from 0.1.0 to 0.2.0. The next run uses the new "
        "one.\n" M_ALL_DONE);
    M_CheckOthpatch(&test, "othpatch 0.2.0");
    M_Close(&test);
}

TEST(declined_othpatch_release_is_kept)
{
    M_CASE test = M_Prepare("self_declined");
    M_AddNetwork(&test);
    M_WriteOthpatchRelease(&test, "0.2.0", "othpatch 0.2.0", nullptr);
    test.answer = "n\n";
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test,
        "othpatch 0.2.0 is out (you have 0.1.0).\n"
        "Install it now? [Y/n] Kept othpatch 0.1.0. Run othpatch again to "
        "update.\n" M_ALL_DONE);
    M_CheckOthpatch(&test, "");
    M_Close(&test);
}

TEST(same_othpatch_release_is_up_to_date)
{
    M_CASE test = M_Prepare("self_same");
    M_AddNetwork(&test);
    M_WriteOthpatchRelease(&test, "0.2.0", "othpatch 0.2.0", nullptr);
    test.version = "0.2.0";
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        "\nChecking for othpatch updates...\n"
        "You already have the newest othpatch (0.2.0).\n" M_ALL_DONE);
    M_CheckOthpatch(&test, "");
    M_Close(&test);
}

TEST(othpatch_digest_mismatch_fails_the_update)
{
    M_CASE test = M_Prepare("self_digest");
    M_AddNetwork(&test);
    M_WriteOthpatchRelease(
        &test, "0.2.0", "othpatch 0.2.0",
        "0000000000000000000000000000000000000000000000000000000000000000");
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 1);
    M_CheckOutput(&test, "Could not update othpatch to 0.2.0: othpatch-0.2.0-");
    M_CheckOutput(&test, " instead of 0000");
    M_CheckOutput(&test, M_SOMETHING_FAILED);
    M_CheckOthpatch(&test, "");
    M_Close(&test);
}

TEST(game_package_with_a_newer_othpatch_replaces_it_again)
{
    M_CASE test = M_PrepareUpdate("self_after_game", "0.1.0", "0.2.0");
    M_WriteOthpatchRelease(&test, "0.3.0", "othpatch 0.3.0", nullptr);
    CHECK_EQ_INT(M_Run(&test, 2, "--update", "en"), 0);
    M_CheckOutput(
        &test,
        "\nChecking for othpatch updates...\n"
        "othpatch 0.3.0 is out (you have 0.1.0).\n"
        "othpatch 0.3.0: 0.0 MB\n"
        "Updated othpatch from 0.1.0 to 0.3.0. The next run uses the new "
        "one.\n" M_GAME_HEADING
        "oth06 0.2.0 is out (you have 0.1.0).\n"
        "oth06 0.2.0: %3$s MB\n"
        "Updated oth06 from 0.1.0 to 0.2.0.\n"
        "Updated othpatch from 0.3.0 to 0.5.0. The next run uses the new "
        "one.\n" M_ALL_DONE);
    M_CheckOthpatchVersion(&test, "0.5.0");
    M_Close(&test);
}

TEST(one_index_serves_the_games_and_othpatch)
{
    M_CASE test = M_PrepareUpdate("shared_index", "0.1.0", "0.2.0");
    M_WriteOthpatchRelease(&test, "0.3.0", "othpatch 0.3.0", nullptr);
    CHECK_EQ_INT(M_Run(&test, 1, "en"), 0);
    M_CheckOutput(
        &test,
        "\nChecking for othpatch updates...\n"
        "othpatch 0.3.0 is out (you have 0.1.0).\n"
        "Kept othpatch 0.1.0. Run othpatch --update to install "
        "it.\n" M_GAME_HEADING
        "oth06 0.2.0 is out (you have 0.1.0).\n"
        "Kept oth06 0.1.0. Run othpatch --update to install it.\n" M_ALL_DONE);
    M_Close(&test);
}
