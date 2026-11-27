#include "core/math/common.h"

#include "test.h"

#include <math.h>

TEST(abs_clears_the_sign)
{
    CHECK(Math_Abs(-3.5f) == 3.5f);
    CHECK(Math_Abs(3.5f) == 3.5f);
    CHECK(signbit(Math_Abs(-0.0f)) == 0);
}

TEST(min_and_max_pick_by_comparison)
{
    CHECK(Math_Min(2.0f, 5.0f) == 2.0f);
    CHECK(Math_Min(5.0f, 2.0f) == 2.0f);
    CHECK(Math_Max(2.0f, 5.0f) == 5.0f);
    CHECK(Math_Max(5.0f, 2.0f) == 5.0f);
}

TEST(min_and_max_pass_through_a_nan_second_argument)
{
    const float nan = nanf("");
    CHECK(isnan(Math_Min(3.0f, nan)));
    CHECK(isnan(Math_Max(3.0f, nan)));
    CHECK(Math_Min(nan, 3.0f) == 3.0f);
    CHECK(Math_Max(nan, 3.0f) == 3.0f);
}

TEST(clamp_keeps_values_inside_the_range)
{
    CHECK(Math_Clamp(5.0f, 0.0f, 10.0f) == 5.0f);
    CHECK(Math_Clamp(-5.0f, 0.0f, 10.0f) == 0.0f);
    CHECK(Math_Clamp(15.0f, 0.0f, 10.0f) == 10.0f);
}

TEST(lerp_reaches_both_ends_exactly)
{
    CHECK(Math_Lerp(2.0f, 8.0f, 0.0f) == 2.0f);
    CHECK(Math_Lerp(2.0f, 8.0f, 1.0f) == 8.0f);
    CHECK(Math_Lerp(0.0f, 10.0f, 0.5f) == 5.0f);
}

TEST(fmod_keeps_the_sign_of_x)
{
    CHECK(Math_Fmod(5.5f, 2.0f) == 1.5f);
    CHECK(Math_Fmod(-5.5f, 2.0f) == -1.5f);
    CHECK(Math_Fmod(5.5f, -2.0f) == 1.5f);
}
