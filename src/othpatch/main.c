#include "othpatch/main.h"

#include "othpatch/context.h"
#include "othpatch/fetch.h"
#include "othpatch/game.h"
#include "othpatch/install.h"
#include "othpatch/step.h"
#include "othpatch/version.h"

#include "core/args.h"
#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "shell/folders.h"

#define M_DEFAULT_REPO "https://srv.thpatch.net/repo.js"

// Sets the size of the arena of one run, which holds its strings and the
// release index.
#define M_ARENA_SIZE ((size_t)64 * 1024 * 1024)

static const ARG_OPTION m_Options[] = {
    ARG_STRING_OF(
        APP_ARGS, "patch", name, "<patch>",
        "a language code, such as de or pt-br, or ja for the original "
        "Japanese; a patch of one repository, such as nmlgc/aero; or the "
        "patch of the one repository that has it"),
    ARG_STRING_OF(
        APP_ARGS, "--remove", remove, "<patch>",
        "remove a patch and the patches that only it needs"),
    ARG_FLAG_OF(
        APP_ARGS, "--list", list, "list the languages and the installed mods"),
    ARG_FLAG_OF(
        APP_ARGS, "--update", update, "install new releases without asking"),
    ARG_FLAG_OF(
        APP_ARGS, "--verbose", verbose,
        "also print the folders and each patch"),
    ARG_FLAG_OF(
        APP_ARGS, "--version", version, "print the version", ARG_SHORT("-v")),
    ARG_STRING_OF(
        APP_ARGS, "--dir", dir, "<folder>",
        "for developers: the patches folder"),
    ARG_STRING_OF(
        APP_ARGS, "--repo", repo, "<url>",
        "for developers: the first repository, " M_DEFAULT_REPO " by default",
        ARG_FALLBACK(M_DEFAULT_REPO)),
    ARG_STRING_OF(
        APP_ARGS, "--releases", releases, "<url>",
        "for developers: the release index of the games and "
        "othpatch, " OTHPATCH_RELEASES_URL " by default",
        ARG_FALLBACK(OTHPATCH_RELEASES_URL)),
};

static const ARG_RULE m_Rules[] = {
    { ARG_EXCLUSIVE, { "patch", "--remove", "--list" } },
};

static const ARG_SCHEMA m_Schema = ARG_SCHEMA_OF(
    "othpatch",
    "Downloads thcrap patches for the opentouhou games beside othpatch, "
    "such as oth06 or oth06.exe, and for the games of the patches that you "
    "have. Then it checks for a newer release of othpatch and of each "
    "game it finds beside it, and asks before it installs one. A renamed "
    "game is not found.\n"
    "\n"
    "Without an argument, othpatch updates the language and every patch "
    "that you have. When no language is set yet, it sets the language of "
    "the system.",
    m_Options, m_Rules);

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

// Holds a command: the steps that it runs in order, whether it needs the
// network, and whether it ends with a summary of the steps.
typedef struct {
    const APP_STEP *steps[2];
    int32_t step_count;
    bool network;
    bool summary;
} M_COMMAND;

static const M_COMMAND m_Run = {
    { &g_StepPatches, &g_StepUpgrade }, 2, true, true
};
static const M_COMMAND m_List = { { &g_StepList }, 1, true, false };
static const M_COMMAND m_Remove = { { &g_StepRemove }, 1, false, false };

// Runs each step of command, also after one fails, and returns the exit
// code.
static int M_RunCommand(APP_CONTEXT *const run, const M_COMMAND *const command)
{
    bool ok = true;
    for (int32_t i = 0; i < command->step_count; i++) {
        ok = command->steps[i]->run(run) && ok;
    }
    if (command->summary) {
        fputs(
            ok ? "\nAll done. Go yukkuri!\n"
               : "\nSomething failed, see above.\n",
            run->out);
    }
    if (command->summary && run->first_run) {
        fputs(
            "\n"
            "To use another language, run othpatch with its code, such as "
            "othpatch de.\n"
            "othpatch ja switches back to the original Japanese.\n"
            "othpatch --list shows every language.\n",
            run->out);
    }
    return ok ? 0 : 1;
}

int Main_Run(
    const int argc, char **const argv, const char *const version,
    const char *const exe_dir, FILE *const in, FILE *const out,
    const MAIN_OUTPUT output)
{
    APP_ARGS args;
    bool help;
    const RESULT parsed = Args_Parse(&m_Schema, argc, argv, &args, &help);
    if (!IS_OK(parsed)) {
        Console_Fail(out, parsed, "%s", parsed.message);
        return 1;
    }
    if (help) {
        Args_PrintHelp(&m_Schema, out);
        return 0;
    }
    if (args.version) {
        fprintf(out, "othpatch %s\n", version);
        return 0;
    }
    ARENA arena;
    Arena_Init(&arena, "othpatch", M_ARENA_SIZE);
    if (exe_dir != nullptr && !args.list) {
        GAME_LIST games = {};
        Game_FindBeside(&games, &arena, exe_dir, out);
        Install_RemoveLeftovers(
            Install_GetPlatform(), exe_dir, games.items, games.count, &arena);
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
        .version = version,
    };
    const M_COMMAND *const command = args.remove != nullptr ? &m_Remove
        : args.list                                         ? &m_List
                                                            : &m_Run;
    int code = 1;
    const RESULT started = command->network ? Fetch_Init() : OK;
    if (IS_OK(started)) {
        code = M_RunCommand(&run, command);
    } else {
        Console_Fail(
            out, started, "othpatch: cannot start: %s", started.message);
    }
    if (command->network) {
        Fetch_Shutdown();
    }
    PatchManager_Close(run.manager);
    Arena_Free(&arena);
    Mem_Free(dir);
    fflush(out);
    return code;
}
