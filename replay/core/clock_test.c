#include "clock.h"

#include <windows.h>

#include <mmsystem.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

// Checks the clock hooks (clock.c) on this program's own imports: real
// time first, then the stepping clock in a second run of the program,
// since a clock is installed once per process.

#define STEP 17

static int m_Failures;

static void M_Check(bool passed, const char *name)
{
    printf("%s: %s\n", passed ? "PASS" : "FAIL", name);
    if (!passed) {
        m_Failures++;
    }
}

static LONGLONG M_Counter(void)
{
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
}

static LONGLONG M_Frequency(void)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

static void M_CheckStepping(void)
{
    M_Check(
        Clock_Install(GetModuleHandleW(nullptr), STEP),
        "Install finds the imports");
    M_Check(timeGetTime() == 0, "the first call returns 0");
    M_Check(M_Counter() == STEP * 1000, "the counter is on the same clock");
    M_Check(timeGetTime() == 2 * STEP, "each call moves the clock on");
    M_Check(M_Frequency() == 1000000, "the frequency is 1,000,000");
    M_Check(timeGetTime() == 3 * STEP, "the frequency leaves the clock");
}

static bool M_RunStepping(void)
{
    wchar_t command_line[MAX_PATH + 16];
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    swprintf(command_line, MAX_PATH + 16, L"\"%ls\" --step", exe);
    STARTUPINFOW startup = { .cb = sizeof(startup) };
    PROCESS_INFORMATION process;
    if (!CreateProcessW(
            exe, command_line, nullptr, nullptr, TRUE, 0, nullptr, nullptr,
            &startup, &process)) {
        return false;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exit_code == 0;
}

static void M_CheckRealTime(void)
{
    M_Check(
        Clock_Install(GetModuleHandleW(nullptr), 0),
        "Install finds the imports");
    const DWORD start = timeGetTime();
    M_Check(start < 1000, "the clock starts from 0");
    Sleep(100);
    const DWORD now = timeGetTime();
    M_Check(now - start >= 90, "the clock follows real time");
    const LONGLONG count = M_Counter();
    M_Check(
        count >= (LONGLONG)now * 1000 && count < ((LONGLONG)now + 1000) * 1000,
        "the counter is on the same clock");
    M_Check(M_Frequency() == 1000000, "the frequency is 1,000,000");
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--step") == 0) {
        M_CheckStepping();
        return m_Failures == 0 ? 0 : 1;
    }
    M_CheckRealTime();
    M_Check(M_RunStepping(), "the stepping clock passes");
    if (m_Failures == 0) {
        printf("all checks passed\n");
    } else {
        printf("%d check(s) failed\n", m_Failures);
    }
    return m_Failures == 0 ? 0 : 1;
}
