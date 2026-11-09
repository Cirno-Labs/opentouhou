#include "stg/rng.h"

void Rng_SetSeed(RNG *const rng, const uint16_t seed)
{
    rng->seed = seed;
    rng->generated = 0;
}

uint16_t Rng_U16(RNG *const rng)
{
    const uint16_t a = (uint16_t)((rng->seed ^ 0x9630) - 0x6553);
    rng->seed = (uint16_t)(((a & 0xc000) >> 14) + (uint16_t)(a * 4));
    rng->generated++;
    return rng->seed;
}

uint32_t Rng_U32(RNG *const rng)
{
    const uint32_t hi = Rng_U16(rng);
    const uint32_t lo = Rng_U16(rng);
    return (hi << 16) | lo;
}

float Rng_F32(RNG *const rng)
{
    return (float)Rng_U32(rng) / 4294967296.0f;
}

float Rng_F32InRange(RNG *const rng, const float range)
{
    return Rng_F32(rng) * range;
}

uint16_t Rng_U16InRange(RNG *const rng, const uint16_t range)
{
    return range != 0 ? Rng_U16(rng) % range : 0;
}

uint32_t Rng_U32InRange(RNG *const rng, const uint32_t range)
{
    return range != 0 ? Rng_U32(rng) % range : 0;
}
