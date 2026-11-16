#include "th01/finite.h"

#include "test.h"

#include <string.h>

static uint64_t M_Bits(const double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

TEST(binary64_round_trip_includes_signed_zero_and_subnormals)
{
    static const double values[] = { 0.0,
                                     -0.0,
                                     1.0,
                                     -16.0,
                                     0x1p-1074,
                                     0x1.fffffffffffffp-1023,
                                     0x1.fffffffffffffp40,
                                     -0x1p-80 };
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); i++) {
        CHECK(
            M_Bits(Finite_ToDouble(Finite_FromDouble(values[i])))
            == M_Bits(values[i]));
    }
}

TEST(extended_addition_keeps_integer_conversion_before_binary64_store)
{
    const FINITE sum = Finite_Add(
        Finite_FromDouble(-0x1.0000000000001p0), Finite_FromDouble(5.0));
    CHECK(Finite_ToDouble(sum) == 4.0);
    CHECK_EQ_INT(Finite_ToPixel(sum), 3);
    CHECK_EQ_INT(Finite_ToPixel(Finite_Negate(sum)), -3);
    CHECK_EQ_INT(Finite_ToPixel(Finite_FromDouble(65537.75)), 1);
    CHECK_EQ_INT(Finite_ToPixel(Finite_FromDouble(-32769.75)), 32767);
}

TEST(extended_rounding_uses_nearest_even_at_both_boundaries)
{
    const FINITE one = Finite_FromDouble(1.0);
    FINITE sum = Finite_Add(one, Finite_FromDouble(0x1p-64));
    CHECK(sum.significand == one.significand);
    sum = Finite_Add(one, Finite_FromDouble(0x1.0000000000001p-64));
    CHECK(sum.significand == one.significand + 1);
    CHECK(Finite_ToDouble(sum) == 1.0);
    sum = Finite_Add(
        Finite_FromDouble(0x1.0000000000001p0), Finite_FromDouble(0x1p-53));
    CHECK(Finite_ToDouble(sum) == 0x1.0000000000002p0);
    sum = Finite_Add(one, Finite_FromDouble(-1.0));
    CHECK_EQ_INT(sum.significand, 0);
    CHECK(!sum.negative);
}

TEST(multiplication_rounds_before_a_binary64_store)
{
    const FINITE product = Finite_Multiply(
        Finite_FromDouble(-0x1.3fd7025f9f050p1), Finite_FromDouble(0.78));
    CHECK(Finite_ToDouble(product) == -0x1.f2f34124876e4p0);
    CHECK(
        Finite_ToDouble(
            Finite_Multiply(Finite_FromDouble(3.0), Finite_FromDouble(-0.5)))
        == -1.5);
    CHECK(
        M_Bits(Finite_ToDouble(
            Finite_Multiply(Finite_FromDouble(-0.0), Finite_FromDouble(2.0))))
        == M_Bits(-0.0));
}

TEST(half_preserves_values_below_the_binary64_subnormal_range)
{
    const FINITE tiny = Finite_Half(Finite_FromDouble(0x1p-1074));
    CHECK(Finite_ToDouble(tiny) == 0.0);
    CHECK(Finite_ToDouble(Finite_Add(tiny, tiny)) == 0x1p-1074);
    const FINITE almost = Finite_Add(Finite_FromDouble(0x1p-1074), tiny);
    CHECK(Finite_ToDouble(almost) == 0x1p-1073);
}
