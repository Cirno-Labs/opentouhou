// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/log.h"

// Checks a condition that must hold. A false condition is fatal: the
// condition and its position are logged and the program aborts. The check
// stays in release builds.
#define ASSERT(cond_)                                                          \
    do {                                                                       \
        if (!(cond_)) {                                                        \
            LOG_FATAL("Assertion failed: %s", #cond_);                         \
        }                                                                      \
    } while (0)
