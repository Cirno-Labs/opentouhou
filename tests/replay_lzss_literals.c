#include "replay_lzss_literals.h"

size_t ReplayTestLzss_EncodeLiterals(
    const uint8_t *const data, const size_t size, uint8_t *const out)
{
    uint32_t bit = 0;
    for (size_t i = 0; i < size; i++) {
        const uint32_t value = 0x100 | data[i];
        for (int j = 8; j >= 0; j--) {
            if ((value >> j & 1) != 0) {
                out[bit / 8] |= (uint8_t)(0x80 >> (bit % 8));
            }
            bit++;
        }
    }
    bit += 14;
    return bit / 8;
}
