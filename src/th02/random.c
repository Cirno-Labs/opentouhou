#include "th02/random_private.h"

#include "pc98/zun/master_rng.h"

#include <string.h>

static RANDOM_STATE m_Rng;
const RANDOM_STATE *const g_Rng = &m_Rng;
RANDOM_STATE *const p_Rng = &m_Rng;

void Random_Reset(void)
{
    memset(&m_Rng, 0, sizeof(m_Rng));
    m_Rng.seed = 1;
}

void Random_Seed(const uint32_t seed)
{
    m_Rng.seed = seed;
}

uint16_t Random_U15(void)
{
    return MasterRng_Next(&m_Rng.seed);
}

void Random_Fill(void)
{
    for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
        m_Rng.ring[i] = (uint8_t)Random_U15();
    }
}

uint8_t Random_RingU8(void)
{
    const uint8_t cursor = m_Rng.cursor++;
    return m_Rng.ring[cursor];
}

uint8_t Random_RingU8And(const uint8_t mask)
{
    return Random_RingU8() & mask;
}

uint16_t Random_RingU16(void)
{
    const uint8_t cursor = m_Rng.cursor++;
    const uint16_t high = cursor == 255 ? 0 : m_Rng.ring[cursor + 1];
    return (uint16_t)(m_Rng.ring[cursor] | (high << 8));
}

uint8_t Random_RingU8Range(const uint8_t minimum, const uint8_t maximum)
{
    const int32_t width = (int32_t)maximum - minimum;
    const uint8_t next = Random_RingU8();
    return (uint8_t)(minimum + (width != 0 ? next % width : next));
}

uint16_t Random_RingU16Range(const uint16_t minimum, const uint16_t maximum)
{
    const uint16_t width = (uint16_t)(maximum - minimum);
    const uint16_t next = Random_RingU16();
    return (uint16_t)(minimum + (width != 0 ? next % width : next));
}
