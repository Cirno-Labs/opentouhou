#include "pc98/zun/math16.h"

#include "test.h"

TEST(vector_uses_original_byte_angles_and_signed_lengths)
{
    const MATH16_VECTOR vector = Math16_Vector(16, 255);
    CHECK_EQ_INT(vector.x, 16);
    CHECK_EQ_INT(vector.y, -1);
}

TEST(aim_wraps_pixel_differences_and_angle_offset)
{
    const MATH16_VECTOR vector = Math16_Aim(
        (MATH16_VECTOR) { INT16_MAX, 0 }, (MATH16_VECTOR) { INT16_MIN, 0 }, 16,
        64);
    CHECK_EQ_INT(vector.x, 0);
    CHECK_EQ_INT(vector.y, 16);
}
