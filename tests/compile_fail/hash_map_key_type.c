// Compiles as it is, and fails to compile with BREAK defined, because it
// then gives a map a key of another type.
#include "core/base/hash_map.h"

typedef struct {
    int32_t font;
    int32_t size;
} FONT_KEY;

typedef struct {
    int32_t id;
} SPRITE;

int32_t M_Use(void);

int32_t M_Use(void)
{
    HASH_MAP(FONT_KEY, int32_t) cache;
    HashMap_Init(&cache, &g_HeapAllocator);
#ifdef BREAK
    const int32_t *const value = HashMap_Get(&cache, ((SPRITE) { 1 }));
#else
    const int32_t *const value = HashMap_Get(&cache, ((FONT_KEY) { 1, 2 }));
#endif
    const int32_t result = value == nullptr ? 0 : *value;
    HashMap_Free(&cache);
    return result;
}
