// SPDX-License-Identifier: GPL-3.0-only

#include "iat.h"

#include <string.h>

bool Iat_Patch(
    HMODULE module, const char *dll, const char *name, uintptr_t hook,
    uintptr_t *original)
{
    uint8_t *base = (uint8_t *)module;
    IMAGE_DOS_HEADER *dos_header = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt_headers =
        (IMAGE_NT_HEADERS *)(base + dos_header->e_lfanew);
    IMAGE_DATA_DIRECTORY import_dir =
        nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_dir.VirtualAddress == 0) {
        return false;
    }

    IMAGE_IMPORT_DESCRIPTOR *descriptor =
        (IMAGE_IMPORT_DESCRIPTOR *)(base + import_dir.VirtualAddress);
    for (; descriptor->Name != 0; descriptor++) {
        const char *dll_name = (const char *)(base + descriptor->Name);
        if (_stricmp(dll_name, dll) != 0) {
            continue;
        }

        uintptr_t name_thunk_rva = descriptor->OriginalFirstThunk != 0
            ? descriptor->OriginalFirstThunk
            : descriptor->FirstThunk;
        IMAGE_THUNK_DATA *name_thunk =
            (IMAGE_THUNK_DATA *)(base + name_thunk_rva);
        IMAGE_THUNK_DATA *address_thunk =
            (IMAGE_THUNK_DATA *)(base + descriptor->FirstThunk);
        for (; name_thunk->u1.AddressOfData != 0;
             name_thunk++, address_thunk++) {
            if ((name_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) != 0) {
                continue;
            }
            IMAGE_IMPORT_BY_NAME *import_name =
                (IMAGE_IMPORT_BY_NAME *)(base + name_thunk->u1.AddressOfData);
            if (strcmp((const char *)import_name->Name, name) != 0) {
                continue;
            }

            uintptr_t *slot = (uintptr_t *)&address_thunk->u1.Function;
            DWORD old_protect;
            if (!VirtualProtect(
                    slot, sizeof(*slot), PAGE_READWRITE, &old_protect)) {
                return false;
            }
            *original = *slot;
            *slot = hook;
            VirtualProtect(slot, sizeof(*slot), old_protect, &old_protect);
            return true;
        }
        return false;
    }
    return false;
}
