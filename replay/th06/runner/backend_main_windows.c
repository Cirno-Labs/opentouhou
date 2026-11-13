#include "main.h"

#include "core/base/memory.h"

#include <stdio.h>
#include <windows.h>

int wmain(const int argc, wchar_t **const argv)
{
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
        result = RunnerMain_Run(argc, utf8);
    } else {
        fputs("cannot convert command-line arguments to UTF-8\n", stderr);
    }
    for (int i = 0; i < converted; i++) {
        Mem_Free(utf8[i]);
    }
    Mem_Free(utf8);
    return result;
}
