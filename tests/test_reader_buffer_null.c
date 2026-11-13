// SPDX-License-Identifier: GPL-3.0-only

#include "core/io/reader.h"

#include <stdlib.h>

int main(void)
{
    READER reader;
    Reader_OpenBuffer(&reader, nullptr, 4);
    return EXIT_SUCCESS;
}
