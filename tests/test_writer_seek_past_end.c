#include "core/io/writer.h"

#include <stdlib.h>

int main(void)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Writer_WriteU16(&writer, 0);
    Writer_Seek(&writer, 3);
    return Result_Ignore(Writer_Close(&writer)) ? EXIT_SUCCESS : EXIT_FAILURE;
}
