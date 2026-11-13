// SPDX-License-Identifier: GPL-3.0-only

// Compiles as it is, and fails to compile with BREAK defined, because it
// then passes an array a slice of another element type.
#include "core/base/array.h"

void M_Use(void);

void M_Use(void)
{
    ARRAY(int32_t) values;
    Array_Init(&values, &g_HeapAllocator);
#ifdef BREAK
    const int16_t more[] = { 1, 2 };
#else
    const int32_t more[] = { 1, 2 };
#endif
    Array_AppendSlice(&values, more, 2);
    Array_Free(&values);
}
