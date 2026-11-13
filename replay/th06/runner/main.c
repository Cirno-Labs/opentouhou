#include "run.h"

#include "main.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "th06/files.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *data;
    const char *replay;
    const char *replays;
    const char *cases;
    bool help;
    RUNNER_OPTIONS options;
} M_ARGS;

static void M_Usage(void)
{
    puts(
        "Usage: th06replay --data DIR (--replay FILE | --replays DIR) "
        "CASE_OR_DIR\n"
        "                  [--break-on-diff] [--break-at CALC]\n"
        "\n"
        "Pairs OTRC assertions with original .rpy input. Calc numbers start at "
        "0.\n"
        "Directory cases match .rpy files by basename. Original files are "
        "read-only.");
}

static RESULT M_ReadCalc(const char *const text, uint64_t *const out)
{
    FAIL_IF(text[0] == '\0', "calc number is empty");
    uint64_t value = 0;
    for (const char *p = text; *p != '\0'; p++) {
        FAIL_IF(*p < '0' || *p > '9', "invalid calc number %s", text);
        const uint64_t digit = (uint64_t)(*p - '0');
        FAIL_IF(value > (UINT64_MAX - digit) / 10, "calc number is too large");
        value = value * 10 + digit;
    }
    *out = value;
    return OK;
}

static RESULT M_Parse(const int argc, char **const argv, M_ARGS *const args)
{
    *args = (M_ARGS) { .options.output = stdout };
    for (int i = 1; i < argc; i++) {
        const char *const arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            args->help = true;
        } else if (strcmp(arg, "--break-on-diff") == 0) {
            args->options.break_on_diff = true;
        } else if (
            strcmp(arg, "--data") == 0 || strcmp(arg, "--replay") == 0
            || strcmp(arg, "--replays") == 0
            || strcmp(arg, "--break-at") == 0) {
            FAIL_IF(i + 1 >= argc, "%s requires a value", arg);
            const char *const value = argv[++i];
            if (strcmp(arg, "--data") == 0) {
                args->data = value;
            } else if (strcmp(arg, "--replay") == 0) {
                args->replay = value;
            } else if (strcmp(arg, "--replays") == 0) {
                args->replays = value;
            } else {
                MUST(M_ReadCalc(value, &args->options.break_at));
                args->options.has_break_at = true;
            }
        } else {
            FAIL_IF(arg[0] == '-', "unknown option %s", arg);
            FAIL_IF(args->cases != nullptr, "more than one case path");
            args->cases = arg;
        }
    }
    if (!args->help) {
        FAIL_IF(args->data == nullptr, "--data is required");
        FAIL_IF(args->cases == nullptr, "a case file or directory is required");
        FAIL_IF(
            (args->replay == nullptr) == (args->replays == nullptr),
            "choose --replay or --replays");
    }
    return OK;
}

static const char *M_Basename(const char *const path)
{
    const char *name = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            name = p + 1;
        }
    }
    return name;
}

static RESULT M_RunFile(const M_ARGS *const args, const char *const case_path)
{
    char *replay_path = nullptr;
    if (args->replay != nullptr) {
        replay_path = Str_Dup(args->replay);
    } else {
        const char *const name = M_Basename(case_path);
        const size_t length = strlen(name);
        FAIL_IF(
            length < 5 || !Str_EndsWith(name, ".otrc") || length > INT_MAX,
            "case name must end in .otrc");
        char *const replay_name =
            Str_Format("%.*s.rpy", (int)(length - 5), name);
        replay_path = Path_Join(&g_HeapAllocator, args->replays, replay_name);
        Mem_Free(replay_name);
    }
    printf("%s\n", case_path);
    RUNNER_REPORT report;
    const RESULT result =
        Runner_Run(case_path, replay_path, &args->options, &report);
    Mem_Free(replay_path);
    if (IS_OK(result)) {
        printf(
            "  matched %" PRIu64
            " calcs, %d START and %d calc columns, %d skipped; replay "
            "complete\n",
            report.rows, report.start_columns, report.calc_columns,
            report.skipped_columns);
    } else {
        printf(
            "  failed after %" PRIu64 " calcs (%d skipped columns)\n",
            report.rows, report.skipped_columns);
    }
    return result;
}

static RESULT M_RunCases(const M_ARGS *const args)
{
    FILE_LIST files;
    RESULT listed = File_ListDir(args->cases, &g_HeapAllocator, &files);
    if (!IS_OK(listed)) {
        Result_Ignore(listed);
        return M_RunFile(args, args->cases);
    }
    if (args->replay != nullptr) {
        File_FreeList(&files);
        return FAIL("a case directory requires --replays");
    }
    int32_t count = 0;
    int32_t failed = 0;
    for (int32_t i = 0; i < files.count; i++) {
        if (!Str_EndsWith(files.items[i], ".otrc")) {
            continue;
        }
        char *const path =
            Path_Join(&g_HeapAllocator, args->cases, files.items[i]);
        if (!Result_Log(M_RunFile(args, path), LOG_LEVEL_ERROR)) {
            failed++;
        }
        Mem_Free(path);
        count++;
    }
    File_FreeList(&files);
    FAIL_IF(count == 0, "directory has no .otrc cases");
    FAIL_IF(failed != 0, "%d of %d cases failed", failed, count);
    printf("%d cases matched\n", count);
    return OK;
}

int RunnerMain_Run(const int argc, char **const argv)
{
    M_ARGS args;
    if (!Result_Log(M_Parse(argc, argv, &args), LOG_LEVEL_ERROR)) {
        M_Usage();
        return 1;
    }
    if (args.help) {
        M_Usage();
        return 0;
    }
    if (!Result_Log(Files_Init(args.data), LOG_LEVEL_ERROR)) {
        return 1;
    }
    const bool passed = Result_Log(M_RunCases(&args), LOG_LEVEL_ERROR);
    Files_Shutdown();
    return passed ? 0 : 1;
}
