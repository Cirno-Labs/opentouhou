#pragma once

// Provides a minimal unit test harness. Each test file includes this header
// once, defines its cases with TEST, and gets a main function that runs every
// case. The executable exits with failure if any check fails, which is what
// meson test reads.

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

#define TEST_MAX_CASES 128

typedef struct {
    const char *name;
    void (*func)(void);
} TEST_CASE;

static TEST_CASE m_TestCases[TEST_MAX_CASES];
static int m_TestCaseCount;
static int m_TestFailures;
static const char *volatile m_TestRunning;

static void Test_Register(const char *const name, void (*const func)(void))
{
    if (m_TestCaseCount >= TEST_MAX_CASES) {
        fprintf(stderr, "%s does not fit, raise TEST_MAX_CASES\n", name);
        abort();
    }
    m_TestCases[m_TestCaseCount++] = (TEST_CASE) { name, func };
}

// Defines a test case. Cases register themselves before main runs, so a file
// needs no list of its cases.
#define TEST(name_)                                                            \
    static void name_(void);                                                   \
    __attribute__((constructor)) static void M_Register_##name_(void)          \
    {                                                                          \
        Test_Register(#name_, name_);                                          \
    }                                                                          \
    static void name_(void)

#define TEST_FAIL(fmt_, ...)                                                   \
    do {                                                                       \
        printf(                                                                \
            "    %s:%d: " fmt_ "\n", __FILE__,                                 \
            __LINE__ __VA_OPT__(, ) __VA_ARGS__);                              \
        m_TestFailures++;                                                      \
    } while (0)

#define CHECK(cond_)                                                           \
    do {                                                                       \
        if (!(cond_)) {                                                        \
            TEST_FAIL("expected: %s", #cond_);                                 \
        }                                                                      \
    } while (0)

#define CHECK_EQ_INT(actual_, expected_)                                       \
    do {                                                                       \
        const long long a_ = (long long)(actual_);                             \
        const long long e_ = (long long)(expected_);                           \
        if (a_ != e_) {                                                        \
            TEST_FAIL("%s: expected %lld, got %lld", #actual_, e_, a_);        \
        }                                                                      \
    } while (0)

#define CHECK_EQ_STR(actual_, expected_)                                       \
    do {                                                                       \
        const char *const a_ = (actual_);                                      \
        const char *const e_ = (expected_);                                    \
        if (a_ == nullptr || strcmp(a_, e_) != 0) {                            \
            TEST_FAIL(                                                         \
                "%s: expected \"%s\", got \"%s\"", #actual_, e_,               \
                a_ != nullptr ? a_ : "(null)");                                \
        }                                                                      \
    } while (0)

#ifndef _WIN32
// Names the case that runs when meson stops a test that ran out of time,
// with a backtrace where the C library has one, and then ends as the
// signal would have. Calls only functions that are safe in a handler.
static void M_TestOnTerm(const int signal_number)
{
    const char *const name = m_TestRunning;
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
    signal(SIGTERM, M_TestOnTerm);
#endif
    int failed = 0;
    for (int i = 0; i < m_TestCaseCount; i++) {
        const int failures_before = m_TestFailures;
        m_TestRunning = m_TestCases[i].name;
        m_TestCases[i].func();
        if (m_TestFailures == failures_before) {
            printf("  PASS  %s\n", m_TestCases[i].name);
        } else {
            printf("  FAIL  %s\n", m_TestCases[i].name);
            failed++;
        }
    }
    printf("%d passed, %d failed\n", m_TestCaseCount - failed, failed);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
