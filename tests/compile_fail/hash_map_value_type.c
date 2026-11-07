// Compiles as it is, and fails to compile with BREAK defined, because it
// then reads a map's value through a pointer to another type.
#include "core/base/hash_map.h"

typedef struct {
    int32_t texture;
} FONT;

typedef struct {
    int32_t id;
} SPRITE;

int32_t M_Use(void);

int32_t M_Use(void)
{
    HASH_MAP(int32_t, FONT) fonts;
    HashMap_Init(&fonts, &g_HeapAllocator);
#ifdef BREAK
    SPRITE *const value = HashMap_GetOrPut(&fonts, 1);
    const int32_t result = value->id;
#else
    FONT *const value = HashMap_GetOrPut(&fonts, 1);
    const int32_t result = value->texture;
#endif
    HashMap_Free(&fonts);
    return result;
}
