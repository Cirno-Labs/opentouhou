#include "core/io/reader.h"

#include <stdlib.h>

int main(void)
{
    READER reader;
    Reader_OpenBuffer(&reader, nullptr, 4, nullptr);
    return EXIT_SUCCESS;
}
