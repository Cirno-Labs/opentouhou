#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Replaces the 32-bit value at address. Reports failure and changes nothing
// when it does not hold old_value.
RESULT Patch_Value32(uintptr_t address, uint32_t old_value, uint32_t new_value);

// Points the call instruction at address from old_target to new_target.
RESULT Patch_Call(
    uintptr_t address, uintptr_t old_target, uintptr_t new_target);

// Replaces all calls to old_target in a function. Checks expected_count,
// at most 16, before changing any instruction.
RESULT Patch_CallsIn(
    uintptr_t start, size_t size, uintptr_t old_target, uintptr_t new_target,
    size_t expected_count);
