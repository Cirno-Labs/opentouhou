#include "tests/jpeg_fixtures.h"
#include "tests/test.h"

#include "core/hash.h"
#include "core/image.h"

#include <string.h>

static IMAGE M_MakeGradient(const int32_t width, const int32_t height)
{
    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            uint8_t *const pixel = &image.pixels[(y * width + x) * 4];
            pixel[0] = (uint8_t)(x * 40);
            pixel[1] = (uint8_t)(y * 40);
            pixel[2] = 128;
            pixel[3] = 255;
        }
    }
    return image;
}

TEST(decode_rejects_unrecognized_data)
{
    const uint8_t data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    READER reader;
    Reader_OpenBuffer(&reader, data, sizeof(data), nullptr);
    IMAGE image;
    const RESULT result = Image_Decode(&reader, &g_HeapAllocator, &image);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK_EQ_INT(image.width, 0);
    CHECK_EQ_INT(image.height, 0);
    Image_Free(&image);
}

TEST(png_round_trip_is_exact)
{
    IMAGE image = M_MakeGradient(6, 5);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    IMAGE decoded;
    const RESULT result = Image_Decode(&reader, &g_HeapAllocator, &decoded);
    CHECK(IS_OK(result));
    CHECK_EQ_INT(decoded.width, image.width);
    CHECK_EQ_INT(decoded.height, image.height);
    if (decoded.width == image.width && decoded.height == image.height) {
        CHECK(memcmp(decoded.pixels, image.pixels, (size_t)6 * 5 * 4) == 0);
    }

    Mem_Free(data);
    Image_Free(&image);
    Image_Free(&decoded);
}

TEST(jpeg_round_trip_decodes)
{
    IMAGE image = M_MakeGradient(8, 8);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Image_EncodeJpeg(&image, 85, &writer);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    CHECK(size >= 2 && data[0] == 0xff && data[1] == 0xd8);

    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    IMAGE decoded;
    const RESULT result = Image_Decode(&reader, &g_HeapAllocator, &decoded);
    CHECK(IS_OK(result));
    CHECK_EQ_INT(decoded.width, image.width);
    CHECK_EQ_INT(decoded.height, image.height);

    Mem_Free(data);
    Image_Free(&image);
    Image_Free(&decoded);
}

TEST(jpeg_pixels_match_direct3d)
{
    const struct {
        const uint8_t *data;
        size_t size;
        int32_t width;
        int32_t height;
        uint32_t crc;
    } cases[] = {
        { m_Jpeg444, sizeof(m_Jpeg444), 32, 32, 0xa4a6167c },
        { m_Jpeg420Tail, sizeof(m_Jpeg420Tail), 37, 29, 0xd2253ec9 },
        { m_JpegGray, sizeof(m_JpegGray), 32, 32, 0xcd55426c },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        READER reader;
        Reader_OpenBuffer(&reader, cases[i].data, cases[i].size, nullptr);
        IMAGE image;
        CHECK(Result_Log(
            Image_Decode(&reader, &g_HeapAllocator, &image), LOG_LEVEL_ERROR));
        CHECK_EQ_INT(image.width, cases[i].width);
        CHECK_EQ_INT(image.height, cases[i].height);
        CHECK_EQ_INT(
            Crc32_Compute(image.pixels, (size_t)image.width * image.height * 4),
            cases[i].crc);
        Reader_Close(&reader);
        Image_Free(&image);
    }
}

TEST(jpeg_errors_leave_an_empty_image)
{
    const uint8_t data[] = { 0xff, 0xd8, 0xff, 0xda, 0x00 };
    READER reader;
    Reader_OpenBuffer(&reader, data, sizeof(data), nullptr);
    IMAGE image;
    const RESULT decoding = Image_Decode(&reader, &g_HeapAllocator, &image);
    CHECK(!IS_OK(decoding));
    Result_Ignore(decoding);
    CHECK_EQ_INT(image.width, 0);
    CHECK_EQ_INT(image.height, 0);
    CHECK(image.pixels == nullptr);
    Image_Free(&image);
    Reader_Close(&reader);
}

TEST(a_one_pixel_jpeg_decodes)
{
    IMAGE image = M_MakeGradient(1, 1);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Image_EncodeJpeg(&image, 85, &writer);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    IMAGE decoded;
    CHECK(Result_Log(
        Image_Decode(&reader, &g_HeapAllocator, &decoded), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded.width, 1);
    CHECK_EQ_INT(decoded.height, 1);
    CHECK_EQ_INT(decoded.pixels[3], 255);
    Reader_Close(&reader);
    Mem_Free(data);
    Image_Free(&image);
    Image_Free(&decoded);
}
