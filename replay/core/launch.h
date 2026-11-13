// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/result.h"

#include <windows.h>

typedef struct {
    HANDLE process;
    HANDLE thread;
} LAUNCH_PROCESS;

// Returns this process's environment plus the NAME=value lines in extra.
// Mem_Free releases it.
wchar_t *Launch_BuildEnvironment(
    const wchar_t *const *extra, size_t extra_count);

// Starts exe_path suspended in working_dir. It inherits the standard
// streams, so the DLL logs to the same terminal.
RESULT Launch_Start(
    const wchar_t *exe_path, wchar_t *command_line, wchar_t *environment,
    const wchar_t *working_dir, LAUNCH_PROCESS *out_process);

// Loads dll_path into process and waits until its DllMain has returned.
RESULT Launch_InjectDll(const LAUNCH_PROCESS *process, const wchar_t *dll_path);

// Resumes process, waits for it to exit and returns its exit code.
int Launch_ResumeAndWait(LAUNCH_PROCESS *process);

// Ends process without resuming it.
void Launch_Stop(LAUNCH_PROCESS *process);
