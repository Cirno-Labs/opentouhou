// SPDX-License-Identifier: GPL-3.0-only

#include "core/io/reader.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
    READER reader;
    if (argc < 1
        || !Result_Log(Reader_OpenFile(&reader, argv[0]), LOG_LEVEL_ERROR)) {
        return EXIT_FAILURE;
    }
    Reader_Borrow(&reader, 1);
    Reader_Close(&reader);
    return EXIT_SUCCESS;
}
