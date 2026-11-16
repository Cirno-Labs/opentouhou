#include "th01/math.h"

#include "test.h"

TEST(random_matches_the_fixed_sequence_and_wrapping_state)
{
    static const uint32_t states[] = { 0x015a4e36, 0x8082a52f, 0xaae684bc,
                                       0x0442c2ed, 0xad889112, 0x9bcd84bb,
                                       0x44bb74b8, 0x190f3a19, 0x59a4a52e,
                                       0xf9963687, 0x232c6bf4, 0xb8deb185 };
    static const uint16_t values[] = {
        346,   130,  10982, 1090,  11656, 7117,
        17595, 6415, 22948, 31126, 9004,  14558
    };
    uint32_t state = 1;
    for (size_t i = 0; i < sizeof(states) / sizeof(*states); i++) {
        CHECK_EQ_INT(Math_Random(&state), values[i]);
        CHECK_EQ_INT(state, states[i]);
    }
    state = 0;
    CHECK_EQ_INT(Math_Random(&state), 0);
    CHECK_EQ_INT(state, 1);
    state = UINT32_MAX;
    CHECK_EQ_INT(Math_Random(&state), 32421);
    CHECK_EQ_INT(state, UINT32_C(0xfea5b1cc));
}

TEST(random_state_is_independent_and_can_be_restored)
{
    uint32_t first = 1;
    uint32_t second = 0;
    uint32_t restored = 1;
    for (int32_t i = 0; i < 10000; i++) {
        const uint16_t next = Math_Random(&first);
        CHECK_EQ_INT(next, Math_Random(&restored));
        CHECK(next <= 32767);
    }
    CHECK_EQ_INT(first, restored);
    CHECK_EQ_INT(second, 0);
    CHECK_EQ_INT(Math_Random(&second), 0);
    CHECK_EQ_INT(Math_Random(&second), 346);
}

TEST(sine_and_cosine_use_the_stock_integer_angles)
{
    static const struct {
        int32_t angle;
        int16_t sine, cosine;
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
        CHECK_EQ_INT(Math_Sin8(cases[i].angle), cases[i].sine);
        CHECK_EQ_INT(Math_Cos8(cases[i].angle), cases[i].cosine);
    }
    for (int32_t i = -1024; i < 1024; i++) {
        CHECK_EQ_INT(Math_Sin8(i + 256), Math_Sin8(i));
        CHECK_EQ_INT(Math_Cos8(i), Math_Sin8(i + 64));
        CHECK_EQ_INT(Math_Sin8(i + 128), -Math_Sin8(i));
    }
}

TEST(polar_rounds_negative_products_down_and_wave_truncates)
{
    CHECK_EQ_INT(Math_PolarY(0, 1, 1), 0);
    CHECK_EQ_INT(Math_PolarY(0, 1, -1), -1);
    CHECK_EQ_INT(Math_WaveX(1, -1), 0);
    CHECK_EQ_INT(Math_PolarY(0, 100, -1), -3);
    CHECK_EQ_INT(Math_WaveX(100, -1), -2);
    CHECK_EQ_INT(Math_PolarY(100, -100, 1), 97);
    CHECK_EQ_INT(Math_WaveX(-100, 1), -2);
    CHECK_EQ_INT(Math_PolarX(100, 3, 32), 102);
    CHECK_EQ_INT(Math_PolarY(-100, 3, 224), -103);
    CHECK_EQ_INT(Math_PolarX(32767, 1, 0), -32768);
    CHECK_EQ_INT(Math_PolarY(-32768, -1, 64), 32767);
    CHECK_EQ_INT(Math_PolarX(0, INT16_MIN, 0), INT16_MIN);
    CHECK_EQ_INT(Math_PolarY(0, INT16_MIN, 192), INT16_MIN);
    CHECK_EQ_INT(Math_WaveX(INT16_MIN, 64), INT16_MIN);
    CHECK_EQ_INT(Math_WaveX(INT16_MIN, 192), INT16_MIN);
}

TEST(angle_preserves_axes_quadrants_and_lookup_quantization)
{
    static const struct {
        int16_t y, x;
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
        CHECK_EQ_INT(Math_Angle(cases[i].y, cases[i].x), cases[i].angle);
    }
    for (int16_t i = 1; i < 256; i++) {
        const uint8_t angle = Math_Angle(i, 256);
        CHECK_EQ_INT(Math_Angle(-i, 256), (uint8_t)-angle);
        CHECK_EQ_INT(Math_Angle(i, -256), 128 - angle);
        CHECK_EQ_INT(Math_Angle(-i, -256), 128 + angle);
        CHECK_EQ_INT(Math_Angle(256, i), 64 - angle);
        CHECK_EQ_INT(Math_Angle(i * 100, 25600), angle);
    }
}

TEST(angle_defines_unsigned_magnitudes_at_the_minimum_coordinate)
{
    CHECK_EQ_INT(Math_Angle(INT16_MIN, 0), 192);
    CHECK_EQ_INT(Math_Angle(0, INT16_MIN), 128);
    CHECK_EQ_INT(Math_Angle(INT16_MIN, INT16_MIN), 160);
    CHECK_EQ_INT(Math_Angle(INT16_MIN, INT16_MAX), 224);
    CHECK_EQ_INT(Math_Angle(INT16_MAX, INT16_MIN), 96);
    CHECK_EQ_INT(Math_Angle(1, INT16_MIN), 128);
    CHECK_EQ_INT(Math_Angle(INT16_MIN, 1), 192);
}

