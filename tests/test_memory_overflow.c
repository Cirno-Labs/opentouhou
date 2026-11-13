// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/memory.h"

#include <stdint.h>
#include <stdlib.h>

int main(void)
{
    Mem_Free(Mem_AllocArr(SIZE_MAX / 2, 3));
    return EXIT_SUCCESS;
}
