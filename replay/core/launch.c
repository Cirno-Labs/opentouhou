// SPDX-License-Identifier: GPL-3.0-only

#include "launch.h"

#include "core/base/memory.h"

#include <stdint.h>

wchar_t *Launch_BuildEnvironment(
    const wchar_t *const *extra, size_t extra_count)
{
    wchar_t *parent_env = GetEnvironmentStringsW();
    size_t parent_size = 0;
    while (parent_env[parent_size] != L'\0'
           || parent_env[parent_size + 1] != L'\0') {
        parent_size++;
    }
    parent_size += 2;

    size_t extra_size = 1;
    for (size_t i = 0; i < extra_count; i++) {
        extra_size += wcslen(extra[i]) + 1;
    }

    size_t total = parent_size + extra_size;
    wchar_t *block = Mem_Alloc(total * sizeof(wchar_t));
    memcpy(block, parent_env, parent_size * sizeof(wchar_t));
    FreeEnvironmentStringsW(parent_env);

    size_t offset = parent_size - 1;
    for (size_t i = 0; i < extra_count; i++) {
        size_t line_size = wcslen(extra[i]) + 1;
        memcpy(block + offset, extra[i], line_size * sizeof(wchar_t));
        offset += line_size;
    }
    block[offset] = L'\0';
    return block;
}

RESULT Launch_Start(
    const wchar_t *exe_path, wchar_t *command_line, wchar_t *environment,
    const wchar_t *working_dir, LAUNCH_PROCESS *out_process)
{
    STARTUPINFOW startup_info = {
        .cb = sizeof(startup_info),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdInput = GetStdHandle(STD_INPUT_HANDLE),
        .hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE),
        .hStdError = GetStdHandle(STD_ERROR_HANDLE),
    };
    PROCESS_INFORMATION process_info;
    if (!CreateProcessW(
            exe_path, command_line, nullptr, nullptr, TRUE,
            CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, environment,
            working_dir, &startup_info, &process_info)) {
        return FAIL(
            "CreateProcessW failed: %lu", (unsigned long)GetLastError());
    }
    out_process->process = process_info.hProcess;
    out_process->thread = process_info.hThread;
    return OK;
}

RESULT Launch_InjectDll(const LAUNCH_PROCESS *process, const wchar_t *dll_path)
{
    size_t path_size = (wcslen(dll_path) + 1) * sizeof(wchar_t);
    LPVOID remote_path = VirtualAllocEx(
        process->process, nullptr, path_size, MEM_COMMIT, PAGE_READWRITE);
    if (remote_path == nullptr) {
        return FAIL(
            "VirtualAllocEx failed: %lu", (unsigned long)GetLastError());
    }
    if (!WriteProcessMemory(
            process->process, remote_path, dll_path, path_size, nullptr)) {
        return FAIL(
            "WriteProcessMemory failed: %lu", (unsigned long)GetLastError());
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE load_library =
        (LPTHREAD_START_ROUTINE)(uintptr_t)GetProcAddress(
            kernel32, "LoadLibraryW");
    HANDLE thread = CreateRemoteThread(
        process->process, nullptr, 0, load_library, remote_path, 0, nullptr);
    if (thread == nullptr) {
        return FAIL(
            "CreateRemoteThread failed: %lu", (unsigned long)GetLastError());
    }

    WaitForSingleObject(thread, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    if (exit_code == 0) {
        return FAIL("LoadLibraryW failed in the child process");
    }
    return OK;
}

int Launch_ResumeAndWait(LAUNCH_PROCESS *process)
{
    ResumeThread(process->thread);
    CloseHandle(process->thread);
    WaitForSingleObject(process->process, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(process->process, &exit_code);
    CloseHandle(process->process);
    return (int)exit_code;
}

void Launch_Stop(LAUNCH_PROCESS *process)
{
    TerminateProcess(process->process, 1);
    WaitForSingleObject(process->process, INFINITE);
    CloseHandle(process->thread);
    CloseHandle(process->process);
}
