// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/main.h"

#include "core/base/memory.h"
#include "core/io/path.h"

#include "version.h"

#include <io.h>
#include <stdio.h>
#include <windows.h>

// Returns whether othpatch is the only process on its console, which is
// the case when it was started by a double-click rather than from a
// terminal. The console then closes as soon as othpatch ends.
static bool M_OwnsConsole(void)
{
    DWORD processes[2];
    return GetConsoleProcessList(processes, 2) == 1;
}

int wmain(const int argc, wchar_t **const argv)
{
    SetConsoleOutputCP(CP_UTF8);
    char **const utf8 = Mem_AllocArrZ((size_t)argc + 1, sizeof(char *));
    int converted = 0;
    for (; converted < argc; converted++) {
        const int size = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, argv[converted], -1, nullptr, 0,
            nullptr, nullptr);
        if (size == 0) {
            break;
        }
        utf8[converted] = Mem_Alloc((size_t)size);
        if (WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, argv[converted], -1,
                utf8[converted], size, nullptr, nullptr)
            == 0) {
            Mem_Free(utf8[converted]);
            break;
        }
    }
    int result = 1;
    if (converted == argc) {
        FILE *const in = _isatty(_fileno(stdin)) ? stdin : nullptr;
        result = Main_Run(
            argc, utf8, OTHPATCH_VERSION, Path_GetExeDir(), in, stdout);
    } else {
        fputs("othpatch: cannot convert the arguments to UTF-8\n", stdout);
    }
    for (int i = 0; i < converted; i++) {
        Mem_Free(utf8[i]);
    }
    Mem_Free(utf8);
    if (M_OwnsConsole()) {
        fputs("\nPress Enter to close this window.\n", stdout);
        fflush(stdout);
        getchar();
    }
    return result;
}
