#include "pc98/zun/math16.h"

#include "test.h"

TEST(sine_and_cosine_use_the_stock_integer_angles)
{
    static const struct {
        int32_t angle;
        int16_t sine;
        int16_t cosine;
    } cases[] = {
        { 0, 0, 256 },         { 1, 6, 256 },         { 2, 13, 256 },
        { 16, 98, 237 },       { 32, 181, 181 },      { 48, 237, 98 },
        { 63, 256, 6 },        { 64, 256, 0 },        { 65, 256, -6 },
        { 96, 181, -181 },     { 128, 0, -256 },      { 160, -181, -181 },
        { 192, -256, 0 },      { 224, -181, 181 },    { 255, -6, 256 },
        { -1, -6, 256 },       { -64, -256, 0 },      { 256, 0, 256 },
        { INT32_MIN, 0, 256 }, { INT32_MAX, -6, 256 }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        CHECK_EQ_INT(Math16_Sin(cases[i].angle), cases[i].sine);
        CHECK_EQ_INT(Math16_Cos(cases[i].angle), cases[i].cosine);
    }
    for (int32_t i = -1024; i < 1024; i++) {
        CHECK_EQ_INT(Math16_Sin(i + 256), Math16_Sin(i));
        CHECK_EQ_INT(Math16_Cos(i), Math16_Sin(i + 64));
        CHECK_EQ_INT(Math16_Sin(i + 128), -Math16_Sin(i));
    }
}

TEST(polar_rounds_negative_products_down_and_wave_truncates)
{
    CHECK_EQ_INT(Math16_PolarY(0, 1, 1), 0);
    CHECK_EQ_INT(Math16_PolarY(0, 1, -1), -1);
    CHECK_EQ_INT(Math16_Wave(1, -1), 0);
    CHECK_EQ_INT(Math16_PolarY(0, 100, -1), -3);
    CHECK_EQ_INT(Math16_Wave(100, -1), -2);
    CHECK_EQ_INT(Math16_PolarY(100, -100, 1), 97);
    CHECK_EQ_INT(Math16_Wave(-100, 1), -2);
    CHECK_EQ_INT(Math16_PolarX(100, 3, 32), 102);
    CHECK_EQ_INT(Math16_PolarY(-100, 3, 224), -103);
    CHECK_EQ_INT(Math16_PolarX(32767, 1, 0), -32768);
    CHECK_EQ_INT(Math16_PolarY(-32768, -1, 64), 32767);
    CHECK_EQ_INT(Math16_PolarX(0, INT16_MIN, 0), INT16_MIN);
    CHECK_EQ_INT(Math16_PolarY(0, INT16_MIN, 192), INT16_MIN);
    CHECK_EQ_INT(Math16_Wave(INT16_MIN, 64), INT16_MIN);
    CHECK_EQ_INT(Math16_Wave(INT16_MIN, 192), INT16_MIN);
}

TEST(angle_preserves_axes_quadrants_and_lookup_quantization)
{
    static const struct {
        int16_t y;
        int16_t x;
        uint8_t angle;
    } cases[] = {
        { 0, 0, 0 },         { 0, 1, 0 },          { 1, 0, 64 },
        { 0, -1, 128 },      { -1, 0, 192 },       { 1, 1, 32 },
        { 1, -1, 96 },       { -1, -1, 160 },      { -1, 1, 224 },
        { 4, 256, 1 },       { 16, 256, 3 },       { 128, 256, 19 },
        { 255, 256, 32 },    { 256, 128, 45 },     { 128, -256, 109 },
        { -128, -256, 147 }, { -256, 128, 211 },   { 1, 32767, 0 },
        { 32767, 1, 64 },    { 32767, 32767, 32 }, { -32767, -32767, 160 },
        { 1, 103, 0 },       { 1, 64, 1 }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        CHECK_EQ_INT(Math16_Angle(cases[i].y, cases[i].x), cases[i].angle);
    }
    for (int16_t i = 1; i < 256; i++) {
        const uint8_t angle = Math16_Angle(i, 256);
        CHECK_EQ_INT(Math16_Angle(-i, 256), (uint8_t)-angle);
        CHECK_EQ_INT(Math16_Angle(i, -256), 128 - angle);
        CHECK_EQ_INT(Math16_Angle(-i, -256), 128 + angle);
        CHECK_EQ_INT(Math16_Angle(256, i), 64 - angle);
        CHECK_EQ_INT(Math16_Angle(i * 100, 25600), angle);
    }
}

TEST(angle_defines_unsigned_magnitudes_at_the_minimum_coordinate)
{
    CHECK_EQ_INT(Math16_Angle(INT16_MIN, 0), 192);
    CHECK_EQ_INT(Math16_Angle(0, INT16_MIN), 128);
    CHECK_EQ_INT(Math16_Angle(INT16_MIN, INT16_MIN), 160);
    CHECK_EQ_INT(Math16_Angle(INT16_MIN, INT16_MAX), 224);
    CHECK_EQ_INT(Math16_Angle(INT16_MAX, INT16_MIN), 96);
    CHECK_EQ_INT(Math16_Angle(1, INT16_MIN), 128);
    CHECK_EQ_INT(Math16_Angle(INT16_MIN, 1), 192);
}

