#include "th06/angle.h"

#include "test.h"

#include "core/math/x87.h"

TEST(add_normalize_keeps_angles_in_range)
{
    CHECK(Angle_AddNormalize(1.0f, 2.0f) == 3.0f);
    CHECK(Angle_AddNormalize(-1.0f, -2.0f) == -3.0f);
}

TEST(add_normalize_turns_into_range)
{
    CHECK(Angle_AddNormalize(3.0f, 1.0f) == 4.0f - 6.2831855f);
    CHECK(Angle_AddNormalize(-3.0f, -1.0f) == -4.0f + 6.2831855f);
}

TEST(add_normalize_stops_after_eighteen_turns)
{
    float expected = 200.0f;
    for (int i = 0; i < 18; i++) {
        expected -= 6.2831855f;
    }
    CHECK(Angle_AddNormalize(200.0f, 0.0f) == expected);
    CHECK(expected > 3.1415927f);
}

TEST(rotate_2d_keeps_z)
{
    const VEC3 in = { 1.0f, 2.0f, 3.0f };
    VEC3 out = { 0.0f, 0.0f, 7.0f };
    Angle_Rotate2D(&out, &in, 0.5f);
    const float s = X87_Sin(0.5f);
    const float c = X87_Cos(0.5f);
    CHECK(out.x == c * 1.0f + s * 2.0f);
    CHECK(out.y == c * 2.0f - s * 1.0f);
    CHECK(out.z == 7.0f);
}

TEST(rotate_2d_in_place_uses_the_new_x)
{
    VEC3 v = { 1.0f, 2.0f, 0.0f };
    Angle_Rotate2D(&v, &v, 0.5f);
    const float s = X87_Sin(0.5f);
    const float c = X87_Cos(0.5f);
    const float x = c * 1.0f + s * 2.0f;
    CHECK(v.x == x);
    CHECK(v.y == c * 2.0f - s * x);
}
