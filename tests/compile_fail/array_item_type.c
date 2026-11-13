// SPDX-License-Identifier: GPL-3.0-only

// Compiles as it is, and fails to compile with BREAK defined, because it
// then passes an array an item of another type.
#include "core/base/array.h"

typedef struct {
    int32_t id;
} SPRITE;

typedef struct {
    float x;
} POINT;

void M_Use(void);

void M_Use(void)
{
    ARRAY(SPRITE) sprites;
    Array_Init(&sprites, &g_HeapAllocator);
#ifdef BREAK
    Array_Append(&sprites, ((POINT) { 1.0f }));
#else
    Array_Append(&sprites, ((SPRITE) { 1 }));
#endif
    Array_Free(&sprites);
}
