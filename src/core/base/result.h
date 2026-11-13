// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/log.h"
#include "core/base/utils.h"

// Names the source position where a failure was raised. Each failure site
// has one static RESULT_SITE, so a RESULT refers to it by pointer.
typedef struct {
    const char *file;
    int line;
} RESULT_SITE;

// Holds the outcome of an operation that can fail. A success has no site. A
// failure has the site where it was raised and a message, which the RESULT
// owns.
//
// FAIL creates a failure and MUST passes one up to the calling function,
// optionally with context. Every RESULT must reach a consumer, which
// releases its message: Result_Log logs a failure and Result_Ignore drops
// it. The compiler warns when a returned RESULT is discarded.
typedef struct [[nodiscard]] {
    const RESULT_SITE *site;
    char *message;
} RESULT;

#define OK ((RESULT) {})
#define IS_OK(result_) ((result_).site == nullptr)

// Evaluates to a pointer to the RESULT_SITE of the current position. Usable
// inside a function only.
#define RESULT_SITE_HERE                                                       \
    __extension__({                                                            \
        static const RESULT_SITE site_ = {                                     \
            .file = __FILE__,                                                  \
            .line = __LINE__,                                                  \
        };                                                                     \
        &site_;                                                                \
    })

// Creates a failure with a printf-style message.
//
// Example:
// return FAIL("%s: unknown version %d", path, version);
#define FAIL(...) Result_Fail(RESULT_SITE_HERE, __VA_ARGS__)

// Returns a failure from the enclosing function when the condition is true.
#define FAIL_IF(cond_, ...)                                                    \
    do {                                                                       \
        if (cond_) {                                                           \
            return FAIL(__VA_ARGS__);                                          \
        }                                                                      \
    } while (0)

// Returns a failed result from the enclosing function. Optional arguments
// form a printf-style context that is put in front of the message. Nothing
// else runs between the failure and the return, so the enclosing function
// must hold no resource that needs release at that point.
//
// Example:
// MUST(M_ReadHeader(reader), "%s", path);
#define MUST(result_, ...)                                                     \
    do {                                                                       \
        RESULT must_result_ = (result_);                                       \
        if (!IS_OK(must_result_)) {                                            \
            __VA_OPT__(                                                        \
                must_result_ = Result_Prefix(must_result_, __VA_ARGS__);)      \
            return must_result_;                                               \
        }                                                                      \
    } while (0)

PRINTF_FORMAT(2, 3)
RESULT Result_Fail(const RESULT_SITE *site, const char *fmt, ...);

// Puts a printf-style context and ": " in front of a failure's message. A
// success is returned unchanged.
PRINTF_FORMAT(2, 3)
RESULT Result_Prefix(RESULT result, const char *fmt, ...);

// Consumes a result. A failure is logged at the level given, with the
// position of its failure site. Returns whether the result was a success.
bool Result_Log(RESULT result, LOG_LEVEL level);

// Consumes a result without logging it. Returns whether the result was a
// success.
bool Result_Ignore(RESULT result);