TEST(vectors_use_lookup_angles_and_wrapped_coordinate_differences)
{
    MATH16_VECTOR vector = Math16_Vector(100, 32);
    CHECK_EQ_INT(vector.x, 70);
    CHECK_EQ_INT(vector.y, 70);
    vector = Math16_Vector(100, 224);
    CHECK_EQ_INT(vector.x, 70);
    CHECK_EQ_INT(vector.y, -71);
    vector = Math16_Aim(
        (MATH16_VECTOR) { 20, 30 }, (MATH16_VECTOR) { 120, 30 }, 50, 0);
    CHECK_EQ_INT(vector.x, 50);
    CHECK_EQ_INT(vector.y, 0);
    vector = Math16_Aim(
        (MATH16_VECTOR) { 32767, 0 }, (MATH16_VECTOR) { -32768, 0 }, 7, 0);
    CHECK_EQ_INT(vector.x, 7);
    CHECK_EQ_INT(vector.y, 0);
    vector = Math16_Aim(
        (MATH16_VECTOR) { 0, 32767 }, (MATH16_VECTOR) { 0, -32768 }, 7, 0);
    CHECK_EQ_INT(vector.x, 0);
    CHECK_EQ_INT(vector.y, 7);
    vector =
        Math16_Aim((MATH16_VECTOR) { 7, 9 }, (MATH16_VECTOR) { 7, 9 }, 7, 0);
    CHECK_EQ_INT(vector.x, 7);
    CHECK_EQ_INT(vector.y, 0);
    vector = Math16_Aim(
        (MATH16_VECTOR) { 20, 30 }, (MATH16_VECTOR) { 120, 30 }, 50, 64);
    CHECK_EQ_INT(vector.x, 0);
    CHECK_EQ_INT(vector.y, 50);
    vector = Math16_Aim(
        (MATH16_VECTOR) { 20, 30 }, (MATH16_VECTOR) { -80, 30 }, 50, 128);
    CHECK_EQ_INT(vector.x, 50);
    CHECK_EQ_INT(vector.y, 0);
}

TEST(square_root_rounds_at_each_integer_boundary)
{
    CHECK_EQ_INT(Math16_Sqrt(0), 0);
    for (int32_t root = 1; root <= 32767; root++) {
        const int32_t square = root * root;
        CHECK_EQ_INT(Math16_Sqrt(square), root);
        CHECK_EQ_INT(Math16_Sqrt(square - root), root - 1);
        CHECK_EQ_INT(Math16_Sqrt(square - root + 1), root);
    }
    CHECK_EQ_INT(Math16_Sqrt(2), 1);
    CHECK_EQ_INT(Math16_Sqrt(3), 2);
    CHECK_EQ_INT(Math16_Sqrt(6), 2);
    CHECK_EQ_INT(Math16_Sqrt(7), 3);
    CHECK_EQ_INT(Math16_Sqrt(0x3fff8000), 32767);
}

TEST(square_root_and_hypot_keep_the_original_overflow_limit)
{
    CHECK_EQ_INT(Math16_Sqrt(-1), 0);
    CHECK_EQ_INT(Math16_Sqrt(INT32_MIN), 0);
    CHECK_EQ_INT(Math16_Sqrt(INT32_MAX), 0);
    CHECK_EQ_INT(Math16_Sqrt(0x3fff8001), 0);
    CHECK_EQ_INT(Math16_Hypot(0, 0), 0);
    CHECK_EQ_INT(Math16_Hypot(1, 1), 1);
    CHECK_EQ_INT(Math16_Hypot(2, 2), 3);
    CHECK_EQ_INT(Math16_Hypot(-3, 4), 5);
    CHECK_EQ_INT(Math16_Hypot(20000, -20000), 28284);
    CHECK_EQ_INT(Math16_Hypot(32767, 0), 32767);
    CHECK_EQ_INT(Math16_Hypot(32767, 32767), 0);
    CHECK_EQ_INT(Math16_Hypot(INT16_MIN, 0), 0);
    CHECK_EQ_INT(Math16_Hypot(INT16_MIN, INT16_MIN), 0);
}

TEST(signed_word_wrap_is_defined_for_all_input_bits)
{
    CHECK_EQ_INT(Math16_Wrap(INT32_MIN), 0);
    CHECK_EQ_INT(Math16_Wrap(INT32_MAX), -1);
    CHECK_EQ_INT(Math16_Wrap(32768), -32768);
    CHECK_EQ_INT(Math16_Wrap(-32769), 32767);
    CHECK_EQ_INT(Math16_Wrap(65535), -1);
    CHECK_EQ_INT(Math16_Wrap(-65537), -1);
    for (int32_t value = -32768; value <= 32767; value++) {
        CHECK_EQ_INT(Math16_Wrap(value), value);
        CHECK_EQ_INT(Math16_Wrap(value + 65536), value);
        CHECK_EQ_INT(Math16_Wrap(value - 65536), value);
    }
}

TEST(subpixel_conversions_preserve_negative_rounding_and_word_wrap)
{
    CHECK_EQ_INT(Math16_FromPixel(-1), -16);
    CHECK_EQ_INT(Math16_FromPixel(-2048), -32768);
    CHECK_EQ_INT(Math16_FromPixel(2048), -32768);
    CHECK_EQ_INT(Math16_FromPixel(4096), 0);
    CHECK_EQ_INT(Math16_ToPixel(-1), -1);
    CHECK_EQ_INT(Math16_ToPixel(-15), -1);
    CHECK_EQ_INT(Math16_ToPixel(-16), -1);
    CHECK_EQ_INT(Math16_ToPixel(-17), -2);
    int32_t pixel = -2048;
    for (int32_t value = -32768; value <= 32767; value++) {
        CHECK_EQ_INT(Math16_ToPixel((int16_t)value), pixel);
        if ((value + 1) % 16 == 0) {
            pixel++;
        }
    }
    for (int32_t value = -2048; value < 2048; value++) {
        CHECK_EQ_INT(Math16_FromPixel((int16_t)value), value * 16);
        CHECK_EQ_INT(Math16_ToPixel(Math16_FromPixel((int16_t)value)), value);
    }
}
