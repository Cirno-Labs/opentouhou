// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/crc32.h"

static uint32_t m_Table[256];
static bool m_TableReady;

static void M_BuildTable(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t value = i;
        for (int bit = 0; bit < 8; bit++) {
            value = (value & 1) != 0 ? (value >> 1) ^ 0xEDB88320u : value >> 1;
        }
        m_Table[i] = value;
    }
    m_TableReady = true;
}

uint32_t Crc32_Update(const uint32_t crc, const void *const data, size_t size)
{
    if (!m_TableReady) {
        M_BuildTable();
    }
    const uint8_t *bytes = data;
    uint32_t value = ~crc;
    while (size-- > 0) {
        value = m_Table[(value ^ *bytes++) & 0xFF] ^ (value >> 8);
    }
    return ~value;
}

uint32_t Crc32_Compute(const void *const data, const size_t size)
{
    return Crc32_Update(0, data, size);
}
