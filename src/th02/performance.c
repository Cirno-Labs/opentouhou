#include "th02/performance_private.h"

#include "pc98/zun/math16.h"

#include <string.h>

static PERFORMANCE m_Performance;
const PERFORMANCE *const g_Performance = &m_Performance;
PERFORMANCE *const p_Performance = &m_Performance;

void Performance_Init(const int8_t rank)
{
    memset(&m_Performance, 0, sizeof(m_Performance));
    m_Performance.maximum = rank == 0 ? 4 : 16;
}

void Performance_Advance(const uint32_t stage_frame)
{
    if (stage_frame % 1800 == 1500
        && m_Performance.value < m_Performance.maximum) {
        m_Performance.value++;
    }
}

void Performance_MissPenalty(void)
{
    if (m_Performance.value > 2) {
        m_Performance.value = 0;
    } else {
        m_Performance.value = Math16_Wrap(m_Performance.value - 2);
        if (m_Performance.value < -6) {
            m_Performance.value = -6;
        }
    }
}

void Performance_AddCollection(const uint8_t weight)
{
    m_Performance.collected = (uint8_t)(m_Performance.collected + weight);
}

void Performance_CommitCollection(void)
{
    if (m_Performance.collected >= 32) {
        m_Performance.item_skill = Math16_Wrap(m_Performance.item_skill + 1);
        m_Performance.collected -= 32;
    }
}

void Performance_Drop(void)
{
    m_Performance.dropped++;
    if ((m_Performance.dropped & 15) == 0) {
        m_Performance.item_skill = Math16_Wrap(m_Performance.item_skill - 1);
    }
}

int16_t Performance_Skill(const int8_t rank, const uint16_t continues)
{
    int32_t combat =
        50 - m_Performance.total_misses * 3 - m_Performance.total_bombs;
    if (combat < 0) {
        combat = 0;
    }
    if (m_Performance.item_skill > 25) {
        m_Performance.item_skill = 25;
    } else if (m_Performance.item_skill < 0) {
        m_Performance.item_skill = 0;
    }
    return Math16_Wrap(
        rank * 20 + 10 - (int32_t)continues * 3 + combat
        + m_Performance.item_skill);
}

void Performance_BeginDemo(void)
{
    m_Performance.value = 12;
}

void Performance_CountBomb(void)
{
    m_Performance.total_bombs++;
}

void Performance_CountMiss(void)
{
    m_Performance.total_misses++;
}
