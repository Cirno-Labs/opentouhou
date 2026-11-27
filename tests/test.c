// The case registry, the failure counter, the running-case name and main()
// for the harness tests/test.h declares. One instance per executable, true
// globals so a fixture in its own translation unit shares them.

#include "tests/test.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <unistd.h>
#endif
#if __has_include(<execinfo.h>)
    #include <execinfo.h>
    #define M_TEST_HAS_BACKTRACE 1
#endif

TEST_CASE g_TestCases[TEST_MAX_CASES];
int g_TestCaseCount;
int g_TestFailures;
const char *volatile g_TestRunning;

void Test_Register(const char *const name, void (*const func)(void))
{
    if (g_TestCaseCount >= TEST_MAX_CASES) {
        fprintf(stderr, "%s does not fit, raise TEST_MAX_CASES\n", name);
        abort();
    }
    g_TestCases[g_TestCaseCount++] = (TEST_CASE) { name, func };
}

#ifndef _WIN32

// Names the case that runs when meson stops a test that ran out of time,
// with a backtrace where the C library has one, and then ends as the
// signal would have. Calls only functions that are safe in a handler.
static void M_OnTerm(const int signal_number)
{
    const char *const name = g_TestRunning;
    static const char before[] = "  KILLED  ";
    write(STDOUT_FILENO, before, sizeof(before) - 1);
    if (name != nullptr) {
        write(STDOUT_FILENO, name, strlen(name));
    }
    write(STDOUT_FILENO, "\n", 1);
    #ifdef M_TEST_HAS_BACKTRACE
    void *frames[64];
    backtrace_symbols_fd(frames, backtrace(frames, 64), STDOUT_FILENO);
    #endif
    signal(signal_number, SIG_DFL);
    raise(signal_number);
}
#endif

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
#ifndef _WIN32
    signal(SIGTERM, M_OnTerm);
#endif
    int failed = 0;
    for (int i = 0; i < g_TestCaseCount; i++) {
        const int failures_before = g_TestFailures;
        g_TestRunning = g_TestCases[i].name;
        g_TestCases[i].func();
        if (g_TestFailures == failures_before) {
            printf("  PASS  %s\n", g_TestCases[i].name);
        } else {
            printf("  FAIL  %s\n", g_TestCases[i].name);
            failed++;
        }
    }
    printf("%d passed, %d failed\n", g_TestCaseCount - failed, failed);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
