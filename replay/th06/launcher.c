#include "../core/launch.h"
#include "../core/workdir.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/base/sha1.h"
#include "core/base/utils.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include <windows.h>

#include <shlobj.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_EXE_NAME "東方紅魔郷.exe"
#define GAME_EXE_SHA1 "e61b4f4fea3802e926ef307f45166599c3e86555"

// Names the replay copy so that the game's replay menu lists it.
#define REPLAY_COPY L"replay\\th6_ud0000.rpy"

#define DEFAULT_OUT_DIR "../opentouhou-cases/th06"

typedef struct {
    const char *game_dir;
    const char *replay;
    bool live;
    bool dry_run;
    bool fast;
    int quit_after;
    const char *out_dir;
} ARGS;

static bool M_ParseArgs(const int argc, char **const argv, ARGS *const args)
{
    *args = (ARGS) { .out_dir = DEFAULT_OUT_DIR };
    for (int i = 1; i < argc; i++) {
        const bool has_value = i + 1 < argc;
        if (strcmp(argv[i], "--game") == 0 && has_value) {
            args->game_dir = argv[++i];
        } else if (strcmp(argv[i], "--replay") == 0 && has_value) {
            args->replay = argv[++i];
        } else if (strcmp(argv[i], "--live") == 0) {
            args->live = true;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            args->dry_run = true;
        } else if (strcmp(argv[i], "--fast") == 0) {
            args->fast = true;
        } else if (strcmp(argv[i], "--quit-after") == 0 && has_value) {
            args->quit_after = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--out") == 0 && has_value) {
            args->out_dir = argv[++i];
        } else {
            return false;
        }
    }
    return args->game_dir != nullptr && (args->replay != nullptr) != args->live
        && !(args->fast && args->live) && !(args->fast && args->quit_after > 0);
}

static wchar_t *M_Wide(const char *const text)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    wchar_t *const wide = Mem_Alloc((size_t)length * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, length);
    return wide;
}

static RESULT M_CheckGameVersion(const char *const path)
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
        strcmp(hex, GAME_EXE_SHA1) != 0, "%s is not retail 1.02h (sha1 %s)",
        path, hex);
    return OK;
}

// Writes the shipped key mapping, windowed, no frame skip, no sound.
static RESULT M_WriteConfig(const char *const work_dir)
{
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/東方紅魔郷.cfg", work_dir);
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));

    // Shoot, bomb, focus, menu, up, down, left, right, skip.
    const int16_t buttons[] = { 0, 1, 2, 4, -1, -1, -1, -1, 3 };
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
        Writer_Write(&writer, &buttons[i]);
    }
    Writer_WriteU16(&writer, 0);
    Writer_WriteU32(&writer, 0x102); // version
    Writer_WriteU8(&writer, 2); // lives
    Writer_WriteU8(&writer, 3); // bombs
    Writer_WriteU8(&writer, 0); // 16-bit colour
    Writer_WriteU8(&writer, 0); // music mode
    Writer_WriteU8(&writer, 0); // sound effects
    Writer_WriteU8(&writer, 1); // difficulty
    Writer_WriteU8(&writer, 1); // windowed
    Writer_WriteU8(&writer, 0); // frame skip
    Writer_WriteI16(&writer, 600); // pad threshold x
    Writer_WriteI16(&writer, 600); // pad threshold y
    for (int i = 0; i < 4; i++) {
        Writer_WriteU32(&writer, 0);
    }
    Writer_WriteU32(&writer, 1); // flags: colour-add emulation
    ASSERT(Writer_Tell(&writer) == 0x38);
    return Writer_Close(&writer);
}

