#include "patch.h"

#include <windows.h>

#include <string.h>

RESULT Patch_Value32(
    const uintptr_t address, const uint32_t old_value, const uint32_t new_value)
{
    uint32_t *const slot = (uint32_t *)address;
    if (*slot != old_value) {
        return FAIL(
            "patch at 0x%08x: expected 0x%08x, found 0x%08x", (uint32_t)address,
            old_value, *slot);
    }
    DWORD protect;
    if (!VirtualProtect(
            slot, sizeof(*slot), PAGE_EXECUTE_READWRITE, &protect)) {
        return FAIL(
            "VirtualProtect failed: %lu", (unsigned long)GetLastError());
    }
    *slot = new_value;
    VirtualProtect(slot, sizeof(*slot), protect, &protect);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    return OK;
}

RESULT Patch_Call(
    const uintptr_t address, const uintptr_t old_target,
    const uintptr_t new_target)
{
    const uintptr_t next = address + 5;
    return Patch_Value32(
        address + 1, (uint32_t)(old_target - next),
        (uint32_t)(new_target - next));
}

RESULT Patch_CallsIn(
    const uintptr_t start, const size_t size, const uintptr_t old_target,
    const uintptr_t new_target, const size_t expected_count)
{
    size_t count = 0;
    for (uintptr_t site = start; site + 5 <= start + size; site++) {
        int32_t offset;
        memcpy(&offset, (const void *)(site + 1), sizeof(offset));
        if (*(const uint8_t *)site == 0xe8
            && site + 5 + (uintptr_t)offset == old_target) {
            count++;
        }
    }
    FAIL_IF(
        count != expected_count, "expected %zu calls, found %zu at 0x%08x",
        expected_count, count, (uint32_t)start);
    for (uintptr_t site = start; site + 5 <= start + size; site++) {
        int32_t offset;
        memcpy(&offset, (const void *)(site + 1), sizeof(offset));
        if (*(const uint8_t *)site == 0xe8
            && site + 5 + (uintptr_t)offset == old_target) {
            MUST(Patch_Call(site, old_target, new_target));
            site += 4;
        }
    }
    return OK;
}
