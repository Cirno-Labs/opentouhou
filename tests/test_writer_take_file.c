#include "core/base/string.h"
#include "core/io/writer.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 1) {
        return EXIT_FAILURE;
    }
    char *const path = Str_Format("%s.out", argv[0]);
    WRITER writer;
    if (!Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR)) {
        return EXIT_FAILURE;
    }
    size_t size;
    Writer_TakeBuffer(&writer, &size);
    return EXIT_SUCCESS;
}
