#include "tests/test.h"

#include "core/base/result.h"
#include "core/io/reader.h"
#include "stg/wave.h"

#include <math.h>
#include <string.h>

static void M_PutU32(uint8_t *const out, const uint32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        out[i] = (uint8_t)(value >> (8 * i));
    }
}

// Builds a wave file of chunks of zeros whose RIFF size is riff_adjust
// bytes more than the file holds.
static size_t M_Build(
    uint8_t *const out, const char *const ids[], const uint32_t sizes[],
    const int32_t count, const int32_t riff_adjust)
{
    memcpy(out, "RIFF", 4);
    memcpy(out + 8, "WAVE", 4);
    size_t position = 12;
    for (int32_t i = 0; i < count; i++) {
        memcpy(out + position, ids[i], 4);
        M_PutU32(out + position + 4, sizes[i]);
        memset(out + position + 8, 0, sizes[i]);
        position += 8 + sizes[i];
    }
    M_PutU32(out + 4, (uint32_t)((int64_t)position - 8 + riff_adjust));
    return position;
}

static bool M_Check(const uint8_t *const data, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    const RESULT result = Wave_Check(&reader, "test.wav");
    const bool ok = Result_Ignore(result);
    if (ok) {
        CHECK_EQ_INT(Reader_Tell(&reader), 0);
    }
    Reader_Close(&reader);
    return ok;
}

TEST(check_accepts_format_and_data)
{
    uint8_t data[64];
    const char *const ids[] = { "fmt ", "data" };
    const uint32_t sizes[] = { 16, 4 };
    CHECK(M_Check(data, M_Build(data, ids, sizes, 2, 0)));
}

TEST(check_rejects_a_missing_chunk)
{
    uint8_t data[64];
    const char *const ids[] = { "fmt ", "LIST" };
    const uint32_t sizes[] = { 16, 4 };
    CHECK(!M_Check(data, M_Build(data, ids, sizes, 2, 0)));
}

TEST(check_rejects_a_file_that_is_not_riff)
{
    uint8_t data[64];
    const char *const ids[] = { "fmt ", "data" };
    const uint32_t sizes[] = { 16, 4 };
    const size_t size = M_Build(data, ids, sizes, 2, 0);
    memcpy(data + 8, "AVI ", 4);
    CHECK(!M_Check(data, size));
}

TEST(check_misses_an_empty_last_chunk)
{
    uint8_t data[64];
    const char *const ids[] = { "fmt ", "data" };
    const uint32_t sizes[] = { 16, 0 };
    CHECK(!M_Check(data, M_Build(data, ids, sizes, 2, 0)));
}

TEST(check_searches_to_the_end_when_sizes_overrun_the_limit)
{
    uint8_t data[64];
    const char *const ids[] = { "fmt ", "data" };
    const uint32_t sizes[] = { 16, 0 };
    CHECK(M_Check(data, M_Build(data, ids, sizes, 2, 4)));
}

TEST(gain_converts_hundredths_of_a_decibel)
{
    CHECK(Wave_Gain(0) == 1.0f);
    CHECK(fabsf(Wave_Gain(-2000) - 0.1f) < 1e-7f);
}
