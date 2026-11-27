#pragma once

// Declares a minimal unit test harness. Each test file includes this
// header once, defines its cases with TEST, and links test_lib (tests/
// meson.build), which provides the case registry, the failure counter
// and main(). The counter is a true global shared by every translation
// unit a test links, so a fixture in its own .c file can fail a check
// too.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MAX_CASES 128

typedef struct {
    const char *name;
    void (*func)(void);
} TEST_CASE;

extern TEST_CASE g_TestCases[TEST_MAX_CASES];
extern int g_TestCaseCount;
extern int g_TestFailures;
extern const char *volatile g_TestRunning;

void Test_Register(const char *name, void (*func)(void));

// Defines a test case. Cases register themselves before main runs, so a file
// needs no list of its cases.
#define TEST(name_)                                                            \
    static void name_(void);                                                   \
    __attribute__((constructor)) static void Test_Register_##name_(void)       \
    {                                                                          \
        Test_Register(#name_, name_);                                          \
    }                                                                          \
    static void name_(void)

#define TEST_FAIL(fmt_, ...)                                                   \
    do {                                                                       \
        printf(                                                                \
            "    %s:%d: " fmt_ "\n", __FILE__,                                 \
            __LINE__ __VA_OPT__(, ) __VA_ARGS__);                              \
        g_TestFailures++;                                                      \
    } while (0)

// Prints why the test cannot run and exits with meson's skip status, 77.
#define TEST_SKIP(fmt_, ...)                                                   \
    do {                                                                       \
        printf("SKIP: " fmt_ "\n" __VA_OPT__(, ) __VA_ARGS__);                 \
        exit(77);                                                              \
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
