// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/arena.h"

#include <stdlib.h>

int main(void)
{
    ARENA arena;
    Arena_Init(&arena, "test", 256);
    Arena_Alloc(&arena, 200);
    Arena_Alloc(&arena, 57);
    Arena_Free(&arena);
    return EXIT_SUCCESS;
}