// Names the case after the replay, or after the time for --live.
static RESULT M_CasePath(const ARGS *const args, wchar_t out[MAX_PATH])
{
    wchar_t *const out_dir = M_Wide(args->out_dir);
    wchar_t dir[MAX_PATH];
    const DWORD length = GetFullPathNameW(out_dir, MAX_PATH, dir, nullptr);
    Mem_Free(out_dir);
    FAIL_IF(length == 0 || length >= MAX_PATH, "bad --out %s", args->out_dir);
    SHCreateDirectoryExW(nullptr, dir, nullptr);

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

static RESULT M_Prepare(const ARGS *const args, const wchar_t *const work_dir)
{
    char work_dir_utf8[MAX_PATH];
    WideCharToMultiByte(
        CP_UTF8, 0, work_dir, -1, work_dir_utf8, MAX_PATH, nullptr, nullptr);
    MUST(M_WriteConfig(work_dir_utf8));
    if (args->replay != nullptr) {
        wchar_t copy[MAX_PATH];
        swprintf(copy, MAX_PATH, L"%ls\\replay", work_dir);
        CreateDirectoryW(copy, nullptr);
        swprintf(copy, MAX_PATH, L"%ls\\" REPLAY_COPY, work_dir);
        wchar_t *const source = M_Wide(args->replay);
        const bool copied = CopyFileW(source, copy, FALSE);
        Mem_Free(source);
        FAIL_IF(!copied, "cannot copy %s", args->replay);
    }
    return OK;
}

static wchar_t *M_Environment(
    const ARGS *const args, const wchar_t *work_dir, const wchar_t *case_path)
{
    wchar_t lines[5][MAX_PATH + 32];
    const wchar_t *extra[5];
    size_t count = 0;
    swprintf(lines[count], MAX_PATH + 32, L"TH06CAPTURE_WORK=%ls", work_dir);
    count++;
    if (args->replay != nullptr) {
        swprintf(
            lines[count], MAX_PATH + 32, L"TH06CAPTURE_REPLAY=./%ls",
            REPLAY_COPY);
        count++;
    }
    if (args->fast) {
        swprintf(lines[count], MAX_PATH + 32, L"TH06CAPTURE_FAST=1");
        count++;
    }
    if (args->quit_after > 0) {
        swprintf(
            lines[count], MAX_PATH + 32, L"TH06CAPTURE_QUIT_AFTER=%d",
            args->quit_after);
        count++;
    }
    if (!args->dry_run) {
        swprintf(
            lines[count], MAX_PATH + 32, L"TH06CAPTURE_CASE=%ls", case_path);
        count++;
    }
    for (size_t i = 0; i < count; i++) {
        extra[i] = lines[i];
    }
    return Launch_BuildEnvironment(extra, count);
}

static RESULT M_Run(const ARGS *const args, const wchar_t *const work_dir)
{
    wchar_t case_path[MAX_PATH];
    MUST(M_CasePath(args, case_path));
    MUST(M_Prepare(args, work_dir));

    char exe_utf8[MAX_PATH];
    snprintf(exe_utf8, MAX_PATH, "%s/%s", args->game_dir, GAME_EXE_NAME);
    wchar_t *const exe = M_Wide(exe_utf8);
    wchar_t *const game_dir = M_Wide(args->game_dir);
    wchar_t *const environment = M_Environment(args, work_dir, case_path);
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
    wcscpy(wcsrchr(dll, L'\\') + 1, L"th06capture.dll");
    const RESULT injected = Launch_InjectDll(&process, dll);
    if (!IS_OK(injected) || args->dry_run) {
        Launch_Stop(&process);
        return injected;
    }
    const int exit_code = Launch_ResumeAndWait(&process);
    FAIL_IF(exit_code != 0, "the game exited with %d", exit_code);
    return OK;
}

int main(void)
{
    int argc;
    wchar_t **const wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    char buffers[16][MAX_PATH];
    char *argv[16];
    for (int i = 0; i < argc && i < 16; i++) {
        WideCharToMultiByte(
            CP_UTF8, 0, wide_argv[i], -1, buffers[i], MAX_PATH, nullptr,
            nullptr);
        argv[i] = buffers[i];
    }
    LocalFree(wide_argv);

    ARGS args;
    if (argc > 16 || !M_ParseArgs(argc, argv, &args)) {
        fprintf(
            stderr,
            "usage: th06capture.exe --game <dir> "
            "(--replay <file.rpy> | --live) [--dry-run] [--fast] "
            "[--quit-after <seconds>] [--out <dir>]\n");
        return 1;
    }
    char exe[MAX_PATH];
    snprintf(exe, MAX_PATH, "%s/%s", args.game_dir, GAME_EXE_NAME);
    wchar_t work_dir[MAX_PATH];
    if (!Result_Log(M_CheckGameVersion(exe), LOG_LEVEL_ERROR)
        || !Result_Log(
            Workdir_CreateTemp(L"th06", work_dir, MAX_PATH), LOG_LEVEL_ERROR)) {
        return 1;
    }
    const bool ran = Result_Log(M_Run(&args, work_dir), LOG_LEVEL_ERROR);
    Workdir_RemoveTemp(work_dir);
    return ran ? 0 : 1;
}
