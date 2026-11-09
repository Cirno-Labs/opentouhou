#include "stg/rng.h"

#include "test.h"

#include "core/base/utils.h"

#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t seed;
    uint16_t u16s[3];
} U16_CASE;

TEST(u16_matches_the_reference_sequence)
{
    static const U16_CASE cases[] = {
        { 0x0000, { 0xc374, 0xbfc7, 0x1293 } },
        { 0x0001, { 0xc378, 0xbfd7, 0x1253 } },
        { 0x1234, { 0x7ac4, 0x1e86, 0x8d8c } },
        { 0xffff, { 0x11f0, 0x89b4, 0xe8c6 } },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        RNG rng;
        Rng_SetSeed(&rng, cases[i].seed);
        for (int j = 0; j < 3; j++) {
            CHECK_EQ_INT(Rng_U16(&rng), cases[i].u16s[j]);
            CHECK_EQ_INT(rng.generated, j + 1);
        }
    }
}

TEST(u32_takes_the_high_half_first)
{
    RNG rng;
    Rng_SetSeed(&rng, 0x1234);
    CHECK_EQ_INT(Rng_U32(&rng), 0x7ac41e86u);
    CHECK_EQ_INT(rng.generated, 2);
}

typedef struct {
    uint16_t seed;
    uint32_t bits;
} F32_CASE;

TEST(f32_divides_the_rounded_u32_by_2_32)
{
    static const F32_CASE cases[] = {
        { 0x0000, 0x3f4374c0 },
        { 0x0001, 0x3f4378c0 },
        { 0x1234, 0x3ef5883d },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        RNG rng;
        Rng_SetSeed(&rng, cases[i].seed);
        const float f = Rng_F32(&rng);
        uint32_t bits;
        memcpy(&bits, &f, sizeof(bits));
        CHECK_EQ_INT(bits, cases[i].bits);
    }
}

TEST(f32_in_range_scales_f32)
{
    RNG rng;
    Rng_SetSeed(&rng, 0x1234);
    RNG reference;
    Rng_SetSeed(&reference, 0x1234);
    CHECK(Rng_F32InRange(&rng, 7.0f) == Rng_F32(&reference) * 7.0f);
}

TEST(u16_in_range_is_modulo)
{
    RNG rng;
    Rng_SetSeed(&rng, 0x1234);
    CHECK_EQ_INT(Rng_U16InRange(&rng, 7), 5);
    CHECK_EQ_INT(rng.generated, 1);
}

TEST(u16_in_range_zero_does_not_advance)
{
    RNG rng;
    Rng_SetSeed(&rng, 0x1234);
    CHECK_EQ_INT(Rng_U16InRange(&rng, 0), 0);
    CHECK_EQ_INT(rng.generated, 0);
}

TEST(u32_in_range_is_modulo)
{
    RNG rng;
    Rng_SetSeed(&rng, 5);
    CHECK_EQ_INT(Rng_U32InRange(&rng, 100), 75);
    CHECK_EQ_INT(rng.generated, 2);
}

TEST(u32_in_range_zero_does_not_advance)
{
    RNG rng;
    Rng_SetSeed(&rng, 5);
    CHECK_EQ_INT(Rng_U32InRange(&rng, 0), 0);
    CHECK_EQ_INT(rng.generated, 0);
}
