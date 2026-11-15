#include "launcher.h"

#include "launch.h"
#include "workdir.h"

#include "core/args.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/base/utils.h"
#include "core/hash/sha1.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include <windows.h>

#include <shlobj.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENV_LINE_SIZE (MAX_PATH + 32)

typedef struct {
    const char *game_dir;
    const char *replay;
    bool live;
    bool dry_run;
    bool fast;
    bool draw;
    bool result;
    int32_t quit_after;
    const char *out_dir;
    ARG_ITEMS seeds;
    ARG_ITEMS keeps;
} M_ARGS;

static const ARG_RULE m_Rules[] = {
    { ARG_ONE_OF, { "--replay", "--live" } },
    { ARG_EXCLUSIVE, { "--fast", "--live" } },
    { ARG_EXCLUSIVE, { "--fast", "--quit-after" } },
    { ARG_REQUIRES, { "--draw", "--fast" } },
    { ARG_REQUIRES, { "--result", "--live" } },
};

static wchar_t *M_Wide(const char *const text)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    wchar_t *const wide = Mem_Alloc((size_t)length * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, length);
    return wide;
}

static RESULT M_CheckGameVersion(
    const LAUNCHER_GAME *const game, const char *const path)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    SHA1 sha1;
    Sha1_Init(&sha1);
    uint8_t chunk[65536];
    while (Reader_Remaining(&reader) > 0 && !Reader_Failed(&reader)) {
        const size_t size = MIN(Reader_Remaining(&reader), sizeof(chunk));
        Reader_ReadBytes(&reader, chunk, size);
        Sha1_Update(&sha1, chunk, size);
    }
    Reader_Close(&reader);
    MUST(Reader_Result(&reader));
    char hex[SHA1_HEX_SIZE];
    Sha1_ToHex(Sha1_Final(&sha1), hex);
    FAIL_IF(
        strcmp(hex, game->exe_sha1) != 0, "%s is not retail %s (sha1 %s)", path,
        game->version, hex);
    return OK;
}

static RESULT M_WriteConfig(
    const LAUNCHER_GAME *const game, const char *const work_dir)
{
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", work_dir, game->cfg_name);
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    game->write_cfg(&writer);
    return Writer_Close(&writer);
}

static RESULT M_OutDir(const M_ARGS *const args, wchar_t dir[MAX_PATH])
{
    wchar_t *const out_dir = M_Wide(args->out_dir);
    const DWORD length = GetFullPathNameW(out_dir, MAX_PATH, dir, nullptr);
    Mem_Free(out_dir);
    FAIL_IF(length == 0 || length >= MAX_PATH, "bad --out %s", args->out_dir);
    SHCreateDirectoryExW(nullptr, dir, nullptr);
    return OK;
}

