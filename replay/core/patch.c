#include "patch.h"

#include <windows.h>

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
