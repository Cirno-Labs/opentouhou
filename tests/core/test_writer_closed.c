#include "core/io/writer.h"

#include <stdlib.h>

int main(void)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Writer_WriteU16(&writer, 0);
    if (!Result_Ignore(Writer_Close(&writer))) {
        return EXIT_FAILURE;
    }
    Writer_WriteU16(&writer, 0);
    return EXIT_SUCCESS;
}
