#include "pc98/pmd/random.h"

uint16_t PmdRandom_Next(uint16_t *const seed, const uint16_t limit)
{
    *seed = (uint16_t)(((uint32_t)*seed * 259 + 3) & 0x7fff);
    return (uint16_t)((uint32_t)*seed * limit / 32767);
}
