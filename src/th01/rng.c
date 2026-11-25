#include "th01/rng.h"

uint32_t g_Rng;

void Rng_Seed(const uint32_t seed)
{
    g_Rng = seed;
}
