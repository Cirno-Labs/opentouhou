#include "othpatch/main.h"

#include "othpatch/fetch.h"
#include "othpatch/install.h"
#include "othpatch/context.h"

#include "shell/folders.h"
#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"

#include "version.h"

#include <string.h>

#define M_DEFAULT_REPO "https://srv.thpatch.net/repo.js"

// Sets the size of the arena of one run, which holds its strings and the
// release index.
#define M_ARENA_SIZE ((size_t)64 * 1024 * 1024)

const char *const g_Games[APP_GAME_COUNT] = { "th06" };

static const char m_Usage[] =
    "Usage: othpatch [--update] [--verbose]\n"
    "                [<language> | <repo>/<patch> | <patch>]\n"
    "       othpatch --remove <patch>\n"
    "       othpatch --list | --help | --version\n"
    "\n"
    "Downloads thcrap patches for the opentouhou games beside othpatch,\n"
    "such as oth06 or oth06.exe, and updates the patches that you have.\n"
    "Then it checks for a newer release of othpatch and of each game, and\n"
    "asks before it installs one.\n"
    "\n"
    "Without an argument, othpatch updates the language and every patch\n"
    "that you have. When no language is set yet, it sets the language of\n"
    "the system.\n"
    "\n"
    "  <language>        a language code, such as de or pt-br, or ja for\n"
    "                    the original Japanese\n"
    "  <repo>/<patch>    a patch of one repository, such as nmlgc/aero\n"
    "  <patch>           the patch of the one repository that has it\n"
    "  --remove <patch>  remove a patch and the patches that only it needs\n"
    "  --list            list the languages and the installed mods\n"
    "  --update          install new releases without asking\n"
    "  --verbose         also print the folders and each patch\n"
    "  -h, --help        print this help\n"
    "  -v, --version     print the version\n"
    "\n"
    "Developer options:\n"
    "  --dir <folder>    the patches folder\n"
    "  --repo <url>      the first repository, " M_DEFAULT_REPO
    "\n"
    "                    by default\n"
    "  --releases <url>  the release index of the games and othpatch,\n"
    "                    " OTHPATCH_RELEASES_URL " by default\n";

static RESULT M_ParseArgs(
    const int argc, char **const argv, APP_ARGS *const args)
{
    *args = (APP_ARGS) {
        .repo = M_DEFAULT_REPO,
        .releases = OTHPATCH_RELEASES_URL,
    };
    for (int i = 1; i < argc; i++) {
        const char *const arg = argv[i];
        const bool valued = strcmp(arg, "--dir") == 0
            || strcmp(arg, "--repo") == 0 || strcmp(arg, "--releases") == 0
            || strcmp(arg, "--remove") == 0;
        if (valued && i + 1 >= argc) {
            return FAIL("%s needs a value", arg);
        }
        if (strcmp(arg, "--dir") == 0) {
            args->dir = argv[++i];
        } else if (strcmp(arg, "--repo") == 0) {
            args->repo = argv[++i];
        } else if (strcmp(arg, "--releases") == 0) {
            args->releases = argv[++i];
        } else if (strcmp(arg, "--remove") == 0) {
            args->remove = argv[++i];
        } else if (strcmp(arg, "--update") == 0) {
            args->update = true;
        } else if (strcmp(arg, "--verbose") == 0) {
            args->verbose = true;
        } else if (strcmp(arg, "--list") == 0) {
            args->list = true;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            args->help = true;
        } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
            args->version = true;
        } else if (arg[0] == '-') {
            return FAIL("unknown option %s; --help lists the options", arg);
        } else if (args->name != nullptr) {
            return FAIL("name one patch only, not %s and %s", args->name, arg);
        } else {
            args->name = arg;
        }
    }
    if (args->list && args->remove != nullptr) {
        return FAIL("use --list or --remove, not both");
    }
    if (args->name != nullptr && (args->remove != nullptr || args->list)) {
        return FAIL(
            "%s takes no patch to install",
            args->remove != nullptr ? "--remove" : "--list");
    }
    return OK;
}

// Returns the patches folder: --dir, or else the patches folder of the
// games beside othpatch. Mem_Free releases *out.
static RESULT M_FindDir(
    const APP_ARGS *const args, const char *const exe_dir, char **const out)
{
    *out = nullptr;
    if (args->dir != nullptr) {
        MUST(File_MakeDir(args->dir));
        *out = Str_Dup(args->dir);
        return OK;
    }
    const FOLDERS_INPUT input = { .exe_dir = exe_dir };
    return Folders_FindPatchesDir(&input, out);
}

static int M_RunAll(APP_CONTEXT *const run, const char *const version)
{
    FILE *const out = run->out;
    bool first_run;
    bool ok = StepPatches_Run(run, &first_run);
    ok = StepUpgrade_Run(run, version) && ok;
    fputs(
        ok ? "\nAll done. Go yukkuri!\n" : "\nSomething failed, see above.\n",
        out);
    if (first_run) {
        fputs(
            "\n"
            "To use another language, run othpatch with its code, such as "
            "othpatch de.\n"
            "othpatch ja switches back to the original Japanese.\n"
            "othpatch --list shows every language.\n",
            out);
    }
    return ok ? 0 : 1;
}

int Main_Run(
    const int argc, char **const argv, const char *const version,
    const char *const exe_dir, FILE *const in, FILE *const out,
    const MAIN_OUTPUT output)
{
    APP_ARGS args;
    const RESULT parsed = M_ParseArgs(argc, argv, &args);
    if (!IS_OK(parsed)) {
        Console_Fail(out, parsed, "othpatch: %s", parsed.message);
        return 1;
    }
    if (args.help) {
        fputs(m_Usage, out);
        return 0;
    }
    if (args.version) {
        fprintf(out, "othpatch %s\n", version);
        return 0;
    }
    ARENA arena;
    Arena_Init(&arena, "othpatch", M_ARENA_SIZE);
    if (exe_dir != nullptr && !args.list) {
        Install_RemoveLeftovers(
            Install_GetPlatform(), exe_dir, g_Games, APP_GAME_COUNT, &arena);
    }
    char *dir = nullptr;
    const RESULT found = M_FindDir(&args, exe_dir, &dir);
    if (!IS_OK(found)) {
        Arena_Free(&arena);
        Console_Fail(
            out, found, "othpatch: cannot find the patches folder: %s",
            found.message);
        return 1;
    }
    APP_CONTEXT run = {
        .args = &args,
        .exe_dir = exe_dir,
        .dir = dir,
        .arena = &arena,
        .in = in,
        .out = out,
        .terminal = output != MAIN_OUTPUT_FILE,
        .ansi = output == MAIN_OUTPUT_ANSI,
    };
    int code = 1;
    if (args.remove != nullptr) {
        code = Installed_Remove(&run);
    } else {
        const RESULT started = Fetch_Init();
        if (!IS_OK(started)) {
            Console_Fail(
                out, started, "othpatch: cannot start: %s", started.message);
        } else if (args.list) {
            code = Installed_List(&run);
        } else {
            code = M_RunAll(&run, version);
        }
        Fetch_Shutdown();
    }
    PatchManager_Close(run.manager);
    Arena_Free(&arena);
    Mem_Free(dir);
    fflush(out);
    return code;
}
