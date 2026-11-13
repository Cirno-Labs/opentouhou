#include "core/base/crc32.h"

#include "test.h"

#include <string.h>

TEST(compute_of_no_data_is_zero)
{
    CHECK_EQ_INT(Crc32_Compute(nullptr, 0), 0);
}

TEST(compute_matches_the_check_value)
{
    CHECK_EQ_INT(Crc32_Compute("123456789", 9), 0xCBF43926u);
}

TEST(compute_matches_known_text)
{
    const char *const text = "The quick brown fox jumps over the lazy dog";
    CHECK_EQ_INT(Crc32_Compute(text, strlen(text)), 0x414FA339u);
}

TEST(update_in_parts_matches_one_call)
{
    const char *const text = "The quick brown fox jumps over the lazy dog";
    const size_t size = strlen(text);
    for (size_t split = 0; split <= size; split++) {
        const uint32_t first = Crc32_Update(0, text, split);
        CHECK_EQ_INT(
            Crc32_Update(first, text + split, size - split), 0x414FA339u);
    }
}
