// SPDX-License-Identifier: GPL-3.0-only

#include "core/image.h"

#include "test.h"

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
    Reader_OpenBuffer(&reader, data, sizeof(data));
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
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    READER reader;
    Reader_OpenBuffer(&reader, data, size);
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
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Image_EncodeJpeg(&image, 85, &writer);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    CHECK(size >= 2 && data[0] == 0xff && data[1] == 0xd8);

    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    IMAGE decoded;
    const RESULT result = Image_Decode(&reader, &g_HeapAllocator, &decoded);
    CHECK(IS_OK(result));
    CHECK_EQ_INT(decoded.width, image.width);
    CHECK_EQ_INT(decoded.height, image.height);

    Mem_Free(data);
    Image_Free(&image);
    Image_Free(&decoded);
}
