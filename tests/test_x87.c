#include "core/math/x87.h"

#include "core/math/common.h"
#include "test.h"

#include <math.h>
#include <stdint.h>

TEST(rint_rounds_ties_to_even)
{
    CHECK(X87_Rint(0.5f) == 0.0f);
    CHECK(X87_Rint(1.5f) == 2.0f);
    CHECK(X87_Rint(2.5f) == 2.0f);
    CHECK(X87_Rint(-0.5f) == 0.0f);
    CHECK(signbit(X87_Rint(-0.5f)));
    CHECK(X87_Rint(-1.5f) == -2.0f);
    CHECK(X87_Rint(-2.5f) == -2.0f);
}

TEST(rint_passes_through_values_with_no_fraction)
{
    CHECK(X87_Rint(0.0f) == 0.0f);
    CHECK(X87_Rint(3.0f) == 3.0f);
    CHECK(X87_Rint(8388608.0f) == 8388608.0f); // 2**23, already integral
    CHECK(isnan(X87_Rint(nanf(""))));
    CHECK(isinf(X87_Rint(INFINITY)));
}

TEST(ftol_truncates_toward_zero)
{
    CHECK_EQ_INT(X87_Ftol(3.9f), 3);
    CHECK_EQ_INT(X87_Ftol(-3.9f), -3);
    CHECK_EQ_INT(X87_Ftol(0.0f), 0);
}

TEST(ftol_gives_integer_indefinite_out_of_range)
{
    CHECK_EQ_INT(X87_Ftol(nanf("")), INT32_MIN);
    CHECK_EQ_INT(X87_Ftol(INFINITY), INT32_MIN);
    CHECK_EQ_INT(X87_Ftol(-INFINITY), INT32_MIN);
    CHECK_EQ_INT(X87_Ftol(1e30f), INT32_MIN);
    CHECK_EQ_INT(X87_Ftol(2147483648.0f), INT32_MIN);
}

TEST(sqrt_matches_the_exact_root)
{
    CHECK(X87_Sqrt(4.0f) == 2.0f);
    CHECK(X87_Sqrt(0.0f) == 0.0f);
    CHECK(X87_Sqrt(2.0f) > 1.41421f && X87_Sqrt(2.0f) < 1.41422f);
}

TEST(sin_cos_tan_match_their_single_counterparts)
{
    CHECK(X87_Sin(0.0f) == 0.0f);
    CHECK(X87_Cos(0.0f) == 1.0f);
    CHECK(X87_Tan(0.0f) == 0.0f);
    CHECK(Math_Abs(X87_Sin(1.5707963f) - 1.0f) < 0.0001f);
}

TEST(sincos_matches_sin_and_cos)
{
    float s, c;
    X87_SinCos(0.7f, &s, &c);
    CHECK(s == X87_Sin(0.7f));
    CHECK(c == X87_Cos(0.7f));
}

TEST(sincosscale_scales_both_components)
{
    float x, y;
    X87_SinCosScale(0.0f, 10.0f, &x, &y);
    CHECK(x == 10.0f);
    CHECK(y == 0.0f);
}

TEST(atan2_matches_quadrants)
{
    CHECK(X87_Atan2(0.0f, 1.0f) == 0.0f);
    CHECK(Math_Abs(X87_Atan2(1.0f, 0.0f) - 1.5707963f) < 0.0001f);
}
