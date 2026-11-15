#include "run.h"

#include "main.h"

#include "core/args.h"
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
    bool break_on_diff;
    const char *break_at;
} M_ARGS;

static const ARG_OPTION m_Options[] = {
    ARG_STRING_OF(
        M_ARGS, "--data", data, nullptr, "<dir>",
        "the folder of the original data", ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--replay", replay, nullptr, "<file>",
        "the .rpy file of one case"),
    ARG_STRING_OF(
        M_ARGS, "--replays", replays, nullptr, "<dir>",
        "the folder whose .rpy files match the cases by basename"),
    ARG_FLAG_OF(
        M_ARGS, "--break-on-diff", break_on_diff,
        "stop in the debugger at the first difference"),
    ARG_STRING_OF(
        M_ARGS, "--break-at", break_at, nullptr, "<calc>",
        "stop in the debugger at calc <calc>, counted from 0"),
    ARG_STRING_OF(
        M_ARGS, "cases", cases, nullptr, "<case-or-dir>",
        "an .otrc case, or a folder of them", ARG_REQUIRED),
};

static const ARG_RULE m_Rules[] = {
    { ARG_ONE_OF, { "--replay", "--replays" } },
};

static const ARG_SCHEMA m_Schema = ARG_SCHEMA_OF(
    "th06replay",
    "Pairs OTRC assertions with original .rpy input. Original files are "
    "read-only.",
    m_Options, m_Rules);

static RESULT M_ReadCalc(const char *const text, uint64_t *const out)
{
    if (text[0] == '\0') {
        return Args_Fail(&m_Schema, "--break-at needs a calc number");
    }
    uint64_t value = 0;
    for (const char *p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return Args_Fail(&m_Schema, "invalid calc number %s", text);
        }
        const uint64_t digit = (uint64_t)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return Args_Fail(&m_Schema, "calc number %s is too large", text);
        }
        value = value * 10 + digit;
    }
    *out = value;
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

static RESULT M_RunFile(
    const M_ARGS *const args, const RUNNER_OPTIONS *const options,
    const char *const case_path)
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
    const RESULT result = Runner_Run(case_path, replay_path, options, &report);
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

static RESULT M_RunCases(
    const M_ARGS *const args, const RUNNER_OPTIONS *const options)
{
    FILE_LIST files;
    RESULT listed = File_ListDir(args->cases, &g_HeapAllocator, &files);
    if (!IS_OK(listed)) {
        Result_Ignore(listed);
        return M_RunFile(args, options, args->cases);
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
        if (!Result_Log(M_RunFile(args, options, path), LOG_LEVEL_ERROR)) {
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
    bool help;
    RUNNER_OPTIONS options = {
        .output = stdout,
    };
    if (!Result_Log(
            Args_Parse(&m_Schema, argc, argv, &args, &help), LOG_LEVEL_ERROR)) {
        return 1;
    }
    if (help) {
        Args_PrintHelp(&m_Schema, stdout);
        return 0;
    }
    options.break_on_diff = args.break_on_diff;
    options.has_break_at = args.break_at != nullptr;
    if (options.has_break_at
        && !Result_Log(
            M_ReadCalc(args.break_at, &options.break_at), LOG_LEVEL_ERROR)) {
        return 1;
    }
    if (!Result_Log(Files_Init(args.data), LOG_LEVEL_ERROR)) {
        return 1;
    }
    const bool passed =
        Result_Log(M_RunCases(&args, &options), LOG_LEVEL_ERROR);
    Files_Shutdown();
    return passed ? 0 : 1;
}
