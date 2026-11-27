#include "replay/retail/retail.h"

#include "core/base/log.h"
#include "core/hash/sha1.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define M_ARENA_SIZE (1024 * 1024)

// GAMEretail.exe, with the command line of Run_Parse and absolute paths,
// starts the game's program in game_dir suspended, makes work/ of
// out_dir, injects GAMEretail.dll and writes its command line to the
// program's standard input.

static wchar_t *M_Wide(ARENA *const arena, const char *const text)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    wchar_t *const wide = Arena_AllocArrZ(
        arena, (size_t)(length > 0 ? length : 1), sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, length);
    return wide;
}

// Returns the SHA-1 of the file at path as hex text from arena.
static RESULT M_HashFile(
    const char *const path, ARENA *const arena, const char **const out)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    SHA1 sha1;
    Sha1_Init(&sha1);
    uint8_t buffer[65536];
    for (size_t left = Reader_Remaining(&reader); left > 0;) {
        const size_t size = left < sizeof(buffer) ? left : sizeof(buffer);
        Reader_ReadBytes(&reader, buffer, size);
        Sha1_Update(&sha1, buffer, size);
        left -= size;
    }
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    MUST(result);
    char *const hex = Arena_Alloc(arena, SHA1_HEX_SIZE);
    Sha1_ToHex(Sha1_Final(&sha1), hex);
    *out = hex;
    return OK;
}

static RESULT M_Stage(
    const RETAIL_PROGRAM *const program, const RUN *const run,
    ARENA *const arena)
{
    const ALLOCATOR *const allocator = Arena_Allocator(arena);
    const char *const exe = Path_Join(allocator, run->game_dir, program->exe);
    const char *sha1;
    MUST(M_HashFile(exe, arena, &sha1));
    FAIL_IF(
        strcmp(sha1, program->exe_sha1) != 0, "%s has SHA-1 %s, not %s", exe,
        sha1, program->exe_sha1);
    const char *const work = Path_Join(allocator, run->out_dir, "work");
    MUST(File_MakeDir(run->out_dir));
    return File_MakeDir(work);
}

static RESULT M_Inject(const HANDLE process, const wchar_t *const dll)
{
    const size_t size = (wcslen(dll) + 1) * sizeof(wchar_t);
    void *const remote =
        VirtualAllocEx(process, nullptr, size, MEM_COMMIT, PAGE_READWRITE);
    FAIL_IF(
        remote == nullptr
            || !WriteProcessMemory(process, remote, dll, size, nullptr),
        "cannot write into the game: %lu", (unsigned long)GetLastError());
    const LPTHREAD_START_ROUTINE load_library =
        (LPTHREAD_START_ROUTINE)(uintptr_t)GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    const HANDLE thread = CreateRemoteThread(
        process, nullptr, 0, load_library, remote, 0, nullptr);
    FAIL_IF(
        thread == nullptr, "cannot start a thread in the game: %lu",
        (unsigned long)GetLastError());
    WaitForSingleObject(thread, INFINITE);
    DWORD loaded = 0;
    GetExitCodeThread(thread, &loaded);
    CloseHandle(thread);
    FAIL_IF(loaded == 0, "the game cannot load %ls", dll);
    return OK;
}

// Starts the program suspended in game_dir with a pipe as its standard
// input, which carries the command line to the DLL.
static RESULT M_Start(
    const RETAIL_PROGRAM *const program, const RUN *const run, const int argc,
    char **const argv, ARENA *const arena, PROCESS_INFORMATION *const out)
{
    const char *const exe =
        Path_Join(Arena_Allocator(arena), run->game_dir, program->exe);
    wchar_t *const exe_w = M_Wide(arena, exe);
    const size_t line_size = wcslen(exe_w) + 3;
    wchar_t *const line = Arena_AllocArrZ(arena, line_size, sizeof(wchar_t));
    swprintf(line, line_size, L"\"%ls\"", exe_w);
    SECURITY_ATTRIBUTES inherit = {
        .nLength = sizeof(inherit),
        .bInheritHandle = TRUE,
    };
    HANDLE in_read;
    HANDLE in_write;
    FAIL_IF(
        !CreatePipe(&in_read, &in_write, &inherit, 0), "cannot make a pipe");
    SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup = {
        .cb = sizeof(startup),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdInput = in_read,
        .hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE),
        .hStdError = GetStdHandle(STD_ERROR_HANDLE),
    };
    const bool started = CreateProcessW(
        exe_w, line, nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr,
        M_Wide(arena, run->game_dir), &startup, out);
    CloseHandle(in_read);
    bool sent = started;
    for (int i = 0; sent && i < argc; i++) {
        DWORD written = 0;
        const DWORD size = (DWORD)strlen(argv[i]) + 1;
        sent = WriteFile(in_write, argv[i], size, &written, nullptr)
            && written == size;
    }
    CloseHandle(in_write);
    FAIL_IF(
        !started, "cannot start %s: %lu", exe, (unsigned long)GetLastError());
    if (!sent) {
        TerminateProcess(out->hProcess, 2);
        return FAIL("cannot pass the command line to the game");
    }
    return OK;
}

static RESULT M_Launch(
    const RETAIL_PROGRAM *const program, const int argc, char **const argv,
    const RUN *const run, ARENA *const arena, int *const status)
{
    MUST(M_Stage(program, run, arena));

    wchar_t dll[MAX_PATH];
    const DWORD self = GetModuleFileNameW(nullptr, dll, MAX_PATH);
    wchar_t *const slash = wcsrchr(dll, L'\\');
    FAIL_IF(
        self == 0 || self >= MAX_PATH || slash == nullptr,
        "cannot find the DLL");
    const size_t room = (size_t)(dll + MAX_PATH - (slash + 1));
    swprintf(slash + 1, room, L"%ls", M_Wide(arena, program->dll));

    PROCESS_INFORMATION process;
    MUST(M_Start(program, run, argc, argv, arena, &process));
    const RESULT injected = M_Inject(process.hProcess, dll);
    if (!IS_OK(injected)) {
        TerminateProcess(process.hProcess, 2);
    } else {
        ResumeThread(process.hThread);
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 2;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    MUST(injected);
    FAIL_IF(code > 2, "the game exited with %lu", (unsigned long)code);
    *status = (int)code;
    return OK;
}

int Retail_Launch(
    const RETAIL_PROGRAM *const program, const int argc, char **const argv)
{
    ARENA arena;
    Arena_Init(&arena, "retail launch", M_ARENA_SIZE);
    int status = 2;
    RUN run;
    bool help = false;
    RESULT result = Run_Parse(argv[0], argc, argv, &arena, &run, &help);
    if (IS_OK(result) && !help) {
        result = M_Launch(program, argc, argv, &run, &arena, &status);
    }
    if (!Result_Log(result, LOG_LEVEL_ERROR)) {
        status = 2;
    } else if (help) {
        status = 0;
    }
    Arena_Free(&arena);
    return status;
}
