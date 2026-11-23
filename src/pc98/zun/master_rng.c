#include "pc98/zun/master_rng.h"

uint16_t MasterRng_Next(uint32_t *const state)
{
    *state = *state * UINT32_C(0x015a4e35) + 1;
    return (uint16_t)((*state >> 16) & 0x7fff);
}