// Names the case after the replay, or after the time for --live.
static RESULT M_CasePath(const M_ARGS *const args, wchar_t out[MAX_PATH])
{
    wchar_t dir[MAX_PATH];
    MUST(M_OutDir(args, dir));

    wchar_t name[MAX_PATH];
    if (args->replay != nullptr) {
        wchar_t *const replay = M_Wide(args->replay);
        _wsplitpath(replay, nullptr, nullptr, name, nullptr);
        Mem_Free(replay);
    } else {
        SYSTEMTIME now;
        GetLocalTime(&now);
        swprintf(
            name, MAX_PATH, L"live-%04d%02d%02d-%02d%02d%02d", now.wYear,
            now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    }
    swprintf(out, MAX_PATH, L"%ls\\%ls.otrc", dir, name);
    return OK;
}

static RESULT M_Prepare(
    const LAUNCHER_GAME *const game, const M_ARGS *const args,
    const wchar_t *const work_dir)
{
    char work_dir_utf8[MAX_PATH];
    WideCharToMultiByte(
        CP_UTF8, 0, work_dir, -1, work_dir_utf8, MAX_PATH, nullptr, nullptr);
    MUST(M_WriteConfig(game, work_dir_utf8));
    if (args->replay != nullptr) {
        wchar_t copy[MAX_PATH];
        swprintf(copy, MAX_PATH, L"%ls\\%ls", work_dir, game->replay_copy);
        *wcsrchr(copy, L'\\') = L'\0';
        CreateDirectoryW(copy, nullptr);
        swprintf(copy, MAX_PATH, L"%ls\\%ls", work_dir, game->replay_copy);
        wchar_t *const source = M_Wide(args->replay);
        const bool copied = CopyFileW(source, copy, FALSE);
        Mem_Free(source);
        FAIL_IF(!copied, "cannot copy %s", args->replay);
    }
    for (size_t i = 0; i < args->seeds.count; i++) {
        wchar_t *const seed = M_Wide(args->seeds.items[i]);
        const RESULT copied = Workdir_CopyIn(work_dir, seed);
        Mem_Free(seed);
        MUST(copied);
    }
    return OK;
}

static RESULT M_Keep(const M_ARGS *const args, const wchar_t *const work_dir)
{
    wchar_t out_dir[MAX_PATH];
    MUST(M_OutDir(args, out_dir));
    size_t kept = 0;
    for (size_t i = 0; i < args->keeps.count; i++) {
        wchar_t *const name = M_Wide(args->keeps.items[i]);
        if (Result_Log(
                Workdir_CopyOut(work_dir, name, out_dir), LOG_LEVEL_ERROR)) {
            kept++;
        }
        Mem_Free(name);
    }
    FAIL_IF(kept != args->keeps.count, "some files were not kept");
    return OK;
}

static wchar_t *M_Environment(
    const LAUNCHER_GAME *const game, const M_ARGS *const args,
    const wchar_t *const work_dir, const wchar_t *const case_path)
{
    wchar_t lines[7][ENV_LINE_SIZE];
    const wchar_t *extra[7];
    size_t count = 0;
    swprintf(lines[count], ENV_LINE_SIZE, L"CAPTURE_WORK=%ls", work_dir);
    count++;
    if (args->replay != nullptr) {
        swprintf(
            lines[count], ENV_LINE_SIZE, L"CAPTURE_REPLAY=./%ls",
            game->replay_copy);
        count++;
    }
    if (args->fast) {
        swprintf(lines[count], ENV_LINE_SIZE, L"CAPTURE_FAST=1");
        count++;
    }
    if (args->draw) {
        swprintf(lines[count], ENV_LINE_SIZE, L"CAPTURE_DRAW=1");
        count++;
    }
    if (args->result) {
        swprintf(lines[count], ENV_LINE_SIZE, L"CAPTURE_RESULT=1");
        count++;
    }
    if (args->quit_after > 0) {
        swprintf(
            lines[count], ENV_LINE_SIZE, L"CAPTURE_QUIT_AFTER=%d",
            args->quit_after);
        count++;
    }
    if (!args->dry_run) {
        swprintf(lines[count], ENV_LINE_SIZE, L"CAPTURE_CASE=%ls", case_path);
        count++;
    }
    for (size_t i = 0; i < count; i++) {
        extra[i] = lines[i];
    }
    return Launch_BuildEnvironment(extra, count);
}

static RESULT M_Run(
    const LAUNCHER_GAME *const game, const M_ARGS *const args,
    const wchar_t *const work_dir)
{
    wchar_t case_path[MAX_PATH];
    MUST(M_CasePath(args, case_path));
    MUST(M_Prepare(game, args, work_dir));

    char exe_utf8[MAX_PATH];
    snprintf(exe_utf8, MAX_PATH, "%s/%s", args->game_dir, game->exe_name);
    wchar_t *const exe = M_Wide(exe_utf8);
    wchar_t *const game_dir = M_Wide(args->game_dir);
    wchar_t *const environment = M_Environment(game, args, work_dir, case_path);
    wchar_t command_line[MAX_PATH];
    swprintf(command_line, MAX_PATH, L"\"%ls\"", exe);
    LAUNCH_PROCESS process;
    const RESULT started =
        Launch_Start(exe, command_line, environment, game_dir, &process);
    Mem_Free(exe);
    Mem_Free(game_dir);
    Mem_Free(environment);
    MUST(started);

    wchar_t dll[MAX_PATH];
    GetModuleFileNameW(nullptr, dll, MAX_PATH);
    wchar_t *const dll_name = wcsrchr(dll, L'\\') + 1;
    swprintf(
        dll_name, MAX_PATH - (size_t)(dll_name - dll), L"%lscapture.dll",
        game->game_id);
    const RESULT injected = Launch_InjectDll(&process, dll);
    if (!IS_OK(injected) || args->dry_run) {
        Launch_Stop(&process);
        return injected;
    }
    const int exit_code = Launch_ResumeAndWait(&process);
    FAIL_IF(exit_code != 0, "the game exited with %d", exit_code);
    return OK;
}

int Launcher_Main(
    const LAUNCHER_GAME *const game, const int argc, char **const argv)
{
    char program[32];
    snprintf(program, sizeof(program), "%lscapture", game->game_id);
    char summary[128];
    snprintf(
        summary, sizeof(summary),
        "Runs the original %ls with the capture tracer and writes the case "
        "of one run.",
        game->game_id);
    char default_out[64];
    snprintf(
        default_out, sizeof(default_out), "../opentouhou-cases/%ls",
        game->game_id);
    char out_help[128];
    snprintf(
        out_help, sizeof(out_help), "write the case to <dir>, %s by default",
        default_out);

    const ARG_OPTION options[] = {
        ARG_STRING_OF(
            M_ARGS, "--game", game_dir, nullptr, "<dir>",
            "the folder of the original game", ARG_REQUIRED),
        ARG_STRING_OF(
            M_ARGS, "--replay", replay, nullptr, "<file.rpy>",
            "capture the playback of a replay"),
        ARG_FLAG_OF(M_ARGS, "--live", live, "capture a game that you play"),
        ARG_FLAG_OF(
            M_ARGS, "--dry-run", dry_run,
            "inject the tracer, then stop the game before it runs"),
        ARG_FLAG_OF(M_ARGS, "--fast", fast, "run the replay as fast as it can"),
        ARG_FLAG_OF(
            M_ARGS, "--draw", draw, "with --fast, still draw the frames"),
        ARG_FLAG_OF(
            M_ARGS, "--result", result,
            "with --live, open the result screen, which saves score.dat when "
            "the game closes"),
        ARG_INT_OF(
            M_ARGS, "--quit-after", quit_after, 0, "<seconds>",
            "close the game after <seconds>"),
        ARG_STRING_OF(M_ARGS, "--out", out_dir, default_out, "<dir>", out_help),
        ARG_LIST_OF(
            M_ARGS, "--seed", seeds, "<file>",
            "copy the file into the work directory first"),
        ARG_LIST_OF(
            M_ARGS, "--keep", keeps, "<name>",
            "copy the file from the work directory to --out at the end"),
    };
    const ARG_SCHEMA schema = ARG_SCHEMA_OF(program, summary, options, m_Rules);

    M_ARGS args;
    bool help;
    if (!Result_Log(
            Args_Parse(&schema, argc, argv, &args, &help), LOG_LEVEL_ERROR)) {
        return 1;
    }
    if (help) {
        Args_PrintHelp(&schema, stdout);
        return 0;
    }
    char exe[MAX_PATH];
    snprintf(exe, MAX_PATH, "%s/%s", args.game_dir, game->exe_name);
    wchar_t work_dir[MAX_PATH];
    if (!Result_Log(M_CheckGameVersion(game, exe), LOG_LEVEL_ERROR)
        || !Result_Log(
            Workdir_CreateTemp(game->game_id, work_dir, MAX_PATH),
            LOG_LEVEL_ERROR)) {
        return 1;
    }
    const bool ran = Result_Log(M_Run(game, &args, work_dir), LOG_LEVEL_ERROR);
    const bool kept = Result_Log(M_Keep(&args, work_dir), LOG_LEVEL_ERROR);
    Workdir_RemoveTemp(work_dir);
    return ran && kept ? 0 : 1;
}