TEST(vectors_use_lookup_angles_and_wrapped_coordinate_differences)
{
    MATH_VECTOR vector = Math_Vector(100, 32);
    CHECK_EQ_INT(vector.x, 70);
    CHECK_EQ_INT(vector.y, 70);
    vector = Math_Vector(100, 224);
    CHECK_EQ_INT(vector.x, 70);
    CHECK_EQ_INT(vector.y, -71);
    vector = Math_VectorBetween(
        (MATH_VECTOR) { 20, 30 }, (MATH_VECTOR) { 120, 30 }, 50);
    CHECK_EQ_INT(vector.x, 50);
    CHECK_EQ_INT(vector.y, 0);
    vector = Math_VectorBetween(
        (MATH_VECTOR) { 32767, 0 }, (MATH_VECTOR) { -32768, 0 }, 7);
    CHECK_EQ_INT(vector.x, 7);
    CHECK_EQ_INT(vector.y, 0);
    vector = Math_VectorBetween(
        (MATH_VECTOR) { 0, 32767 }, (MATH_VECTOR) { 0, -32768 }, 7);
    CHECK_EQ_INT(vector.x, 0);
    CHECK_EQ_INT(vector.y, 7);
    vector =
        Math_VectorBetween((MATH_VECTOR) { 7, 9 }, (MATH_VECTOR) { 7, 9 }, 7);
    CHECK_EQ_INT(vector.x, 7);
    CHECK_EQ_INT(vector.y, 0);
}

TEST(square_root_rounds_at_each_integer_boundary)
{
    CHECK_EQ_INT(Math_Sqrt(0), 0);
    for (int32_t root = 1; root <= 32767; root++) {
        const int32_t square = root * root;
        CHECK_EQ_INT(Math_Sqrt(square), root);
        CHECK_EQ_INT(Math_Sqrt(square - root), root - 1);
        CHECK_EQ_INT(Math_Sqrt(square - root + 1), root);
    }
    CHECK_EQ_INT(Math_Sqrt(2), 1);
    CHECK_EQ_INT(Math_Sqrt(3), 2);
    CHECK_EQ_INT(Math_Sqrt(6), 2);
    CHECK_EQ_INT(Math_Sqrt(7), 3);
    CHECK_EQ_INT(Math_Sqrt(0x3fff8000), 32767);
}

TEST(square_root_and_hypot_keep_the_original_overflow_limit)
{
    CHECK_EQ_INT(Math_Sqrt(-1), 0);
    CHECK_EQ_INT(Math_Sqrt(INT32_MIN), 0);
    CHECK_EQ_INT(Math_Sqrt(INT32_MAX), 0);
    CHECK_EQ_INT(Math_Sqrt(0x3fff8001), 0);
    CHECK_EQ_INT(Math_Hypot(0, 0), 0);
    CHECK_EQ_INT(Math_Hypot(1, 1), 1);
    CHECK_EQ_INT(Math_Hypot(2, 2), 3);
    CHECK_EQ_INT(Math_Hypot(-3, 4), 5);
    CHECK_EQ_INT(Math_Hypot(20000, -20000), 28284);
    CHECK_EQ_INT(Math_Hypot(32767, 0), 32767);
    CHECK_EQ_INT(Math_Hypot(32767, 32767), 0);
    CHECK_EQ_INT(Math_Hypot(INT16_MIN, 0), 0);
    CHECK_EQ_INT(Math_Hypot(INT16_MIN, INT16_MIN), 0);
}

TEST(signed_word_wrap_is_defined_for_all_input_bits)
{
    CHECK_EQ_INT(Math_Wrap16(INT32_MIN), 0);
    CHECK_EQ_INT(Math_Wrap16(INT32_MAX), -1);
    CHECK_EQ_INT(Math_Wrap16(32768), -32768);
    CHECK_EQ_INT(Math_Wrap16(-32769), 32767);
    CHECK_EQ_INT(Math_Wrap16(65535), -1);
    CHECK_EQ_INT(Math_Wrap16(-65537), -1);
    for (int32_t value = -32768; value <= 32767; value++) {
        CHECK_EQ_INT(Math_Wrap16(value), value);
        CHECK_EQ_INT(Math_Wrap16(value + 65536), value);
        CHECK_EQ_INT(Math_Wrap16(value - 65536), value);
    }
}

TEST(subpixel_conversions_preserve_negative_rounding_and_word_wrap)
{
    CHECK_EQ_INT(Math_SubpixelFromPixel(-1), -16);
    CHECK_EQ_INT(Math_SubpixelFromPixel(-2048), -32768);
    CHECK_EQ_INT(Math_SubpixelFromPixel(2048), -32768);
    CHECK_EQ_INT(Math_SubpixelFromPixel(4096), 0);
    CHECK_EQ_INT(Math_SubpixelToPixel(-1), -1);
    CHECK_EQ_INT(Math_SubpixelToPixel(-15), -1);
    CHECK_EQ_INT(Math_SubpixelToPixel(-16), -1);
    CHECK_EQ_INT(Math_SubpixelToPixel(-17), -2);
    int32_t pixel = -2048;
    for (int32_t value = -32768; value <= 32767; value++) {
        CHECK_EQ_INT(Math_SubpixelToPixel((int16_t)value), pixel);
        if ((value + 1) % 16 == 0) {
            pixel++;
        }
    }
    for (int32_t value = -2048; value < 2048; value++) {
        CHECK_EQ_INT(Math_SubpixelFromPixel((int16_t)value), value * 16);
        CHECK_EQ_INT(
            Math_SubpixelToPixel(Math_SubpixelFromPixel((int16_t)value)),
            value);
    }
}
