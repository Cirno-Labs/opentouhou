// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/array.h"

#include <stdlib.h>

int main(void)
{
    ARRAY(int) array;
    Array_Init(&array, &g_HeapAllocator);
    Array_Append(&array, 1);
    const int value = *Array_Get(&array, 1);
    Array_Free(&array);
    return value;
}
