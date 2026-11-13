// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Replaces the 32-bit value at address. Reports failure and changes nothing
// when it does not hold old_value.
RESULT Patch_Value32(uintptr_t address, uint32_t old_value, uint32_t new_value);

// Points the call instruction at address from old_target to new_target.
RESULT Patch_Call(
    uintptr_t address, uintptr_t old_target, uintptr_t new_target);
